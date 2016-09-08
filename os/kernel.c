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



/* == */
/*    */
/* == */

struct digit
{
  struct digit *prev, *next;
  char value;
};

/* Representation of a big (positive) integer: Most Significant Digit
   (MSD) is the HEAD of the list. Least Significant Digit (LSD) is the
   TAIL of the list */
typedef struct digit * big_number_t;


/* Add a new digit after the LSD */
void bn_push_lsd(big_number_t * bn, char value)
{
  struct digit *d;
  d = (struct digit*) sos_kmalloc(sizeof(struct digit), 0);
  SOS_ASSERT_FATAL(d != NULL);
  d->value = value;
  list_add_tail(*bn, d);
}


/* Add a new digit before the MSD */
void bn_push_msd(big_number_t * bn, char value)
{
  struct digit *d;
  d = (struct digit*) sos_kmalloc(sizeof(struct digit), 0);
  SOS_ASSERT_FATAL(d != NULL);
  d->value = value;
  list_add_head(*bn, d);
}


/* Construct a big integer from a (machine) integer */
big_number_t bn_new(unsigned long int i)
{
  big_number_t retval;

  list_init(retval);
  do
    {
      bn_push_msd(&retval, i%10);
      i /= 10;
    }
  while (i != 0);

  return retval;
}


/* Create a new big integer from another big integer */
big_number_t bn_copy(const big_number_t bn)
{
  big_number_t retval;
  int nb_elts;
  struct digit *d;

  list_init(retval);
  list_foreach(bn, d, nb_elts)
    {
      bn_push_lsd(&retval, d->value);
    }

  return retval;
}


/* Free the memory used by a big integer */
void bn_del(big_number_t * bn)
{
  struct digit *d;

  list_collapse(*bn, d)
    {
      sos_kfree((sos_vaddr_t)d);
    }
}


/* Shift left a big integer: bn := bn*10^shift */
void bn_shift(big_number_t *bn, int shift)
{
  for ( ; shift > 0 ; shift --)
    {
      bn_push_lsd(bn, 0);
    }
}


/* Dump the big integer in bochs */
void bn_print_bochs(const big_number_t bn)
{
  int nb_elts;
  const struct digit *d;

  if (list_is_empty(bn))
    printf("0");
  else
    list_foreach(bn, d, nb_elts)
  printf("%d", d->value);
}

/* Dump the big integer on the console */
void bn_print_console(unsigned char row, unsigned char col,
		      unsigned char attribute,
		      const big_number_t bn,
		      int nb_decimals)
{
  if (list_is_empty(bn))
    os_printf(row, col, attribute, "0");
  else
    {
      int nb_elts;
      const struct digit *d;
      unsigned char x = col;

      list_foreach(bn, d, nb_elts)
	{
	  if (nb_elts == 0)
	    {
	      os_printf(row, x, attribute, "%d.", d->value);
	      x += 2;
	    }
	  else if (nb_elts < nb_decimals)
	    {
	      os_printf(row, x, attribute, "%d", d->value);
	      x ++;
	    }
	}

      os_printf(row, x, attribute, " . 10^{%d}  ", nb_elts-1);
    }
}


/* Result is the addition of 2 big integers */
big_number_t bn_add (const big_number_t bn1, const big_number_t bn2)
{
  big_number_t retval;
  const struct digit *d1, *d2;
  sos_bool_t  bn1_end = FALSE, bn2_end = FALSE;
  char carry = 0;

  list_init(retval);
  d1 = list_get_tail(bn1);
  bn1_end = list_is_empty(bn1);
  d2 = list_get_tail(bn2);
  bn2_end = list_is_empty(bn2);
  do
    {
      if (! bn1_end)
	carry += d1->value;
      if (! bn2_end)
	carry += d2->value;

      bn_push_msd(&retval, carry % 10);
      carry  /= 10;

      if (! bn1_end)
	d1 = d1->prev;
      if (! bn2_end)
	d2 = d2->prev;
      if (d1 == list_get_tail(bn1))
	bn1_end = TRUE;
      if (d2 == list_get_tail(bn2))
	bn2_end = TRUE;
    }
  while (!bn1_end || !bn2_end);

  if (carry > 0)
    {
      bn_push_msd(&retval, carry);
    }

  return retval;
}


/* Result is the multiplication of a big integer by a single digit */
big_number_t bn_muli (const big_number_t bn, char digit)
{
  big_number_t retval;
  int nb_elts;
  char   carry = 0;
  const struct digit *d;

  list_init(retval);
  list_foreach_backward(bn, d, nb_elts)
    {
      carry += d->value * digit;
      bn_push_msd(&retval, carry % 10);
      carry /= 10;
    }

  if (carry > 0)
    {
      bn_push_msd(&retval, carry);
    }

  return retval;
}


/* Result is the multiplication of 2 big integers */
big_number_t bn_mult(const big_number_t bn1, const big_number_t bn2)
{
  int shift = 0;
  big_number_t retval;
  int nb_elts;
  struct digit *d;

  list_init(retval);
  list_foreach_backward(bn2, d, nb_elts)
    {
      big_number_t retmult = bn_muli(bn1, d->value);
      big_number_t old_retval = retval;
      bn_shift(& retmult, shift);
      retval = bn_add(old_retval, retmult);
      bn_del(& retmult);
      bn_del(& old_retval);
      shift ++;
    }

  return retval;
}


/* Result is the factorial of an integer */
big_number_t bn_fact(unsigned long int v)
{
  unsigned long int i;
  big_number_t retval = bn_new(1);
  for (i = 1 ; i <= v ; i++)
    {
      big_number_t I   = bn_new(i);
      big_number_t tmp = bn_mult(retval, I);
      os_printf(4, 0,
			      SOS_X86_VIDEO_BG_BLUE | SOS_X86_VIDEO_FG_LTGREEN,
			      "%d! = ", (int)i);
      bn_print_console(4, 8, SOS_X86_VIDEO_BG_BLUE | SOS_X86_VIDEO_FG_WHITE,
		       tmp, 55);
      bn_del(& I);
      bn_del(& retval);
      retval = tmp;
    }

  return retval;
}


void bn_test()
{
  big_number_t bn = bn_fact(1000);
  printf("1000! = ");
  bn_print_bochs(bn);
  printf("\n");
  
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

 	/*
	 * Setup kernel virtual memory allocator
	 */ 
	if (sos_kmem_vmm_setup(sos_kernel_core_base_paddr,
 	                          sos_kernel_core_top_paddr,
 	                          bootstrap_stack_bottom,
 	                          bootstrap_stack_bottom + bootstrap_stack_size))
		printf("Could not setup the Kernel virtual space allocator\n");
	 
	if (sos_kmalloc_setup())
		printf("Could not setup the Kmalloc subsystem\n");
 	 
	/* Run some kmalloc tests */
	bn_test();

	/* An operatig system never ends */
	for (;;)
		continue;

	return;
 
}


