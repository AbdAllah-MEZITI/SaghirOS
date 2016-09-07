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
#include "os/assert.h"

extern struct multiboot_tag_basic_meminfo* mbi_tag_mem;

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


/* Page fault exception handler */
static void pgflt_ex(int exid)
{
  printf("Got page fault\n");
  os_printf(10, 30,
			  SOS_X86_VIDEO_FG_LTRED | SOS_X86_VIDEO_BG_BLUE,
			  "Got EXPECTED (?) Page fault ! But where ???");
  for (;;) ;
}


static void test_paging(sos_vaddr_t sos_kernel_core_top_vaddr)
{
  /* The (linear) address of the page holding the code we are currently executing */
  sos_vaddr_t vpage_code = SOS_PAGE_ALIGN_INF(test_paging);

  /* The new physical page that will hold the code */
  sos_paddr_t ppage_new;

  /* Where this page will be mapped temporarily in order to copy the
     code into it: right after the kernel code/data */
  sos_vaddr_t vpage_tmp = sos_kernel_core_top_vaddr;

  unsigned i;

  /* Bind the page fault exception to one of our routines */
  sos_exception_set_routine(SOS_EXCEPT_PAGE_FAULT,
			    pgflt_ex);

  /*
   * Test 1: move the page where we execute the code elsewhere in
   * physical memory
   */
  os_printf(4, 0,
			  SOS_X86_VIDEO_FG_LTGREEN | SOS_X86_VIDEO_BG_BLUE,
			  "Moving current code elsewhere in physical memory:");


  /* Allocate a new physical page */
  ppage_new = sos_physmem_ref_physpage_new(FALSE);
  if (! ppage_new)
    {
      /* STOP ! No memory left */
      os_printf(20, 0,
				 SOS_X86_VIDEO_FG_LTRED
				   | SOS_X86_VIDEO_BG_BLUE,
				 "test_paging : Cannot allocate page");
      return;
    }

  os_printf(5, 0,
			  SOS_X86_VIDEO_FG_YELLOW | SOS_X86_VIDEO_BG_BLUE,
			  "Hello from the address 0x%x in physical memory",
			  sos_paging_get_paddr(vpage_code));

  os_printf(6, 0,
			  SOS_X86_VIDEO_FG_YELLOW | SOS_X86_VIDEO_BG_BLUE,
			  "Transfer vpage 0x%x: ppage 0x%x -> 0x%x (tmp vpage 0x%x)",
	                  vpage_code,
			  sos_paging_get_paddr(vpage_code),
			  ppage_new,
			  (unsigned)vpage_tmp);

  /* Map the page somewhere (right after the kernel mapping) in order
     to copy the code we are currently executing */
  sos_paging_map(ppage_new, vpage_tmp,
		 FALSE,
		 SOS_VM_MAP_ATOMIC
		 | SOS_VM_MAP_PROT_READ
		 | SOS_VM_MAP_PROT_WRITE);

  /* Ok, the new page is referenced by the mapping, we can release our reference to it */
  sos_physmem_unref_physpage(ppage_new);

  /* Copy the contents of the current page of code to this new page mapping */
  memcpy((void*)vpage_tmp,
	 (void*)vpage_code,
	 SOS_PAGE_SIZE);

  /* Transfer the mapping of the current page of code to this new page */
  sos_paging_map(ppage_new, vpage_code,
		 FALSE,
		 SOS_VM_MAP_ATOMIC
		 | SOS_VM_MAP_PROT_READ
		 | SOS_VM_MAP_PROT_WRITE);
  
  /* Ok, here we are: we have changed the physcal page that holds the
     code we are executing ;). However, this new page is mapped at 2
     virtual addresses:
     - vpage_tmp
     - vpage_code
     We can safely unmap it from sos_kernel_core_top_vaddr, while
     still keeping the vpage_code mapping */
  sos_paging_unmap(vpage_tmp);

  os_printf(7, 0,
			  SOS_X86_VIDEO_FG_YELLOW | SOS_X86_VIDEO_BG_BLUE,
			  "Hello from the address 0x%x in physical memory",
			  sos_paging_get_paddr(vpage_code));

  os_printf(9, 0,
			  SOS_X86_VIDEO_FG_LTGREEN | SOS_X86_VIDEO_BG_BLUE,
			  "Provoking a page fault:");

  /*
   * Test 2: make sure the #PF handler works
   */

  /* Scan part of the kernel up to a page fault. This page fault
     should occur on the first page unmapped after the kernel area,
     which is exactly the page we temporarily mapped/unmapped
     (vpage_tmp) above to move the kernel code we are executing */
  for (i = vpage_code ; /* none */ ; i += SOS_PAGE_SIZE)
    {
      unsigned *pint = (unsigned *)SOS_PAGE_ALIGN_INF(i);
      printf("Test vaddr 0x%x : val=", (unsigned)pint);
      os_printf(10, 0,
			      SOS_X86_VIDEO_FG_YELLOW | SOS_X86_VIDEO_BG_BLUE,
			      "Test vaddr 0x%x : val=      ",
			      (unsigned)pint);
      printf("0x%x\n", *pint);
      os_printf(10, 30,
			      SOS_X86_VIDEO_FG_YELLOW | SOS_X86_VIDEO_BG_BLUE,
			      "0x%x          ", *pint);
    }

  /* BAD ! Did not get the page fault... */
  os_printf(20, 0,
			  SOS_X86_VIDEO_FG_LTRED | SOS_X86_VIDEO_BG_BLUE,
			  "We should have had a #PF at vaddr 0x%x !",
			  vpage_tmp);
}





/* ====================================================================================== */
/* Check if MAGIC is valid and print the Multiboot information structure pointed by ADDR. */
/* ====================================================================================== */
void cmain (unsigned long magic, unsigned long addr)
{
	sos_paddr_t sos_kernel_core_base_paddr, sos_kernel_core_top_paddr;

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
	sos_gdt_setup();
	sos_idt_setup();


	/* Setup SOS IRQs and exceptions subsystem */
	sos_exceptions_setup();
	sos_irq_setup();


	/* Configure the timer so as to raise the IRQ0 at a 100Hz rate */
	sos_i8254_set_frequency(100);


	/* Binding some HW interrupts and exceptions to software routines */
	sos_irq_set_routine(SOS_IRQ_TIMER,
			    clk_it);

	/* Enabling the HW interrupts here, this will make the timer HW
     interrupt call our clk_it handler */
	asm volatile ("sti\n");


/* =====================================================================================  */
	/* Multiboot says: "The value returned for upper memory is maximally
	   the address of the first upper memory hole minus 1 megabyte.". It
	   also adds: "It is not guaranteed to be this value." aka "YMMV" ;) */
	sos_physmem_setup((mbi_tag_mem->mem_upper<<10) + (1<<20),
				& sos_kernel_core_base_paddr,
				& sos_kernel_core_top_paddr);

/* =====================================================================================  */
	/* Switch to paged-memory mode */

	/* Disabling interrupts should seem more correct, but it's not really necessary at this stage */
	if (sos_paging_setup(
				sos_kernel_core_base_paddr,
				sos_kernel_core_top_paddr))
		printf("Could not setup paged memory mode\n");
	os_printf(2,0, 
			SOS_X86_VIDEO_FG_YELLOW | SOS_X86_VIDEO_BG_BLUE,
			"Paged-memory mode is activated");
	
	cls ();
	test_paging(sos_kernel_core_top_paddr);

	/* An operatig system never ends */
	for (;;)
		continue;

	return;
 
}


