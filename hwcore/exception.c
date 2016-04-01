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

#include "exception.h"

/* array of exception wrappers, defined in exception_wrappers.S */
extern sos_vaddr_t sos_exception_wrapper_array[SOS_EXCEPT_NUM];

/* arrays of exception handlers, shared with exception_wrappers.S */
sos_exception_handler_t sos_exception_handler_array[SOS_EXCEPT_NUM] =
  { NULL, };

sos_ret_t sos_exceptions_setup(void)
{
  /* We inidicate that the double fault exception handler is defined,
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
