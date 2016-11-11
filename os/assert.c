/* Copyright (C) 2004  The KOS Team

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

#include <lib/klibc.h>
//#include <drivers/bochs.h>
#include <lib/x86_videomem.h>

#include "assert.h"

void sos_display_fatal_error(const char *format, /* args */...)
{
  char buff[256];
  va_list ap;
  
  asm("cli\n"); /* disable interrupts -- x86 only */ \

  va_start(ap, format);
  vsnprintf(buff, sizeof(buff), format, ap);
  va_end(ap);

  /* sos_bochs_putstring(buff); sos_bochs_putstring("\n");

  sos_x86_videomem_putstring(23, 0,
			     SOS_X86_VIDEO_BG_BLACK
			     | SOS_X86_VIDEO_FG_LTRED , buff);*/
  printf("%s\n",buff);
  os_printf (23, 0, SOS_X86_VIDEO_BG_BLACK | SOS_X86_VIDEO_FG_LTRED, "%s", buff);

  /* Infinite loop: processor halted */
  for ( ; ; )
    asm("hlt\n");
}
