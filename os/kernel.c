/* kernel.c - the C part of the kernel */
/* Copyright (C) 1999, 2010  Free Software Foundation, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "multiboot2.h"
#include "stdio.h"
#include "mbi.h"

#include "x86_videomem.h"

#include <hwcore/idt.h>
#include <hwcore/gdt.h>
#include <hwcore/irq.h>
#include <hwcore/exception.h>
#include <hwcore/i8254.h>
#include <hwcore/paging.h>
#include "list.h"
#include "physmem.h"
#include <os/kmem_vmm.h>
#include <os/kmalloc.h>
#include <os/time.h>
#include "os/assert.h"

extern struct multiboot_tag_basic_meminfo* mbi_tag_mem;
extern sos_vaddr_t bootstrap_stack_bottom, bootstrap_stack_size;

/* Helper function to display each bits of a 32bits integer on the
   screen as dark or light carrets */
static void display_bits(unsigned char row, unsigned char col,
			 unsigned char attribute,
			 sos_ui32_t integer)
{
  int i;
  /* Scan each bit of the integer, MSb first */
  for (i = 31 ; i >= 0 ; i--)
    {
      /* Test if bit i of 'integer' is set */
      int bit_i = (integer & (1 << i));
      /* Ascii 219 => dark carret, Ascii 177 => light carret */
      unsigned char ascii_code = bit_i?219:177;
      /*sos_x86_videomem_putchar(row, col++,
			       attribute,
			       ascii_code);*/
      os_putchar (row, col++, attribute, ascii_code);
    }
}


/* Clock IRQ handler */
static void clk_it(int intid)
{
  static sos_ui32_t clock_count = 0;

  display_bits(0, 48,
	       SOS_X86_VIDEO_FG_LTGREEN | SOS_X86_VIDEO_BG_BLUE,
	       clock_count);
  clock_count++;

}



/* ======================================================================
 * Page fault exception handling
 */

/* Helper function to dump a backtrace on bochs and/or the console */
static void dump_backtrace(const struct sos_cpu_state *cpu_state,
			   sos_vaddr_t stack_bottom,
			   sos_size_t  stack_size,
			   sos_bool_t on_console,
			   sos_bool_t on_bochs)
{
  void backtracer(sos_vaddr_t PC,
			 sos_vaddr_t params,
			 sos_ui32_t depth,
			 void *custom_arg)
    {
      sos_ui32_t invalid = 0xffffffff, *arg1, *arg2, *arg3, *arg4;

      /* Get the address of the first 3 arguments from the
	 frame. Among these arguments, 0, 1, 2, 3 arguments might be
	 meaningful (depending on how many arguments the function may
	 take). */
      arg1 = (sos_ui32_t*)params;
      arg2 = (sos_ui32_t*)(params+4);
      arg3 = (sos_ui32_t*)(params+8);
      arg4 = (sos_ui32_t*)(params+12);

      /* Make sure the addresses of these arguments fit inside the
	 stack boundaries */
#define INTERVAL_OK(b,v,u) ( ((b) <= (sos_vaddr_t)(v)) \
                             && ((sos_vaddr_t)(v) < (u)) )
      if (!INTERVAL_OK(stack_bottom, arg1, stack_bottom + stack_size))
	arg1 = &invalid;
      if (!INTERVAL_OK(stack_bottom, arg2, stack_bottom + stack_size))
	arg2 = &invalid;
      if (!INTERVAL_OK(stack_bottom, arg3, stack_bottom + stack_size))
	arg3 = &invalid;
      if (!INTERVAL_OK(stack_bottom, arg4, stack_bottom + stack_size))
	arg4 = &invalid;

      /* Print the function context for this frame */
      if (on_bochs)
	printf("[%d] PC=0x%x arg1=0x%x arg2=0x%x arg3=0x%x\n",
			 (unsigned)depth, (unsigned)PC,
			 (unsigned)*arg1, (unsigned)*arg2,
			 (unsigned)*arg3);

      if (on_console)
	os_printf(		23-depth, 3,
				SOS_X86_VIDEO_BG_BLUE
				  | SOS_X86_VIDEO_FG_LTGREEN,
				"[%d] PC=0x%x arg1=0x%x arg2=0x%x arg3=0x%x arg4=0x%x",
				(unsigned)depth, PC,
				(unsigned)*arg1, (unsigned)*arg2,
				(unsigned)*arg3, (unsigned)*arg4);
      
    }

  sos_backtrace(cpu_state, 15, stack_bottom, stack_size, backtracer, NULL);
}


