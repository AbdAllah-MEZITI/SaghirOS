/* Copyright (C) 2004  The SOS Team
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
#ifndef _SOS_TYPES_H_
#define _SOS_TYPES_H_

/**
 * @file types.h
 *
 * SOS basic types definition
 */

/** Generic virtual address (kernel or user) */
typedef unsigned int       sos_vaddr_t;

/** Memory size of an object (positive) */
typedef unsigned int       sos_size_t;

/** Low-level sizes */
typedef unsigned long int  sos_ui32_t; /* 32b unsigned */
typedef unsigned short int sos_ui16_t; /* 16b unsigned */
typedef unsigned char      sos_ui8_t;  /* 8b unsigned */

typedef enum { FALSE=0, TRUE } sos_bool_t;

/** Not a proper type, but highly useful with basic type
    manipulations */
#define NULL ((void*)0)

#endif /* _SOS_TYPES_H_ */
