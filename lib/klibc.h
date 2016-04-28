/* Copyright (C) 2003  The KOS Team
   Copyright (C) 1999  Free Software Foundation

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
#ifndef _SOS_KLIBC_H_
#define _SOS_KLIBC_H_

/**
 * @file klibc.h
 *
 * Basic libc-style support for common useful functions (string.h,
 * stdarg.h), some with slight non-standard behavior (see comments).
 */

#include <types.h>

/* string.h functions */

void *memcpy(void *dst, const void *src, register unsigned int size ) ;
void *memset(void *dst, register int c, register unsigned int length ) ;
int memcmp(const void *s1, const void *s2, sos_size_t n);

unsigned int strlen( register const char *str) ;
unsigned int strnlen(const char * s, sos_size_t maxlen);

/**
 * @note Same as strncpy(), with a slightly different semantic.
 * Actually, strncpy(3C) says " The result will not be null-terminated
 * if the length of 'from' is n or more.".  Here, 'dst' is ALWAYS
 * null-terminated. And its total len will ALWAYS be <= len, with
 * null-terminating-char included.
 */
char *strzcpy( register char *dst, register const char *src,
               register int len ) ;

/**
 * @note Same as strncat(), with the same semantic : 'dst' is ALWAYS
 * null-terminated. And its total len will ALWAYS be <= len, with
 * null-terminating-char included.
 */
char *strzcat (char *dest, const char *src,
               const sos_size_t len);
 
int strcmp(register const char *s1, register const char *s2 );
int strncmp(register const char *s1, register const char *s2,
	    register int len );

/* Basic stdarg.h macros. Taken from gcc support files */
#define __GNUC_VA_LIST 
typedef void *__gnuc_va_list;
typedef __gnuc_va_list va_list;
#define __va_rounded_size(TYPE) \
  (((sizeof (TYPE) + sizeof (int) - 1) / sizeof (int)) * sizeof (int))
#define va_start(AP, LASTARG) \
  (AP = ((__gnuc_va_list) __builtin_next_arg (LASTARG)))
#define va_end(AP) \
  ((void)0)
#define va_arg(AP, TYPE) \
  (AP = (__gnuc_va_list) ((char *) (AP) + __va_rounded_size (TYPE)),  \
   *((TYPE *) (void *) ((char *) (AP) - __va_rounded_size (TYPE))))
#define __va_copy(dest, src) \
  (dest) = (src)

/* stdarg.h functions. There might be a non-standard behavior: there
   will always be a trailing '\0' in the resulting string */
int vsnprintf(char *, sos_size_t, const char *, va_list);
int snprintf(char *, sos_size_t, const char *, /*args*/ ...)
  __attribute__ ((format (printf, 3, 4)));

#endif /* _SOS_KLIBC_H_ */
