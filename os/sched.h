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
#ifndef _SOS_SCHED_H_
#define _SOS_SCHED_H_


/**
 * @file sched.h
 *
 * A basic scheduler with simple FIFO threads' ordering.
 *
 * The functions below manage CPU queues, and are NEVER responsible
 * for context switches (see thread.h for that) or synchronizations
 * (see kwaitq.h or the higher levels primitives [mutex, semaphore,
 * ...] for that).
 *
 * @note IMPORTANT: all the functions below are meant to be called
 * ONLY by the thread/timer/kwaitq subsystems. DO NOT use them
 * directly from anywhere else: use ONLY the thread/kwaitq functions!
 * If you still want to call them directly despite this disclaimer,
 * simply disable interrupts before clling them.
 */

#include <os/errno.h>


#include <os/thread.h>


/**
 * Initialize the scheduler
 *
 * @note: The use of this function is RESERVED
 */
sos_ret_t sos_sched_subsystem_setup();


/**
 * Mark the given thread as ready
 *
 * @note: The use of this function is RESERVED
 */
sos_ret_t sos_sched_set_ready(struct sos_thread * thr);


/**
 * Return the identifier of the next thread to run. Also removes it
 * from the ready list, but does NOT set is as current_thread !
 *
 * @param current_thread TCB of the thread calling the function
 *
 * @param do_yield When TRUE, put the current executing thread at the
 * end of the ready list. Otherwise it is kept at the head of it.
 *
 * @note: The use of this function is RESERVED
 */
struct sos_thread * sos_reschedule(struct sos_thread * current_thread,
				    sos_bool_t do_yield);

#endif /* _SOS_WAITQUEUE_H_ */
