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
#ifndef _SOS_IDT_H_
#define _SOS_IDT_H_

/**
 * @file idt.h
 *
 * Manage the x86 Interrupt Descriptor Table, the table which maps the
 * hardware interrupt lines, hardware exceptions, and software
 * interrupts, to software routines. We only define "interrupt gate"
 * IDT entries. Don't use it directly; refer instead to interrupt.c,
 * exceptions.c and syscall.c.
 *
 * @see Intel x86 doc, Vol 3, chapter 5
 */

#include <os/errno.h>
#include <os/types.h>

/* Mapping of the CPU exceptions in the IDT (imposed by Intel
   standards) */
#define SOS_EXCEPT_BASE 0
#define SOS_EXCEPT_NUM  32
#define SOS_EXCEPT_MAX  (SOS_HWEXCEPT_BASE + SOS_HWEXCEPT_NUM - 1)

/* Mapping of the IRQ lines in the IDT */
#define SOS_IRQ_BASE    32
#define SOS_IRQ_NUM     16
#define SOS_IRQ_MAX     (SOS_IRQ_BASE + SOS_IRQ_NUM - 1)

/**
 * Number of IDT entries.
 *
 * @note Must be large enough to map the hw interrupts, the exceptions
 * (=> total is 48 entries), and the syscall(s). Since our syscall
 * will be 0x42, it must be >= 0x43. Intel doc limits this to 256
 * entries, we use this limit.
 */
#define SOS_IDTE_NUM      256 /* 0x100 */

/** Initialization routine: all the IDT entries (or "IDTE") are marked
    "not present". */
sos_ret_t sos_idt_setup(void);

/**
 * Enable the IDT entry if handler_address != NULL, with the given
 * lowest_priviledge.\ Disable the IDT entry when handler_address ==
 * NULL (the lowest_priviledge parameter is then ignored). Intel doc
 * says that there must not be more than 256 entries.
 *
 * @note IRQ Unsafe
 */
sos_ret_t sos_idt_set_handler(int index,
			      sos_vaddr_t handler_address,
			      int lowest_priviledge /* 0..3 */);


/**
 * @note IRQ Unsafe
 *
 * @return the handler address and DPL in the 2nd and 3rd
 * parameters
 */
sos_ret_t sos_idt_get_handler(int index,
			      sos_vaddr_t *handler_address,
			      int *lowest_priviledge);

#endif /* _SOS_IDT_H_ */
