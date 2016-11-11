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
#ifndef _SOS_GDT_H_
#define _SOS_GDT_H_

/**
 * @file gdt.h
 *
 * The routines that manage the GDT, the table that maps the virtual
 * addresses (data/instructions, segment-relative), to "linear"
 * addresses (ie paged-memory). In SOS/x86, we use a "flat" virtual
 * space, ie the virtual and linear spaces are equivalent.
 *
 * @see Intel x86 doc vol 3, chapter 3
 */

#include <os/types.h>
#include <os/errno.h>

/**
 * Configure the virtual space as a direct mapping to the linear
 * address space (ie "flat" virtual space).
 */
sos_ret_t sos_gdt_subsystem_setup(void);

#endif /* _SOS_GDT_H_ */
