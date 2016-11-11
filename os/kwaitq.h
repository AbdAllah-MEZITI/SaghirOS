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
#ifndef _SOS_KWAITQ_H_
#define _SOS_KWAITQ_H_

#include <os/errno.h>
#include <os/thread.h>
#include <os/time.h>
#include <os/sched.h>


/**
 * @kwaitq.h
 *
 * Low-level functions to manage queues of threads waiting for a
 * resource. These functions are public. For higher-level
 * synchronization primitives such as mutex, semaphores, conditions,
 * ... prefer looking at the corresponding libraries.
 */


/**
 * Define this if you want to know the names of the kwaitq
 */
// #define SOS_KWQ_DEBUG


/* Forward declaration */
struct sos_kwaitq_entry;


/**
 * Definition of a waitqueue. In a kwaitq, the threads are ordererd in
 * FIFO order.
 */
struct sos_kwaitq
{
#ifdef SOS_KWQ_DEBUG
# define SOS_KWQ_DEBUG_MAX_NAMELEN 32
  char name[SOS_KWQ_DEBUG_MAX_NAMELEN];
#endif
  struct sos_kwaitq_entry *waiting_list;
};


/**
 * Definition of an entry for a thread waiting in the waitqueue
 */
struct sos_kwaitq_entry
{
  /** The thread associted with this entry */
  struct sos_thread *thread;

  /** The kwaitqueue this entry belongs to */
  struct sos_kwaitq *kwaitq;

  /** TRUE when somebody woke up this entry */
  sos_bool_t wakeup_triggered;

  /** The status of wakeup for this entry. @see wakeup_status argument
      of sos_kwaitq_wakeup() */
  sos_ret_t wakeup_status;

  /** Other entries in this kwaitqueue */
  struct sos_kwaitq_entry *prev_entry_in_kwaitq, *next_entry_in_kwaitq;

  /** Other entries for the thread */
  struct sos_kwaitq_entry *prev_entry_for_thread, *next_entry_for_thread;  
};


/**
 * Initialize an empty waitqueue.
 *
 * @param name Used only if SOS_KWQ_DEBUG is defined (safe [deep
 * copied])
 */
sos_ret_t sos_kwaitq_init(struct sos_kwaitq *kwq,
			  const char *name);


/**
 * Release a waitqueue, making sure that no thread is in it.
 *
 * @return -SOS_EBUSY in case a thread is still in the waitqueue.
 */
sos_ret_t sos_kwaitq_dispose(struct sos_kwaitq *kwq);


/**
 * Return whether there are no threads in the waitq
 */
sos_bool_t sos_kwaitq_is_empty(const struct sos_kwaitq *kwq);


/**
 * Initialize a waitqueue entry. Mainly consists in updating the
 * "thread" field of the entry (set to current running thread), and
 * initializing the remaining of the entry as to indicate it does not
 * belong to any waitq.
 */
sos_ret_t sos_kwaitq_init_entry(struct sos_kwaitq_entry *kwq_entry);


/**
 * Add an entry (previously initialized with sos_kwaitq_init_entry())
 * in the given waitqueue.
 *
 * @note: No state change/context switch can occur here ! Among other
 * things: the current executing thread is not preempted.
 */
sos_ret_t sos_kwaitq_add_entry(struct sos_kwaitq *kwq,
			       struct sos_kwaitq_entry *kwq_entry);


/**
 * Remove the given kwaitq_entry from the kwaitq.
 *
 * @note: No state change/context switch can occur here ! Among other
 * things: the thread associated with the entry is not necessarilly
 * the same as the one currently running, and does not preempt the
 * current running thread if they are different.
 */
sos_ret_t sos_kwaitq_remove_entry(struct sos_kwaitq *kwq,
				  struct sos_kwaitq_entry *kwq_entry);


/**
 * Helper function to make the current running thread block in the
 * given kwaitq, waiting to be woken up by somedy else or by the given
 * timeout. It calls the sos_kwaitq_add_entry() and
 * sos_kwaitq_remove_entry().
 *
 * @param timeout The desired timeout (can be NULL => wait for
 * ever). It is updated by the function to reflect the remaining
 * timeout in case the thread has been woken-up prior to its
 * expiration.
 *
 * @return -SOS_EINTR when the thread is resumed while it has not be
 * explicitely woken up by someone calling sos_kwaitq_wakeup() upon
 * the same waitqueue... This can only happen 1/ if the timeout
 * expired, or 2/ if the current thread is also in another kwaitq
 * different to "kwq". Otherwise return the value set by
 * sos_kwaitq_wakeup(). The timeout remaining is updated in timeout.
 *
 * @note This is a BLOCKING FUNCTION
 */
sos_ret_t sos_kwaitq_wait(struct sos_kwaitq *kwq,
			  struct sos_time *timeout);


/**
 * Wake up as much as nb_thread threads (SOS_KWQ_WAKEUP_ALL to wake
 * up all threads) in the kwaitq kwq, in FIFO order.
 *
 * @param wakeup_status The value returned by sos_kwaitq_wait() when
 * the thread will effectively woken up due to this wakeup.
 */
sos_ret_t sos_kwaitq_wakeup(struct sos_kwaitq *kwq,
			    unsigned int nb_threads,
			    sos_ret_t wakeup_status);
#define SOS_KWQ_WAKEUP_ALL (~((unsigned int)0))


#endif /* _SOS_KWAITQ_H_ */
