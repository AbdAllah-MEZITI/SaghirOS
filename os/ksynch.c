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


#include <hwcore/irq.h>


#include "ksynch.h"


sos_ret_t sos_ksema_init(struct sos_ksema *sema, const char *name,
			 int initial_value)
{
  sema->value = initial_value;
  return sos_kwaitq_init(& sema->kwaitq, name);
}


sos_ret_t sos_ksema_dispose(struct sos_ksema *sema)
{
  return sos_kwaitq_dispose(& sema->kwaitq);
}


sos_ret_t sos_ksema_down(struct sos_ksema *sema,
			 struct sos_time *timeout)
{
  sos_ui32_t flags;
  sos_ret_t retval;

  sos_disable_IRQs(flags);
  retval = SOS_OK;

  sema->value --;
  if (sema->value < 0)
    {
      /* Wait for somebody to wake us */
      retval = sos_kwaitq_wait(& sema->kwaitq, timeout);

      /* Something wrong happened (timeout, external wakeup, ...) ? */
      if (SOS_OK != retval)
	{
	  /* Yes: pretend we did not ask for the semaphore */
	  sema->value ++;
	}
    }

  sos_restore_IRQs(flags);
  return retval;
}


sos_ret_t sos_ksema_trydown(struct sos_ksema *sema)
{
  sos_ui32_t flags;
  sos_ret_t retval;

  sos_disable_IRQs(flags);

  /* Can we take the semaphore without blocking ? */
  if (sema->value >= 1)
    {
      /* Yes: we take it now */
      sema->value --;      
      retval = SOS_OK;
    }
  else
    {
      /* No: we signal it */
      retval = -SOS_EBUSY;
    }

  sos_restore_IRQs(flags);
  return retval;
}


sos_ret_t sos_ksema_up(struct sos_ksema *sema)
{
  sos_ui32_t flags;
  sos_ret_t retval;

  sos_disable_IRQs(flags);

  sema->value ++;
  retval = sos_kwaitq_wakeup(& sema->kwaitq, 1, SOS_OK);

  sos_restore_IRQs(flags);
  return retval;
}


sos_ret_t sos_kmutex_init(struct sos_kmutex *mutex, const char *name)
{
  mutex->owner = NULL;
  return sos_kwaitq_init(& mutex->kwaitq, name);
}


sos_ret_t sos_kmutex_dispose(struct sos_kmutex *mutex)
{
  return sos_kwaitq_dispose(& mutex->kwaitq);
}


/*
 * Implementation based on ownership transfer (ie no while()
 * loop). The only assumption is that the thread awoken by
 * kmutex_unlock is not suppressed before effectively waking up: in
 * that case the mutex will be forever locked AND unlockable (by
 * nobody other than the owner, but this is not natural since this
 * owner already issued an unlock()...). The same problem happens with
 * the semaphores, but in a less obvious manner.
 */
sos_ret_t sos_kmutex_lock(struct sos_kmutex *mutex,
			  struct sos_time *timeout)
{
  __label__ exit_kmutex_lock;
  sos_ui32_t flags;
  sos_ret_t retval;

  sos_disable_IRQs(flags);
  retval = SOS_OK;

  /* Mutex already owned ? */
  if (NULL != mutex->owner)
    {
      /* Owned by us or by someone else ? */
      if (sos_thread_get_current() == mutex->owner)
	{
	  /* Owned by us: do nothing */
	  retval = -SOS_EBUSY;
	  goto exit_kmutex_lock;
	}

      /* Wait for somebody to wake us */
      retval = sos_kwaitq_wait(& mutex->kwaitq, timeout);

      /* Something wrong happened ? */
      if (SOS_OK != retval)
	{
	  goto exit_kmutex_lock;
	}
    }

  /* Ok, the mutex is available to us: take it */
  mutex->owner = sos_thread_get_current();

 exit_kmutex_lock:
  sos_restore_IRQs(flags);
  return retval;
}


sos_ret_t sos_kmutex_trylock(struct sos_kmutex *mutex)
{
  sos_ui32_t flags;
  sos_ret_t retval;

  sos_disable_IRQs(flags);

  /* Mutex available to us ? */
  if (NULL == mutex->owner)
    {
      /* Great ! Take it now */
      mutex->owner = sos_thread_get_current();

      retval = SOS_OK;
    }
  else
    {
      /* No: signal it */
      retval = -SOS_EBUSY;
    }

  sos_restore_IRQs(flags);
  return retval;
}


sos_ret_t sos_kmutex_unlock(struct sos_kmutex *mutex)
{
  sos_ui32_t flags;
  sos_ret_t  retval;

  sos_disable_IRQs(flags);

  if (sos_thread_get_current() != mutex->owner)
    retval = -SOS_EPERM;

  else if (sos_kwaitq_is_empty(& mutex->kwaitq))
    {
      /*
       * There is NOT ANY thread waiting => we really mark the mutex
       * as FREE
       */
      mutex->owner = NULL;
      retval = SOS_OK;
    }
  else
    {
      /*
       * There is at least 1 thread waiting => we DO NOT mark the
       * mutex as free !
       * Actually, we should have written:
       *   mutex->owner = thread_that_is_woken_up;
       * But the real Id of the next thread owning the mutex is not
       * that important. What is important here is that mutex->owner
       * IS NOT NULL. Otherwise there will be a possibility for the
       * thread woken up here to have the mutex stolen by a thread
       * locking the mutex in the meantime.
       */
      retval = sos_kwaitq_wakeup(& mutex->kwaitq, 1, SOS_OK);
    } 
 
  sos_restore_IRQs(flags);
  return retval;
}
