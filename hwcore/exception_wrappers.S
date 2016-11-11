/* Copyright (C) 2004  The KOS Team
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
#include "exception.h"
         
.file "exception_wrappers.S"

.text

/* The address of the table of handlers (defined in exception.c) */
.extern sos_exception_handler_array

/* The address of the table of wrappers (defined below, and shared
   with exception.c */
.globl sos_exception_wrapper_array

	
/**
 * For exceptions with/without error code, refer to Intel x86 doc vol 3,
 * section 5.12
 */

/* These wrappers are for exceptions without error code */
.irp id,                              \
         SOS_EXCEPT_DIVIDE_ERROR,                \
         SOS_EXCEPT_DEBUG,                       \
         SOS_EXCEPT_NMI_INTERRUPT,               \
         SOS_EXCEPT_BREAKPOINT,                  \
         SOS_EXCEPT_OVERFLOW,                    \
         SOS_EXCEPT_BOUND_RANGE_EXCEDEED,        \
         SOS_EXCEPT_INVALID_OPCODE,              \
         SOS_EXCEPT_DEVICE_NOT_AVAILABLE,        \
         SOS_EXCEPT_COPROCESSOR_SEGMENT_OVERRUN, \
         SOS_EXCEPT_INTEL_RESERVED_1,            \
         SOS_EXCEPT_FLOATING_POINT_ERROR,        \
         SOS_EXCEPT_MACHINE_CHECK,               \
         SOS_EXCEPT_INTEL_RESERVED_2,            \
         SOS_EXCEPT_INTEL_RESERVED_3,            \
         SOS_EXCEPT_INTEL_RESERVED_4,            \
         SOS_EXCEPT_INTEL_RESERVED_5,            \
         SOS_EXCEPT_INTEL_RESERVED_6,            \
         SOS_EXCEPT_INTEL_RESERVED_7,            \
         SOS_EXCEPT_INTEL_RESERVED_8,            \
         SOS_EXCEPT_INTEL_RESERVED_9,            \
         SOS_EXCEPT_INTEL_RESERVED_10,           \
         SOS_EXCEPT_INTEL_RESERVED_11,           \
         SOS_EXCEPT_INTEL_RESERVED_12,           \
         SOS_EXCEPT_INTEL_RESERVED_13,           \
         SOS_EXCEPT_INTEL_RESERVED_14
 
        .p2align 2, 0x90
        sos_exception_wrapper_\id:
        .type sos_exception_wrapper_\id,@function
 
                /* Fake error code */
                pushl $0
                /* Backup the context */
                pushl %ebp
                movl %esp, %ebp
 
                pushl %edi
                pushl %esi
                pushl %edx
                pushl %ecx
                pushl %ebx
                pushl %eax
                subl  $2,%esp
                pushw %ss
                pushw %ds
                pushw %es
                pushw %fs
                pushw %gs
 
		/*
		 * Call the handler with the exception number and the
		 * address of the stored CPU context as arguments
		 */
                pushl %esp
                pushl $\id
                leal  sos_exception_handler_array,%edi
                call  *\id*4(%edi)
                /* Unallocate the arguments passed to the handler */
                addl  $8, %esp

                /* Restore the context */
                popw  %gs
                popw  %fs
                popw  %es
                popw  %ds
                popw  %ss
                addl  $2,%esp
                popl  %eax
                popl  %ebx
                popl  %ecx
                popl  %edx
                popl  %esi
                popl  %edi
 
                popl  %ebp
                /* Remove fake error code */
                addl $4, %esp
                iret
.endr 
 
        /* These wrappers are for exceptions with error code */
.irp id,                              \
	SOS_EXCEPT_INVALID_TSS,                  \
	SOS_EXCEPT_SEGMENT_NOT_PRESENT,          \
	SOS_EXCEPT_STACK_SEGMENT_FAULT,          \
	SOS_EXCEPT_GENERAL_PROTECTION,           \
	SOS_EXCEPT_PAGE_FAULT,                   \
	SOS_EXCEPT_ALIGNEMENT_CHECK
 
        .p2align 2, 0x90
        sos_exception_wrapper_\id:
        .type sos_exception_wrapper_\id,@function
 
                /* ret eflags */
                /* ret cs */
                /* ret eip */
                /* Error code */
 
                /* Backup the context */
                pushl %ebp
                movl %esp, %ebp
 
                pushl %edi
                pushl %esi
                pushl %edx
                pushl %ecx
                pushl %ebx
                pushl %eax
                subl  $2,%esp
                pushw %ss
                pushw %ds
                pushw %es
                pushw %fs
                pushw %gs

		/*
		 * Call the handler with the exception number and the
		 * address of the stored CPU context as arguments
		 */
                pushl %esp
                pushl $\id
                leal  sos_exception_handler_array,%edi
                call  *\id*4(%edi)
                /* Unallocate the arguments passed to the handler */
                addl  $8, %esp

                /* Restore the context */
                popw  %gs
                popw  %fs
                popw  %es
                popw  %ds
                popw  %ss
                addl  $2,%esp
                popl  %eax
                popl  %ebx
                popl  %ecx
                popl  %edx
                popl  %esi
                popl  %edi
                popl  %ebp
 
                /* Error code isn't compatible with iretd */
                addl $4, %esp
 
                iret
.endr


/* Double fault handler not supported. We must define it since we
   define an entry for it in the sos_exception_wrapper_array. It
   simply uses an alternate stack to display a message and stop the
   system. qemu won't handle it correctly (see comment in qemu's
   sources). */
#define ALTERNATE_DOUBLE_FAULT_STACK_SIZE 512
.irp id, SOS_EXCEPT_DOUBLE_FAULT
.p2align 2, 0x90
sos_exception_wrapper_\id:
.type sos_exception_wrapper_\id,@function
1:	cli /* Not necessary */
	movl $double_fault_alternate_stack, %eax
	addl $ALTERNATE_DOUBLE_FAULT_STACK_SIZE, %eax
	movl %eax, %esp
	pushl $msg_double_fault_not_supported
	call sos_display_fatal_error ; jmp 1b /* Not necessary */
.endr

.section ".rodata"
msg_double_fault_not_supported:
	.string "exception_wrappers.S: Double fault detected ! NOT SUPPORTED yet. System Halted."

/* Build the sos_irq_wrapper_array, shared with interrupt.c */
.p2align 5, 0x0
sos_exception_wrapper_array:
	.irp id, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15, \
	         16,17,18,19,20,21,22,23,24,25,26,27,29,30,31
	  .long (sos_exception_wrapper_\id)
	.endr

/* Alternate stack for double fault handler */
.bss
.p2align 2, 0x0
.size double_fault_alternate_stack, ALTERNATE_DOUBLE_FAULT_STACK_SIZE
double_fault_alternate_stack:
	.fill ALTERNATE_DOUBLE_FAULT_STACK_SIZE, 1, 0x0