/* Page fault exception handler with demand paging for the kernel */
static void pgflt_ex(int intid, const struct sos_cpu_state *ctxt)
{
  static sos_ui32_t demand_paging_count = 0;
  sos_vaddr_t faulting_vaddr = sos_cpu_context_get_EX_faulting_vaddr(ctxt);
  sos_paddr_t ppage_paddr;

  /* Check if address is covered by any VMM range */
  if (! sos_kmem_vmm_is_valid_vaddr(faulting_vaddr))
    {
      /* No: The page fault is out of any kernel virtual region. For
	 the moment, we don't handle this. */
      dump_backtrace(ctxt,
		     bootstrap_stack_bottom,
		     bootstrap_stack_size,
		     TRUE, TRUE);
      sos_display_fatal_error("Unresolved page Fault at instruction 0x%x on access to address 0x%x (info=%x)!",
			      sos_cpu_context_get_PC(ctxt),
			      (unsigned)faulting_vaddr,
			      (unsigned)sos_cpu_context_get_EX_info(ctxt));
      SOS_ASSERT_FATAL(! "Got page fault (note: demand paging is disabled)");
    }


  /*
   * Demand paging
   */
 
  /* Update the number of demand paging requests handled */
  demand_paging_count ++;
  display_bits(0, 0,
	       SOS_X86_VIDEO_FG_LTRED | SOS_X86_VIDEO_BG_BLUE,
	       demand_paging_count);

  /* Allocate a new page for the virtual address */
  ppage_paddr = sos_physmem_ref_physpage_new(FALSE);
  if (! ppage_paddr)
    SOS_ASSERT_FATAL(! "TODO: implement swap. (Out of mem in demand paging because no swap for kernel yet !)");
  SOS_ASSERT_FATAL(SOS_OK == sos_paging_map(ppage_paddr,
					    SOS_PAGE_ALIGN_INF(faulting_vaddr),
					    FALSE,
					    SOS_VM_MAP_PROT_READ
					    | SOS_VM_MAP_PROT_WRITE
					    | SOS_VM_MAP_ATOMIC));
  sos_physmem_unref_physpage(ppage_paddr);

  /* Ok, we can now return to interrupted context */
}


/* ======================================================================
 * Demonstrate the use of SOS kernel threads
 *  - Kernel Threads are created with various priorities and their
 *    state is printed on both the console and the bochs' 0xe9 port
 *  - For tests regarding threads' synchronization, see mouse_sim.c
 */

struct thr_arg
{
  char character;
  int  color;

  int col;
  int row;
};


static void demo_thread(void *arg)
{
  struct thr_arg *thr_arg = (struct thr_arg*)arg;
  int progress = 0;

  printf("start %c", thr_arg->character);
  while (1)
    {
      progress ++;
      display_bits(thr_arg->row, thr_arg->col+1, thr_arg->color, progress);

      putchar(thr_arg->character);

      /* Yield the CPU to another thread sometimes... */
      if ((random() % 100) == 0)
	{
	  printf("[37myield(%c)[m\n", thr_arg->character);
	  os_putchar(thr_arg->row, thr_arg->col, 0x1e, 'Y');
	  SOS_ASSERT_FATAL(SOS_OK == sos_thread_yield());
	  os_putchar(thr_arg->row, thr_arg->col, 0x1e, 'R');
	}

      /* Go to sleep some other times... */
      else if ((random() % 200) == 0)
	{
	  struct sos_time t = (struct sos_time){ .sec=0, .nanosec=50000000 };
	  printf("[37msleep1(%c)[m\n", thr_arg->character);
	  os_putchar(thr_arg->row, thr_arg->col, 0x1e, 's');
	  SOS_ASSERT_FATAL(SOS_OK == sos_thread_sleep(& t));
	  SOS_ASSERT_FATAL(sos_time_is_zero(& t));
	  os_putchar(thr_arg->row, thr_arg->col, 0x1e, 'R');
	}

      /* Go to sleep for a longer time some other times... */
      else if ((random() % 300) == 0)
	{
	  struct sos_time t = (struct sos_time){ .sec=0, .nanosec=300000000 };
	  printf("[37msleep2(%c)[m\n", thr_arg->character);
	  os_putchar(thr_arg->row, thr_arg->col, 0x1e, 'S');
	  SOS_ASSERT_FATAL(SOS_OK == sos_thread_sleep(& t));
	  SOS_ASSERT_FATAL(sos_time_is_zero(& t));
	  os_putchar(thr_arg->row, thr_arg->col, 0x1e, 'R');
	}

      /* Infinite loop otherwise */
    }
}


