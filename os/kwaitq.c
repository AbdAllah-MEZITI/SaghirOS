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

#include <lib/klibc.h>
#include <os/list.h>
#include <os/assert.h>
#include <hwcore/irq.h>

#include "kwaitq.h"


sos_ret_t sos_kwaitq_init(struct sos_kwaitq *kwq,
			  const char *name)
{
  memset(kwq, 0x0, sizeof(struct sos_kwaitq));

#ifdef SOS_KWQ_DEBUG
  if (! name)
    name = "<unknown>";
  strzcpy(kwq->name, name, SOS_KWQ_DEBUG_MAX_NAMELEN);
#endif
  list_init_named(kwq->waiting_list,
		  prev_entry_in_kwaitq, next_entry_in_kwaitq);

  return SOS_OK;
}


sos_ret_t sos_kwaitq_dispose(struct sos_kwaitq *kwq)
{
  sos_ui32_t flags;
  sos_ret_t retval;

  sos_disable_IRQs(flags);
  if (list_is_empty_named(kwq->waiting_list,
			  prev_entry_in_kwaitq, next_entry_in_kwaitq))
    retval = SOS_OK;
  else
    retval = -SOS_EBUSY;

  sos_restore_IRQs(flags);
  return retval;
}


sos_bool_t sos_kwaitq_is_empty(const struct sos_kwaitq *kwq)
{
  sos_ui32_t flags;
  sos_ret_t retval;

  sos_disable_IRQs(flags);
  retval = list_is_empty_named(kwq->waiting_list,
			       prev_entry_in_kwaitq, next_entry_in_kwaitq);

  sos_restore_IRQs(flags);
  return retval;  
}


sos_ret_t sos_kwaitq_init_entry(struct sos_kwaitq_entry *kwq_entry)
{
  memset(kwq_entry, 0x0, sizeof(struct sos_kwaitq_entry));
  kwq_entry->thread = sos_thread_get_current();
  return SOS_OK;
}


/** Internal helper function equivalent to sos_kwaitq_add_entry(), but
    without interrupt protection scheme, and explicit priority
    ordering */
inline static sos_ret_t _kwaitq_add_entry(struct sos_kwaitq *kwq,
					  struct sos_kwaitq_entry *kwq_entry)
{
  /* This entry is already added in the kwaitq ! */
  SOS_ASSERT_FATAL(NULL == kwq_entry->kwaitq);

  /* sos_kwaitq_init_entry() has not been called ?! */
  SOS_ASSERT_FATAL(NULL != kwq_entry->thread);

  /* (Re-)Initialize wakeup status of the entry */
  kwq_entry->wakeup_triggered = FALSE;
  kwq_entry->wakeup_status    = SOS_OK;

  /* Add the thread in the list */
  list_add_tail_named(kwq->waiting_list, kwq_entry,
		      prev_entry_in_kwaitq, next_entry_in_kwaitq);

  /* Update the list of waitqueues for the thread */
  list_add_tail_named(kwq_entry->thread->kwaitq_list, kwq_entry,
		      prev_entry_for_thread, next_entry_for_thread);

  kwq_entry->kwaitq = kwq;

  return SOS_OK;
}


sos_ret_t sos_kwaitq_add_entry(struct sos_kwaitq *kwq,
			       struct sos_kwaitq_entry *kwq_entry)
{
  sos_ui32_t flags;
  sos_ret_t retval;

  sos_disable_IRQs(flags);
  retval = _kwaitq_add_entry(kwq, kwq_entry);
  sos_restore_IRQs(flags);

  return retval;
}


/** Internal helper function equivalent to sos_kwaitq_remove_entry(),
    but without interrupt protection scheme */
inline static sos_ret_t
_kwaitq_remove_entry(struct sos_kwaitq *kwq,
		     struct sos_kwaitq_entry *kwq_entry)
{
  SOS_ASSERT_FATAL(kwq_entry->kwaitq == kwq);

  list_delete_named(kwq->waiting_list, kwq_entry,
		    prev_entry_in_kwaitq, next_entry_in_kwaitq);

  list_delete_named(kwq_entry->thread->kwaitq_list, kwq_entry,
		    prev_entry_for_thread, next_entry_for_thread);

  kwq_entry->kwaitq = NULL;
  return SOS_OK;
}


sos_ret_t sos_kwaitq_remove_entry(struct sos_kwaitq *kwq,
				  struct sos_kwaitq_entry *kwq_entry)
{
  sos_ui32_t flags;
  sos_ret_t retval;

  sos_disable_IRQs(flags);
  retval = _kwaitq_remove_entry(kwq, kwq_entry);
  sos_restore_IRQs(flags);

  return retval;
}


sos_ret_t sos_kwaitq_wait(struct sos_kwaitq *kwq,
			  struct sos_time *timeout)
{
  sos_ui32_t flags;
  sos_ret_t retval;
  struct sos_kwaitq_entry kwq_entry;

  sos_kwaitq_init_entry(& kwq_entry);

  sos_disable_IRQs(flags);

  retval = _kwaitq_add_entry(kwq, & kwq_entry);

  /* Wait for wakeup or timeout */
  sos_thread_sleep(timeout);
  /* Woken up ! */

  /* Sleep delay elapsed ? */
  if (! kwq_entry.wakeup_triggered)
    {
      /* Yes (timeout occured, or wakeup on another waitqueue): remove
	 the waitq entry by ourselves */
      _kwaitq_remove_entry(kwq, & kwq_entry);
      retval = -SOS_EINTR;
    }
  else
    {
      retval = kwq_entry.wakeup_status;
    }
  
  sos_restore_IRQs(flags);

  /* We were correctly awoken: position return status */
  return retval;
}


sos_ret_t sos_kwaitq_wakeup(struct sos_kwaitq *kwq,
			    unsigned int nb_threads,
			    sos_ret_t wakeup_status)
{
  sos_ui32_t flags;

  sos_disable_IRQs(flags);

  /* Wake up as much threads waiting in waitqueue as possible (up to
     nb_threads), scanning the list in FIFO order */
  while (! list_is_empty_named(kwq->waiting_list,
			       prev_entry_in_kwaitq, next_entry_in_kwaitq))
    {
      struct sos_kwaitq_entry *kwq_entry
	= list_get_head_named(kwq->waiting_list,
			      prev_entry_in_kwaitq, next_entry_in_kwaitq);

      /* Enough threads woken up ? */
      if (nb_threads <= 0)
	break;

      /*
       * Ok: wake up the thread for this entry
       */

      /* Thread already woken up ? */
      if (SOS_THR_RUNNING == sos_thread_get_state(kwq_entry->thread))
	{
	  /* Yes => Do nothing because WE are that woken-up thread. In
	     particular: don't call set_ready() here because this
	     would result in an inconsistent configuration (currently
	     running thread marked as "waiting for CPU"...). */
	  continue;
	}
      else
	{
	  /* No => wake it up now. */
	  sos_sched_set_ready(kwq_entry->thread);
	}

      /* Remove this waitq entry */
      _kwaitq_remove_entry(kwq, kwq_entry);
      kwq_entry->wakeup_triggered = TRUE;
      kwq_entry->wakeup_status    = wakeup_status;

      /* Next iteration... */
      nb_threads --;
    }

  sos_restore_IRQs(flags);

  return SOS_OK;
}
