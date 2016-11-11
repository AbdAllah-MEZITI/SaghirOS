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

#include <os/errno.h>
#include <lib/klibc.h>
#include <os/assert.h>
#include <os/list.h>

#include "sched.h"


/**
 * The definition of the scheduler queue. We could have used a normal
 * kwaitq here, it would have had the same properties. But, in the
 * definitive version (O(1) scheduler), the structure has to be a bit
 * more complicated. So, in order to keep the changes as small as
 * possible between this version and the definitive one, we don't use
 * kwaitq here.
 */
static struct
{
  unsigned int nr_threads;
  struct sos_thread *thread_list;
} ready_queue;


sos_ret_t sos_sched_subsystem_setup()
{
  memset(& ready_queue, 0x0, sizeof(ready_queue));

  return SOS_OK;
}


/**
 * Helper function to add a thread in a ready queue AND to change the
 * state of the given thread to "READY".
 *
 * @param insert_at_tail TRUE to tell to add the thread at the end of
 * the ready list. Otherwise it is added at the head of it.
 */
static sos_ret_t add_in_ready_queue(struct sos_thread *thr,
				    sos_bool_t insert_at_tail)
{

  SOS_ASSERT_FATAL( (SOS_THR_CREATED == thr->state)
		    || (SOS_THR_RUNNING == thr->state) /* Yield */
		    || (SOS_THR_BLOCKED == thr->state) );

  /* Add the thread to the CPU queue */
  if (insert_at_tail)
    list_add_tail_named(ready_queue.thread_list, thr,
			ready.rdy_prev, ready.rdy_next);
  else
    list_add_head_named(ready_queue.thread_list, thr,
			ready.rdy_prev, ready.rdy_next);
  ready_queue.nr_threads ++;

  /* Ok, thread is now really ready to be (re)started */
  thr->state = SOS_THR_READY;

  return SOS_OK;
}


sos_ret_t sos_sched_set_ready(struct sos_thread *thr)
{
  sos_ret_t retval;

  /* Don't do anything for already ready threads */
  if (SOS_THR_READY == thr->state)
    return SOS_OK;

  /* Real-time thread: schedule it for the present turn */
  retval = add_in_ready_queue(thr, TRUE);

  return retval;
}


struct sos_thread * sos_reschedule(struct sos_thread *current_thread,
				   sos_bool_t do_yield)
{

  if (SOS_THR_ZOMBIE == current_thread->state)
    {
      /* Don't think of returning to this thread since it is
	 terminated */
      /* Nop */
    }
  else if (SOS_THR_BLOCKED != current_thread->state)
    {
      /* Take into account the current executing thread unless it is
	 marked blocked */
      if (do_yield)
	/* Ok, reserve it for next turn */
	add_in_ready_queue(current_thread, TRUE);
      else
	/* Put it at the head of the active list */
	add_in_ready_queue(current_thread, FALSE);
    }

  /* The next thread is that at the head of the ready list */
  if (ready_queue.nr_threads > 0)
    {
      struct sos_thread *next_thr;

      /* Queue is not empty: take the thread at its head */
      next_thr = list_pop_head_named(ready_queue.thread_list,
				     ready.rdy_prev, ready.rdy_next);
      ready_queue.nr_threads --;

      return next_thr;
    }

  SOS_FATAL_ERROR("No kernel thread ready ?!");
  return NULL;
}
