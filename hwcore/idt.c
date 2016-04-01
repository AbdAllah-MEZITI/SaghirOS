/* Copyright (C) 2004  David Decotigny
   Copyright (C) 1999  Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA. 
*/
#include "segment.h"

#include "idt.h"

/**
 * An entry in the IDT, or "IDTE" in the following, ie a reference to
 * a interrupt/trap routine or a task gate to handle the sw/hw
 * interrupts and exceptions.
 *
 * @see figure 5-2, intel x86 doc, vol 3
 */
struct x86_idt_entry
{
  /* Low dword */
  sos_ui16_t offset_low;  /* 15..0, offset of the routine in the segment */
  sos_ui16_t seg_sel;     /* 31..16, the ID of the segment */

  /* High dword */
  sos_ui8_t reserved:5;   /* 4..0 */
  sos_ui8_t flags:3;      /* 7..5 */
  sos_ui8_t type:3;       /* 10..8 (interrupt gate, trap gate...) */
  sos_ui8_t op_size:1;    /* 11 (0=16bits instructions, 1=32bits instr.) */
  sos_ui8_t zero:1;       /* 12 */
  sos_ui8_t dpl:2;        /* 14..13 */
  sos_ui8_t present:1;    /* 15 */
  sos_ui16_t offset_high; /* 31..16 */
} __attribute__((packed));


/**
 * The IDT register, which stores the address and size of the
 * IDT.
 *
 * @see Intel x86 doc vol 3, section 2.4, figure 2-4
 */
struct x86_idt_register
{
  /* The maximum GDT offset allowed to access an entry in the GDT */
  sos_ui16_t  limit;

  /* This is not exactly a "virtual" address, ie an adddress such as
     those of instructions and data; this is a "linear" address, ie an
     address in the paged memory. However, in SOS we configure the
     segmented memory as a "flat" space: the 0-4GB segment-based (ie
     "virtual") addresses directly map to the 0-4GB paged memory (ie
     "linear"), so that the "linear" addresses are numerically equal
     to the "virtual" addresses: this base_addr will thus be the same
     as the address of the gdt array */
  sos_ui32_t base_addr;
} __attribute__((packed, aligned (8)));


static struct x86_idt_entry    idt[SOS_IDTE_NUM];

sos_ret_t sos_idt_setup()
{
  struct x86_idt_register idtr;
  int i;

  for (i = 0 ;
       i < SOS_IDTE_NUM ;
       i++)
    {
      struct x86_idt_entry *idte = idt + i;

      /* Setup an empty IDTE interrupt gate, see figure 5-2 in Intel
	 x86 doc, vol 3 */
      idte->seg_sel   = SOS_BUILD_SEGMENT_REG_VALUE(0, FALSE, SOS_SEG_KCODE);
      idte->reserved  = 0;
      idte->flags     = 0;
      idte->type      = 0x6; /* Interrupt gate (110b) */
      idte->op_size   = 1;   /* 32bits instructions */
      idte->zero      = 0;

      /* Disable this IDT entry for the moment */
      sos_idt_set_handler(i, (sos_vaddr_t)NULL, 0/* Don't care */);
    }

  /*
   * Setup the IDT register, see Intel x86 doc vol 3, section 5.8.
   */

  /* Address of the IDT */
  idtr.base_addr  = (sos_ui32_t) idt;

  /* The limit is the maximum offset in bytes from the base address of
     the IDT */
  idtr.limit      = sizeof(idt) - 1;

  /* Commit the IDT into the CPU */
  asm volatile ("lidt %0\n"::"m"(idtr):"memory");
  
  return SOS_OK;
}


sos_ret_t sos_idt_set_handler(int index,
			      sos_vaddr_t handler_address,
			      int lowest_priviledge /* 0..3 */)
{
  struct x86_idt_entry *idte;

  if ((index < 0) || (index >= SOS_IDTE_NUM))
    return -SOS_EINVAL;
  if ((lowest_priviledge < 0) || (lowest_priviledge > 3))
    return -SOS_EINVAL;
  
  idte = idt + index;
  if (handler_address != (sos_vaddr_t)NULL)
    {
      idte->offset_low  = handler_address & 0xffff;
      idte->offset_high = (handler_address >> 16) & 0xffff;
      idte->dpl         = lowest_priviledge;
      idte->present     = 1;   /* Yes, there is a handler */
    }
  else /* Disable this IDT entry */
    {
      idte->offset_low  = 0;
      idte->offset_high = 0;
      idte->dpl         = 0;
      idte->present     = 0;   /* No, there is no handler */
    }

  return SOS_OK;
}


sos_ret_t sos_idt_get_handler(int index,
			      sos_vaddr_t *handler_address,
			      int *lowest_priviledge)
{
  if ((index < 0) || (index >= SOS_IDTE_NUM))
    return -SOS_EINVAL;

  if (handler_address != NULL)
    *handler_address = idt[index].offset_low
                       | (idt[index].offset_high << 16);
  if (lowest_priviledge != NULL)
    *lowest_priviledge = idt[index].dpl;

  return SOS_OK;
}
