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
#include "ioports.h"

#include "i8259.h"

#define PIC_MASTER 0x20
#define PIC_SLAVE  0xa0

/** Setup the 8259 PIC */
sos_ret_t sos_i8259_subsystem_setup(void)
{
  /* Send ICW1: 8086 mode + NOT Single ctrl + call address
     interval=8 */
  outb(0x11, PIC_MASTER);
  outb(0x11, PIC_SLAVE);

  /* Send ICW2: ctrl base address */
  outb(0x20, PIC_MASTER+1);
  outb(0x28, PIC_SLAVE+1);

  /* Send ICW3 master: mask where slaves are connected */
  outb(0x4, PIC_MASTER+1);
  /* Send ICW3 slave: index where the slave is connected on master */
  outb(0x2, PIC_SLAVE+1);

  /* Send ICW4: 8086 mode, fully nested, not buffered, no implicit EOI */
  outb(0x1, PIC_MASTER+1);
  outb(0x1, PIC_SLAVE+1);

  /* Send OCW1:
   * Closing all IRQs : waiting for a correct handler The only IRQ
   * enabled is the cascade (that's why we use 0xFB for the master) */
  outb(0xFB, PIC_MASTER+1);
  outb(0xFF, PIC_SLAVE+1);

  return SOS_OK;
}


sos_ret_t sos_i8259_enable_irq_line(int numirq)
{
  if(numirq < 8)
    /*  irq on master PIC */
    outb((inb(PIC_MASTER+1) & ~(1 << numirq)), PIC_MASTER+1);
  else
    /*  irq on slave PIC */
    outb((inb(PIC_SLAVE+1) & ~(1 << (numirq-8))), PIC_SLAVE+1);
  
  return SOS_OK;
}


sos_ret_t sos_i8259_disable_irq_line(int numirq)
{
  if(numirq < 8)
    /*  irq on master PIC */
    outb((inb(PIC_MASTER+1) | (1 << numirq)), PIC_MASTER+1);
  else
    /*  irq on slave PIC */
    outb((inb(PIC_SLAVE+1) | (1 << (numirq-8))), PIC_SLAVE+1);

  return SOS_OK;
}
