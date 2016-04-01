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
#ifndef _SOS_HWEXCEPT_H_
#define _SOS_HWEXCEPT_H_

/**
 * @file exception.c
 *
 * Hardware exception routines management.
 */

#ifndef ASM_SOURCE
#  include <os/errno.h>
#endif

/**
 * Standard Intel x86 exceptions.
 *
 * @see Intel x86 doc vol 3, section 5.12.
 */
#define SOS_EXCEPT_DIVIDE_ERROR                  0         // No error code
#define SOS_EXCEPT_DEBUG                         1         // No error code
#define SOS_EXCEPT_NMI_INTERRUPT                 2         // No error code
#define SOS_EXCEPT_BREAKPOINT                    3         // No error code
#define SOS_EXCEPT_OVERFLOW                      4         // No error code
#define SOS_EXCEPT_BOUND_RANGE_EXCEDEED          5         // No error code
#define SOS_EXCEPT_INVALID_OPCODE                6         // No error code
#define SOS_EXCEPT_DEVICE_NOT_AVAILABLE          7         // No error code
#define SOS_EXCEPT_DOUBLE_FAULT                  8         // Yes (Zero)
#define SOS_EXCEPT_COPROCESSOR_SEGMENT_OVERRUN   9         // No error code
#define SOS_EXCEPT_INVALID_TSS                  10         // Yes
#define SOS_EXCEPT_SEGMENT_NOT_PRESENT          11         // Yes
#define SOS_EXCEPT_STACK_SEGMENT_FAULT          12         // Yes
#define SOS_EXCEPT_GENERAL_PROTECTION           13         // Yes
#define SOS_EXCEPT_PAGE_FAULT                   14         // Yes
#define SOS_EXCEPT_INTEL_RESERVED_1             15         // No
#define SOS_EXCEPT_FLOATING_POINT_ERROR         16         // No
#define SOS_EXCEPT_ALIGNEMENT_CHECK             17         // Yes (Zero)
#define SOS_EXCEPT_MACHINE_CHECK                18         // No
#define SOS_EXCEPT_INTEL_RESERVED_2             19         // No
#define SOS_EXCEPT_INTEL_RESERVED_3             20         // No
#define SOS_EXCEPT_INTEL_RESERVED_4             21         // No
#define SOS_EXCEPT_INTEL_RESERVED_5             22         // No
#define SOS_EXCEPT_INTEL_RESERVED_6             23         // No
#define SOS_EXCEPT_INTEL_RESERVED_7             24         // No
#define SOS_EXCEPT_INTEL_RESERVED_8             25         // No
#define SOS_EXCEPT_INTEL_RESERVED_9             26         // No
#define SOS_EXCEPT_INTEL_RESERVED_10            27         // No
#define SOS_EXCEPT_INTEL_RESERVED_11            28         // No
#define SOS_EXCEPT_INTEL_RESERVED_12            29         // No
#define SOS_EXCEPT_INTEL_RESERVED_13            30         // No
#define SOS_EXCEPT_INTEL_RESERVED_14            31         // No

#ifndef ASM_SOURCE

typedef void (*sos_exception_handler_t)(int exception_number);

sos_ret_t sos_exceptions_setup(void);
sos_ret_t sos_exception_set_routine(int exception_number,
				    sos_exception_handler_t routine);
sos_exception_handler_t sos_exception_get_routine(int exception_number);
#endif /* ! ASM_SOURCE */

#endif /* _SOS_HWEXCEPT_H_ */
