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
#include "idt.h"
#include "irq.h"

#include <os/assert.h>
#include "exception.h"

/* array of exception wrappers, defined in exception_wrappers.S */
extern sos_vaddr_t sos_exception_wrapper_array[SOS_EXCEPT_NUM];

/* arrays of exception handlers, shared with exception_wrappers.S */
sos_exception_handler_t sos_exception_handler_array[SOS_EXCEPT_NUM] =
  { NULL, };

/* List of exception names for the x86 architecture */
static const char * sos_x86_exnames[] = {
  [SOS_EXCEPT_DIVIDE_ERROR]                = "Division by zero",
  [SOS_EXCEPT_DEBUG]                       = "Debug",
  [SOS_EXCEPT_NMI_INTERRUPT]               = "Non Maskable Interrupt",
  [SOS_EXCEPT_BREAKPOINT]                  = "Breakpoint",
  [SOS_EXCEPT_OVERFLOW]                    = "Overflow",
  [SOS_EXCEPT_BOUND_RANGE_EXCEDEED]        = "Bound Range Exceeded",
  [SOS_EXCEPT_INVALID_OPCODE]              = "Invalid Opcode",
  [SOS_EXCEPT_DEVICE_NOT_AVAILABLE]        = "Device Unavailable",
  [SOS_EXCEPT_DOUBLE_FAULT]                = "Double Fault",
  [SOS_EXCEPT_COPROCESSOR_SEGMENT_OVERRUN] = "Coprocessor Segment Overrun",
  [SOS_EXCEPT_INVALID_TSS]                 = "Invalid TSS",
  [SOS_EXCEPT_SEGMENT_NOT_PRESENT]         = "Segment Not Present",
  [SOS_EXCEPT_STACK_SEGMENT_FAULT]         = "Stack Segfault",
  [SOS_EXCEPT_GENERAL_PROTECTION]          = "General Protection",
  [SOS_EXCEPT_PAGE_FAULT]                  = "Page Fault",
  [SOS_EXCEPT_INTEL_RESERVED_1]            = "INTEL1",
  [SOS_EXCEPT_FLOATING_POINT_ERROR]        = "FP Error",
  [SOS_EXCEPT_ALIGNEMENT_CHECK]            = "Alignment Check",
  [SOS_EXCEPT_MACHINE_CHECK]               = "Machine Check",
  [SOS_EXCEPT_INTEL_RESERVED_2]            = "INTEL2",
  [SOS_EXCEPT_INTEL_RESERVED_3]            = "INTEL3",
  [SOS_EXCEPT_INTEL_RESERVED_4]            = "INTEL4",
  [SOS_EXCEPT_INTEL_RESERVED_5]            = "INTEL5",
  [SOS_EXCEPT_INTEL_RESERVED_6]            = "INTEL6",
  [SOS_EXCEPT_INTEL_RESERVED_7]            = "INTEL7",
  [SOS_EXCEPT_INTEL_RESERVED_8]            = "INTEL8",
  [SOS_EXCEPT_INTEL_RESERVED_9]            = "INTEL9",
  [SOS_EXCEPT_INTEL_RESERVED_10]           = "INTEL10",
  [SOS_EXCEPT_INTEL_RESERVED_11]           = "INTEL11",
  [SOS_EXCEPT_INTEL_RESERVED_12]           = "INTEL12",
  [SOS_EXCEPT_INTEL_RESERVED_13]           = "INTEL13",
  [SOS_EXCEPT_INTEL_RESERVED_14]           = "INTEL14"
};


/* Catch-all exception handler */
static void sos_generic_ex(int exid, const struct sos_cpu_state *ctxt)
{
  const char *exname = sos_exception_get_name(exid);

  sos_display_fatal_error("Exception %s in Kernel at instruction 0x%x (info=%x)!\n",
			  exname,
			  sos_cpu_context_get_PC(ctxt),
			  (unsigned)sos_cpu_context_get_EX_info(ctxt));
}


sos_ret_t sos_exception_subsystem_setup(void)
{
  sos_ret_t retval;
  int exid;

  /* Setup the generic exception handler by default for everybody
     except for the double fault exception */
  for (exid = 0 ; exid < SOS_EXCEPT_NUM ; exid ++)
    {
      /* Skip double fault (see below) */
      if (exid == SOS_EXCEPT_DOUBLE_FAULT)
	continue;

      retval = sos_exception_set_routine(exid, sos_generic_ex);
      if (SOS_OK != retval)
	return retval;
    }

  /* We indicate that the double fault exception handler is defined,
     and give its address. this handler is a do-nothing handler (see
     exception_wrappers.S), and it can NOT be overriden by the
     functions below */
  return sos_idt_set_handler(SOS_EXCEPT_BASE + SOS_EXCEPT_DOUBLE_FAULT,
			    (sos_vaddr_t) sos_exception_wrapper_array[SOS_EXCEPT_DOUBLE_FAULT],
			    0 /* CPL0 routine */);
}


sos_ret_t sos_exception_set_routine(int exception_number,
				    sos_exception_handler_t routine)
{
  sos_ret_t retval;
  sos_ui32_t flags;
  
  if ((exception_number < 0) || (exception_number >= SOS_EXCEPT_NUM))
    return -SOS_EINVAL;

  /* Double fault not supported */
  if (exception_number == SOS_EXCEPT_DOUBLE_FAULT)
    return -SOS_ENOSUP;
  
  sos_disable_IRQs(flags);

  retval = SOS_OK;

  /* Set the exception routine to be called by the exception wrapper */
  sos_exception_handler_array[exception_number] = routine;

  /* If the exception is to be enabled, update the IDT with the exception
     wrapper */
  if (routine != NULL)
    retval
      = sos_idt_set_handler(SOS_EXCEPT_BASE + exception_number,
			    (sos_vaddr_t) sos_exception_wrapper_array[exception_number],
			    0 /* CPL0 routine */);
  else /* Disable the IDT entry */
    retval
      = sos_idt_set_handler(SOS_EXCEPT_BASE + exception_number,
			    (sos_vaddr_t)NULL /* No routine => disable IDTE */,
			    0 /* don't care */);

  sos_restore_IRQs(flags);
  return retval;
}


sos_exception_handler_t sos_exception_get_routine(int exception_number)
{
  if ((exception_number < 0) || (exception_number >= SOS_EXCEPT_NUM))
    return NULL;

  /* Double fault not supported */
  if (exception_number == SOS_EXCEPT_DOUBLE_FAULT)
    return NULL;
  
  /* Expected to be atomic */
  return sos_exception_handler_array[exception_number];
}


const char * sos_exception_get_name(int exception_number)
{
  if ((exception_number < 0) || (exception_number >= SOS_EXCEPT_NUM))
    return NULL;

  return sos_x86_exnames[exception_number];
}

