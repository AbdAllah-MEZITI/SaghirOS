/* Copyright (C) 2004 David Decotigny

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
#ifndef _SOS_KSYNCH_H_
#define _SOS_KSYNCH_H_


/**
 * @file synch.h
 *
 * Common kernel synchronisation primitives.
 */


#include <os/errno.h>
#include <os/kwaitq.h>


/* ====================================================================
 * Kernel semaphores, NON-recursive
 */


/**
 * The structure of a (NON-RECURSIVE) kernel Semaphore
 */
struct sos_ksema
{
  int value;
  struct sos_kwaitq kwaitq;
};


/*
 * Initialize a kernel semaphore structure with the given name
 *
 * @param name Name of the semaphore (for debugging purpose only; safe
 * [deep copied])
 *
 * @param initial_value The value of the semaphore before any up/down
 */
sos_ret_t sos_ksema_init(struct sos_ksema *sema, const char *name,
			 int initial_value);


/*
 * De-initialize a kernel semaphore
 *
 * @return -SOS_EBUSY when semaphore could not be de-initialized
 * because at least a thread is in the waitq.
 */
sos_ret_t sos_ksema_dispose(struct sos_ksema *sema);


/*
 * Enters the semaphore
 *
 * @param timeout Maximum time to wait for the semaphore. Or NULL for
 * "no limit". Updated on return to reflect the time remaining (0 when
 * timeout has been triggered)
 *
 * @return -SOS_EINTR when timeout was triggered or when another waitq
 * woke us up.
 *
 * @note This is a BLOCKING FUNCTION
 */
sos_ret_t sos_ksema_down(struct sos_ksema *sema,
			 struct sos_time *timeout);


/*
 * Try to enter the semaphore without blocking.
 *
 * @return -SOS_EBUSY when locking the semaphore would block
 */
sos_ret_t sos_ksema_trydown(struct sos_ksema *sema);


/**
 * Increments the semaphore's value, eventually waking up a thread
 */
sos_ret_t sos_ksema_up(struct sos_ksema *sema);



/* ====================================================================
 * Kernel mutex (ie binary semaphore with strong ownership),
 * NON-recursive !
 */


/**
 * The structure of a (NON-RECURSIVE) kernel Mutex
 */
struct sos_kmutex
{
  struct sos_thread  *owner;
  struct sos_kwaitq  kwaitq;
};


/*
 * Initialize a kernel mutex structure with the given name
 *
 * @param name Name of the mutex (for debugging purpose only; safe
 * [deep copied])
 *
 * @param initial_value The value of the mutex before any up/down
 */
sos_ret_t sos_kmutex_init(struct sos_kmutex *mutex, const char *name);


/*
 * De-initialize a kernel mutex
 *
 * @return -SOS_EBUSY when mutex could not be de-initialized
 * because at least a thread is in the waitq.
 */
sos_ret_t sos_kmutex_dispose(struct sos_kmutex *mutex);


/*
 * Lock the mutex. If the same thread multiply locks the same mutex,
 * it won't hurt (no deadlock, return value = -SOS_EBUSY).
 *
 * @param timeout Maximum time to wait for the mutex. Or NULL for "no
 * limit". Updated on return to reflect the time remaining (0 when
 * timeout has been triggered)
 *
 * @return -SOS_EINTR when timeout was triggered or when another waitq
 * woke us up, -SOS_EBUSY when the thread already owns the mutex.
 *
 * @note This is a BLOCKING FUNCTION
 */
sos_ret_t sos_kmutex_lock(struct sos_kmutex *mutex,
			  struct sos_time *timeout);


/*
 * Try to lock the mutex without blocking.
 *
 * @return -SOS_EBUSY when locking the mutex would block
 */
sos_ret_t sos_kmutex_trylock(struct sos_kmutex *mutex);


/**
 * Unlock the mutex, eventually waking up a thread
 *
 * @return -SOS_EPERM when the calling thread is NOT the owner of the
 * mutex
 */
sos_ret_t sos_kmutex_unlock(struct sos_kmutex *mutex);


#endif /* _SOS_KSYNCH_H_ */
