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
#ifndef _SOS_HWINTR_H_
#define _SOS_HWINTR_H_

/**
 * @file irq.c
 *
 * Hardware interrupts routines management.
 */

#include <os/errno.h>

#define sos_save_flags(flags) \
  asm volatile("pushfl ; popl %0":"=g"(flags)::"memory")
#define sos_restore_flags(flags) \
  asm volatile("push %0; popfl"::"g"(flags):"memory")

#define sos_disable_IRQs(flags)    \
  ({ sos_save_flags(flags); asm("cli\n"); })
#define sos_restore_IRQs(flags)    \
  sos_restore_flags(flags)

/* Usual IRQ levels */
#define SOS_IRQ_TIMER         0
#define SOS_IRQ_KEYBOARD      1
#define SOS_IRQ_SLAVE_PIC     2
#define SOS_IRQ_COM2          3
#define SOS_IRQ_COM1          4
#define SOS_IRQ_LPT2          5
#define SOS_IRQ_FLOPPY        6
#define SOS_IRQ_LPT1          7
#define SOS_IRQ_8_NOT_DEFINED 8
#define SOS_IRQ_RESERVED_1    9
#define SOS_IRQ_RESERVED_2    10
#define SOS_IRQ_RESERVED_3    11
#define SOS_IRQ_RESERVED_4    12
#define SOS_IRQ_COPROCESSOR   13
#define SOS_IRQ_HARDDISK      14
#define SOS_IRQ_RESERVED_5    15

typedef void (*sos_irq_handler_t)(int irq_level);

/** Setup the PIC */
sos_ret_t sos_irq_setup(void);

/**
 * If the routine is not NULL, the IDT is setup to call an IRQ
 * wrapper upon interrupt, which in turn will call the routine, and
 * the PIC is programmed to raise an irq.\ If the routine is
 * NULL, we disable the irq line.
 */
sos_ret_t sos_irq_set_routine(int irq_level,
			      sos_irq_handler_t routine);

sos_irq_handler_t sos_irq_get_routine(int irq_level);

#endif /* _SOS_HWINTR_H_ */
