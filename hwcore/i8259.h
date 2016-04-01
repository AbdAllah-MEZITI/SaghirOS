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
#ifndef _SOS_i8259_H_
#define _SOS_i8259_H_

#include <os/errno.h>

/**
 * @file i8259.h PIC
 *
 * PIC Management routines. See the Intel 8259A datasheet (on kos
 * website), page 9+. Should be not be used directly: only interrupt.c
 * should use this.
 *
 * @see i8259A datasheet on Kos website.
 */

/** Setup PIC and Disable all IRQ lines */
sos_ret_t sos_i8259_setup(void);

sos_ret_t sos_i8259_enable_irq_line(int numirq);

sos_ret_t sos_i8259_disable_irq_line(int numirq);

#endif /* _SOS_i8259_H_ */
