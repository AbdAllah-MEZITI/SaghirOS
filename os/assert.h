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
#ifndef _SOS_ASSERT_H_
#define _SOS_ASSERT_H_

//#include <drivers/bochs.h>
//#include <drivers/x86_videomem.h>

/**
 * If the expr is FALSE, print a message and halt the machine
 */
#define SOS_ASSERT_FATAL(expr) \
   ({ \
     int __res=(int)(expr); \
     if (! __res) { \
       asm("cli\n"); /* disable interrupts -- x86 only */ \
/*       sos_bochs_printf("%s@%s:%d Assertion " # expr " failed\n", \
			__PRETTY_FUNCTION__, __FILE__, __LINE__); \
*/       os_printf(24, 0, 12, \
                               "%s@%s:%d Assertion " # expr " failed", \
			       __PRETTY_FUNCTION__, __FILE__, __LINE__); \
       for (;;) asm("hlt;") ; /* Infinite loop, ie simple system halt */ \
     } \
   })


#endif /* _SOS_ASSERT_H_ */
