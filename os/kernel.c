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
#include "list.h"
#include "physmem.h"
/*#include "os/assert.h"*/

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



#define MY_PPAGE_NUM_INT 511
struct my_ppage
{
  sos_ui32_t before[MY_PPAGE_NUM_INT];
  struct my_ppage *prev, *next;
  sos_ui32_t after[MY_PPAGE_NUM_INT];
}; /* sizeof() Must be <= 4kB */

static void test_physmem()
{
  /* We place the pages we did allocate here */
  struct my_ppage *ppage_list, *my_ppage;
  sos_count_t num_alloc_ppages = 0, num_free_ppages = 0;

  ppage_list = NULL;
  while ((my_ppage = (struct my_ppage*)sos_physmem_ref_physpage_new(FALSE))
	 != NULL)
    {
      int i;
      num_alloc_ppages++;
      
      /* Print the allocation status */
      printf("Could allocate %d pages      \n", num_alloc_ppages);

      /* We fill this page with its address */
      for (i = 0 ; i < MY_PPAGE_NUM_INT ; i++)
	my_ppage->before[i] = my_ppage->after[i] = (sos_ui32_t)my_ppage;

      /* We add this page at the tail of our list of ppages */
      list_add_tail(ppage_list, my_ppage);
    }

  /* Now we release these pages in FIFO order */
  while ((my_ppage = list_pop_head(ppage_list)) != NULL)
    {
      /* We make sure this page was not overwritten by any unexpected
	 value */
      int i;
      for (i = 0 ; i < MY_PPAGE_NUM_INT ; i++)
	{
	  /* We don't get what we expect ! */
	  if ((my_ppage->before[i] !=  (sos_ui32_t)my_ppage)
	      || (my_ppage->after[i] !=  (sos_ui32_t)my_ppage))
	    {
	      /* STOP ! */
	      printf("Page overwritten");
	      return;
	    }
	}

      /* Release the descriptor */
      if (sos_physmem_unref_physpage((sos_paddr_t)my_ppage) < 0)
	{
	  /* STOP ! */
	  printf("Cannot release page");
	  return;
	}

      /* Print the deallocation status */
      num_free_ppages ++;
      printf("Could free %d pages      \n",
		num_free_ppages);
    }

  /* Print the overall stats */
  printf("Could allocate %d bytes, could free %d bytes     ",
			  num_alloc_ppages << SOS_PAGE_SHIFT,
			  num_free_ppages << SOS_PAGE_SHIFT);

  /*SOS_ASSERT_FATAL(num_alloc_ppages == num_free_ppages);*/
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


	/* Multiboot says: "The value returned for upper memory is maximally
	   the address of the first upper memory hole minus 1 megabyte.". It
	   also adds: "It is not guaranteed to be this value." aka "YMMV" ;) */
	sos_physmem_setup((mbi_tag_mem->mem_upper<<10) + (1<<20),
				& sos_kernel_core_base_paddr,
				& sos_kernel_core_top_paddr);

	test_physmem();

	/* An operatig system never ends */
	for (;;)
		continue;

	return;
 
}