static void test_thread()
{
  /* "static" variables because we want them to remain even when the
     function returns */
  static struct thr_arg arg_b, arg_c, arg_d, arg_e, arg_R, arg_S;
  sos_ui32_t flags;

  sos_disable_IRQs(flags);

  arg_b = (struct thr_arg) { .character='b', .col=0, .row=21, .color=0x14 };
  sos_create_kernel_thread("YO[b]", demo_thread, (void*)&arg_b);

  arg_c = (struct thr_arg) { .character='c', .col=46, .row=21, .color=0x14 };
  sos_create_kernel_thread("YO[c]", demo_thread, (void*)&arg_c);

  arg_d = (struct thr_arg) { .character='d', .col=0, .row=20, .color=0x14 };
  sos_create_kernel_thread("YO[d]", demo_thread, (void*)&arg_d);

  arg_e = (struct thr_arg) { .character='e', .col=0, .row=19, .color=0x14 };
  sos_create_kernel_thread("YO[e]", demo_thread, (void*)&arg_e);

  arg_R = (struct thr_arg) { .character='R', .col=0, .row=17, .color=0x1c };
  sos_create_kernel_thread("YO[R]", demo_thread, (void*)&arg_R);

  arg_S = (struct thr_arg) { .character='S', .col=0, .row=16, .color=0x1c };
  sos_create_kernel_thread("YO[S]", demo_thread, (void*)&arg_S);

  sos_restore_IRQs(flags);
}


/* ======================================================================
 * An operating system MUST always have a ready thread ! Otherwise:
 * what would the CPU have to execute ?!
 */
static void idle_thread()
{
  sos_ui32_t idle_twiddle = 0;

  while (1)
    {
      /* Remove this instruction if you get an "Invalid opcode" CPU
	 exception (old 80386 CPU) */
      asm("hlt\n");

      idle_twiddle ++;
      display_bits(0, 0, SOS_X86_VIDEO_FG_GREEN | SOS_X86_VIDEO_BG_BLUE,
		   idle_twiddle);
      
      /* Lend the CPU to some other thread */
      sos_thread_yield();
    }
}





