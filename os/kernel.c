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

/* Division by zero exception handler */
static void divide_ex(int exid)
{
  static sos_ui32_t div_count = 0;
  display_bits(0, 0,
	       SOS_X86_VIDEO_FG_LTRED | SOS_X86_VIDEO_BG_BLUE,
	       div_count);
  div_count++;
}



/* ====================================================================================== */
/* Check if MAGIC is valid and print the Multiboot information structure pointed by ADDR. */
/* ====================================================================================== */
void cmain (unsigned long magic, unsigned long addr)
{
	unsigned int i;  

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
	//mbi_print(magic, addr);

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
	sos_exception_set_routine(SOS_EXCEPT_DIVIDE_ERROR,
				  divide_ex);

	/* Enabling the HW interrupts here, this will make the timer HW
     interrupt call our clk_it handler */
	asm volatile ("sti\n");



	/* Raise a rafale of 'division by 0' exceptions.
	All this code is not really needed (equivalent to a bare "i=1/0;"),
	except when compiling with -O3: "i=1/0;" is considered dead code with gcc -O3. */
	i = 10;
	while (1)
	{
		/* Stupid function call to fool gcc optimizations */
		printf("i = 1 / %d...\n", i);
		//sos_bochs_printf("i = 1 / %d...\n", i);
		i = 1 / i;
	}

	/* Will never print this since the "divide by zero" exception always
	   returns to the faulting instruction (see Intel x86 doc vol 3,
	   section 5.12), thus re-evaluating the "divide-by-zero" exprssion
	   and raising the "divide by zero" exception again and again... */
	printf("Invisible \n");

}


