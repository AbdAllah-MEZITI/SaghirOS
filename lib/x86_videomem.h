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
#ifndef _SOS_X86_VIDEOMEM_H_
#define _SOS_X86_VIDEOMEM_H_

/**
 * @file x86_videomem.h
 *
 * On x86 PC platforms, the text mode screen memory (and CGA/EGA/VGA
 * too) is mapped into physical memory. This file handles access to
 * this screen, supposed to be set in text-mode, through this memory
 * area. All the functions below print the characters directly to the
 * memory, without interpreting the escaped characters (such as \n,
 * \r...)
 */

#include <os/errno.h>

/**
 * x86 video attributes
 * See http://webster.cs.ucr.edu/AoA/DOS/ch23/CH23-1.html
 */
/* Normal and Dark/Light foreground */
#define SOS_X86_VIDEO_FG_BLACK     0
#define SOS_X86_VIDEO_FG_DKGRAY    8
#define SOS_X86_VIDEO_FG_BLUE      1
#define SOS_X86_VIDEO_FG_LTBLUE    9
#define SOS_X86_VIDEO_FG_GREEN     2
#define SOS_X86_VIDEO_FG_LTGREEN   10
#define SOS_X86_VIDEO_FG_CYAN      3
#define SOS_X86_VIDEO_FG_LTCYAN    11
#define SOS_X86_VIDEO_FG_RED       4
#define SOS_X86_VIDEO_FG_LTRED     12
#define SOS_X86_VIDEO_FG_MAGENTA   5
#define SOS_X86_VIDEO_FG_LTMAGENTA 13
#define SOS_X86_VIDEO_FG_BROWN     6
#define SOS_X86_VIDEO_FG_YELLOW    14
#define SOS_X86_VIDEO_FG_LTGRAY    7
#define SOS_X86_VIDEO_FG_WHITE     15
/* Background */
#define SOS_X86_VIDEO_BG_BLACK     (0 << 4)
#define SOS_X86_VIDEO_BG_BLUE      (1 << 4)
#define SOS_X86_VIDEO_BG_GREEN     (2 << 4)
#define SOS_X86_VIDEO_BG_CYAN      (3 << 4)
#define SOS_X86_VIDEO_BG_RED       (4 << 4)
#define SOS_X86_VIDEO_BG_MAGENTA   (5 << 4)
#define SOS_X86_VIDEO_BG_BROWN     (6 << 4)
#define SOS_X86_VIDEO_BG_LTGRAY    (7 << 4)
/* Blinking */
#define SOS_X86_VIDEO_FG_BLINKING  (1 << 7)


/** Setup the video RAM mapping and clear the screen */
sos_ret_t sos_x86_videomem_setup(void);

/** Clears the screen and set the background color as given by
    attribute */
sos_ret_t sos_x86_videomem_cls(unsigned char attribute);

/** Print the string on the scren with the given attribute. Does not
    handle scrolling */
sos_ret_t sos_x86_videomem_putstring(unsigned char row, unsigned char col,
				     unsigned char attribute,
				     const char *str);

/** Print the character on the scren with the given attribute. Does not
    handle scrolling */
sos_ret_t sos_x86_videomem_putchar(unsigned char row, unsigned char col,
				   unsigned char attribute,
				   unsigned char c);

/**
 * Print the formatted string. Very restricted version of printf(3):
 * 1/ can print max 255 chars, 2/ supports only %d/%i, %c, %s, %x
 * without any support for flag charachters (eg %08x).
 */
sos_ret_t sos_x86_videomem_printf(unsigned char row, unsigned char col,
				  unsigned char attribute,
				  const char *format, /* args */...)
     __attribute__ ((format (printf, 4, 5)));

#endif /* _SOS_X86_VIDEOMEM_H_ */