/* ====================================================================================== */
/* Check if MAGIC is valid and print the Multiboot information structure pointed by ADDR. */
/* ====================================================================================== */
void cmain (unsigned long magic, unsigned long addr)
{
	sos_paddr_t sos_kernel_core_base_paddr, sos_kernel_core_top_paddr;
	struct sos_time tick_resolution;

	/* Grub sends us a structure, called multiboot_info_t with a lot of
	   precious informations about the system, see the multiboot
	   documentation for more information. */
	/*multiboot_info_t *mbi;
	mbi = (multiboot_info_t *) addr;*/

  

	/* Clear the screen.  */
	cls ();

	/* Am I booted by a Multiboot-compliant boot loader?  */
	if (magic != MULTIBOOT2_BOOTLOADER_MAGIC)
	{
		printf ("Invalid magic number: 0x%x\n", (unsigned) magic);
		return;
	}

	if (addr & 7)
	{
		printf ("Unaligned mbi: 0x%x\n", addr);
		return;
	}

	printf("================================================================ \n");
	printf("Welcome From 'Multiboot-compliant boot loader' GRUB2 to SaghirOS \n");
	printf("================================================================ \n");

	/* Print the Multi-Boot Information structure */
	mbi_print(magic, addr);

/* =====================================================================================  */

	/* Setup CPU segmentation and IRQ subsystem */
	sos_gdt_subsystem_setup();
	sos_idt_subsystem_setup();


	/* Setup SOS IRQs and exceptions subsystem */
	sos_exception_subsystem_setup();
	sos_irq_subsystem_setup();


	/* Configure the timer so as to raise the IRQ0 at a 100Hz rate */
	sos_i8254_set_frequency(100);

	/* Setup the kernel time subsystem to get prepared to take the timer
	   ticks into account */
	tick_resolution = (struct sos_time) { .sec=0, .nanosec=10000000UL };
	sos_time_subsysem_setup(& tick_resolution);


	/* Binding some HW interrupts and exceptions to software routines */
	sos_irq_set_routine(SOS_IRQ_TIMER,
			    clk_it);


/* =====================================================================================  */
	/* Multiboot says: "The value returned for upper memory is maximally
	   the address of the first upper memory hole minus 1 megabyte.". It
	   also adds: "It is not guaranteed to be this value." aka "YMMV" ;) */
	sos_physmem_subsystem_setup((mbi_tag_mem->mem_upper<<10) + (1<<20),
				& sos_kernel_core_base_paddr,
				& sos_kernel_core_top_paddr);

/* =====================================================================================  */

	/*
	 * Switch to paged-memory mode
	 */

	/* Disabling interrupts should seem more correct, but it's not really
	   necessary at this stage */
 	SOS_ASSERT_FATAL(SOS_OK ==
		   sos_paging_subsystem_setup(sos_kernel_core_base_paddr,
					      sos_kernel_core_top_paddr));
	
	/* Bind the page fault exception */
	sos_exception_set_routine(SOS_EXCEPT_PAGE_FAULT,
					pgflt_ex);
	cls ();

 	/*
	 * Setup kernel virtual memory allocator
	 */ 
	if (sos_kmem_vmm_subsystem_setup(sos_kernel_core_base_paddr,
 	                          sos_kernel_core_top_paddr,
 	                          bootstrap_stack_bottom,
 	                          bootstrap_stack_bottom + bootstrap_stack_size))
		printf("Could not setup the Kernel virtual space allocator\n");
	 
	if (sos_kmalloc_subsystem_setup())
		printf("Could not setup the Kmalloc subsystem\n");
 	 

	/*
	 * Initialize the Kernel thread and scheduler subsystems
	 */
  
	/* Initialize kernel thread subsystem */
	sos_thread_subsystem_setup(	bootstrap_stack_bottom,
					bootstrap_stack_size);

	/* Initialize the scheduler */
	sos_sched_subsystem_setup();

	/* Declare the IDLE thread */
	SOS_ASSERT_FATAL(sos_create_kernel_thread("idle", idle_thread, NULL) != NULL);

	/* Enabling the HW interrupts here, this will make the timer HW
	interrupt call the scheduler */
	asm volatile ("sti\n");


	/* Now run some Kernel threads just for fun ! */
	extern void MouseSim();
	MouseSim();
	test_thread();



  /*
   * We can safely exit from this function now, for there is already
   * an idle Kernel thread ready to make the CPU busy working...
   *
   * However, we must EXPLICITELY call sos_thread_exit() because a
   * simple "return" will return nowhere ! Actually this first thread
   * was initialized by the Grub bootstrap stage, at a time when the
   * word "thread" did not exist. This means that the stack was not
   * setup in order for a return here to call sos_thread_exit()
   * automagically. Hence we must call it manually. This is the ONLY
   * kernel thread where we must do this manually.
   */
	printf("Bye from primary thread !\n");
	sos_thread_exit();
	SOS_FATAL_ERROR("No trespassing !");
 
}


