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
#include "i8259.h"

#include "irq.h"

/* array of IRQ wrappers, defined in irq_wrappers.S */
extern sos_vaddr_t sos_irq_wrapper_array[SOS_IRQ_NUM];

/* arrays of IRQ handlers, shared with irq_wrappers.S */
sos_irq_handler_t sos_irq_handler_array[SOS_IRQ_NUM] = { NULL, };


sos_ret_t sos_irq_setup(void)
{
  return sos_i8259_setup();
}


sos_ret_t sos_irq_set_routine(int irq_level,
			      sos_irq_handler_t routine)
{
  sos_ret_t retval;
  sos_ui32_t flags;
  
  if ((irq_level < 0) || (irq_level >= SOS_IRQ_NUM))
    return -SOS_EINVAL;
  
  sos_disable_IRQs(flags);

  retval = SOS_OK;

  /* Set the irq routine to be called by the IRQ wrapper */
  sos_irq_handler_array[irq_level] = routine;

  /* If the irq is to be enabled, update the IDT with the IRQ
     wrapper */
  if (routine != NULL)
    {
      retval
	= sos_idt_set_handler(SOS_IRQ_BASE + irq_level,
			      (sos_vaddr_t) sos_irq_wrapper_array[irq_level],
			      0 /* CPL0 routine */);
      /* A problem occured */
      if (retval != SOS_OK)
	sos_irq_handler_array[irq_level] = NULL;
    }
  else /* Disable this idt entry */
    {
      retval
	= sos_idt_set_handler(SOS_IRQ_BASE + irq_level,
			      (sos_vaddr_t)NULL /* Disable IDTE */,
			      0  /* Don't care */);
    }

  /* Update the PIC only if an IRQ handler has been set */
  if (sos_irq_handler_array[irq_level] != NULL)
    sos_i8259_enable_irq_line(irq_level);
  else
    sos_i8259_disable_irq_line(irq_level);
    
  sos_restore_IRQs(flags);
  return retval;
}


sos_irq_handler_t sos_irq_get_routine(int irq_level)
{
  if ((irq_level < 0) || (irq_level >= SOS_IRQ_NUM))
    return NULL;
  
  /* Expected to be atomic */
  return sos_irq_handler_array[irq_level];
}
