/* Copyright (C) 2004  David Decotigny (with INSA Rennes for vsnprintf)
   Copyright (C) 2003  The KOS Team
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
#include "klibc.h"

/* For an optimized version, see BSD sources ;) */
void *memcpy(void *dst0, const void *src0, register unsigned int size)
{
  char *dst;
  const char *src;
  for (dst = (char*)dst0, src = (const char*)src0 ;
       size > 0 ;
       dst++, src++, size--)
    *dst = *src;
  return dst0;
}

/* ditto */
void *memset(void *dst0, register int c, register unsigned int length)
{
  char *dst;
  for (dst = (char*) dst0 ;
       length > 0 ;
       dst++, length --)
    *dst = (char)c;
  return dst0;
}

int memcmp(const void *s1, const void *s2, sos_size_t len)
{
  const unsigned char *c1, *c2;
  unsigned int i;
 
  for (i = 0, c1 = s1, c2 = s2; i < len; i++, c1++, c2++)
    {
      if(*c1 != *c2)
        return *c1 - *c2;
    }
 
  return 0;
}


unsigned int strlen(register const char *str)
{
  unsigned int retval = 0;
  
  while (*str++)
    retval++;
  
  return retval;
}


unsigned int strnlen(const char * s, sos_size_t count)
{
  const char *sc;
  
  for (sc = s; count-- && *sc != '\0'; ++sc)
    /* nothing */continue;

  return sc - s;
}


char *strzcpy(register char *dst, register const char *src, register int len)
{
  int i;

  if (len <= 0)
    return dst;
  
  for (i = 0; i < len; i++)
    {
      dst[i] = src[i];
      if(src[i] == '\0')
        return dst;
    }
  
  dst[len-1] = '\0'; 
  return dst;
}


char *strzcat (char *dest, const char *src, sos_size_t n)
{
  char *res = dest;
  
  for ( ; *dest ; dest++);
  
  for ( ; *src ; src++, dest++) {
    *dest = *src;
    n--;
    if (n <= 0)
      break;
  }

  *dest = '\0';
  return res;
}

int strcmp(register const char *s1, register const char *s2)
{
  while (*s1 == *s2++)
    if (*s1++ == 0)
      return (0);
  
  return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
}


int strncmp(register const char *s1, register const char *s2, register int len)
{
  char c1 = '\0', c2 = '\0';
  
  while (len > 0)
    {
      c1 = (unsigned char) *s1++;
      c2 = (unsigned char) *s2++;
      if (c1 == '\0' || c1 != c2)
        return c1 - c2;
      len--;
    }
  
  return c1 - c2;
}


/* I (d2) borrowed and rewrote this for Nachos/INSA Rennes. Thanks to
   them for having kindly allowed me to do so. */
int vsnprintf(char *buff, sos_size_t len, const char * format, va_list ap)
{
  sos_size_t i, result;
  
  if (!buff || !format || (len < 0))
    return -1;
  
#define PUTCHAR(thechar) \
  do { \
    if (result < len-1) \
      *buff++ = (thechar);  \
    result++; \
  } while (0)
  
  result = 0;
  for(i=0 ; format[i] != '\0' ; i++){
    switch (format[i])
      {
      case '%':
	i++;
	switch(format[i])
	  {
	  case '%':
	    {
	      PUTCHAR('%');
	      break;
	    }
	  case 'i':;
	  case 'd':
	    {
	      int integer = va_arg(ap,int);
	      int cpt2 = 0;
	      char buff_int[16];
	      
	      if (integer<0)
		PUTCHAR('-');
	      /* Ne fait pas integer = -integer ici parce que INT_MIN
		 n'a pas d'equivalent positif (int = [-2^31, 2^31-1]) */
	      
	      do {
		int m10 = integer%10;
		m10 = (m10 < 0)? -m10:m10;
		buff_int[cpt2++]=(char)('0'+ m10);
		integer=integer/10;
	      } while(integer!=0);
	      
	      for(cpt2 = cpt2 - 1 ; cpt2 >= 0 ; cpt2--)
		PUTCHAR(buff_int[cpt2]);
	      
	      break;
	    }
	    
	  case 'c':
	    {
	      int value = va_arg(ap,int);
	      PUTCHAR((char)value);
	      break;
	    }
	    
	  case 's':
	    {
	      char *string = va_arg(ap,char *);
	      if (! string)
		string = "(null)";
	      for( ; *string != '\0' ; string++)
		PUTCHAR(*string);
	      break;
	    }

	  case 'x':
	    {
	      unsigned int hexa = va_arg(ap,int);
	      unsigned int nb;
	      int i, had_nonzero = 0;
	      for(i=0 ; i < 8 ; i++)
		{
		  nb = (unsigned int)(hexa << (i*4));
		  nb = (nb >> 28) & 0xf;
		  // Skip the leading zeros
		  if (nb == 0)
		    {
		      if (had_nonzero)
			PUTCHAR('0');
		    }
		  else
		    {
		      had_nonzero = 1;
		      if (nb < 10)
			PUTCHAR('0'+nb);
		      else
			PUTCHAR('a'+(nb-10));
		    }
		}
	      if (! had_nonzero)
		PUTCHAR('0');
	      break;
	    }
	    break;
	
	  default:
            PUTCHAR('%');
	    PUTCHAR(format[i]);
	  }
	break;
	
      default:
        PUTCHAR(format[i]);
      }
  }
  
  *buff = '\0';
  return result;
}


int snprintf(char * buff, sos_size_t len, const char *format, ...)
{
  va_list ap;
 
  va_start(ap, format);
  len = vsnprintf(buff, len, format, ap);
  va_end(ap);
 
  return len;
}
