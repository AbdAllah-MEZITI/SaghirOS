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
#ifndef _SOS_ERRNO_H_
#define _SOS_ERRNO_H_

/**
 * @file errno.h
 *
 * SOS return value codes and errors.
 */

/* Positive values of the error codes */
#define SOS_OK     0   /* No error */
#define SOS_EINVAL 1   /* Invalid argument */
#define SOS_ENOSUP 2   /* Operation not supported */
#define SOS_EFATAL 255 /* Internal fatal error */

/* A negative value means that an error occured.  For
 *  example -SOS_EINVAL means that the error was "invalid
 * argument" */
typedef int sos_ret_t;

#endif /* _SOS_ERRNO_H_ */
