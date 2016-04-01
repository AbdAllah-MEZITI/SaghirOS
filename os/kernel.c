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


/* Check if MAGIC is valid and print the Multiboot information structure pointed by ADDR.  */
void cmain (unsigned long magic, unsigned long addr)
{  

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

}


