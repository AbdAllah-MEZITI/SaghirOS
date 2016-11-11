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

#include <os/assert.h>
#include <lib/klibc.h>
#include <hwcore/irq.h>
#include <os/list.h>

#include "time.h"


/**
 * Number of nanoseconds in 1 second
 */
#define NS_IN_SEC 1000000000UL


/**
 * The list of timeout actions waiting for a timeout. The timeout
 * actions are stored in the list in increasing initial timeout
 * order. Actually, the "timeout" field won't reflect this initial
 * timeout: for each element in the list, it stores the timeout
 * _difference_ between the timeout action and the previous in the
 * list.
 */
static struct sos_timeout_action *timeout_action_list;


/**
 * Current resolution of a time tick
 */
static struct sos_time tick_resolution;


/**
 * Time elapsed between boot and last timer tick
 *
 * @note No 'volatile' here because the tick value is NEVER modified
 * while in any of the functions below: it is modified only out of
 * these functions by the IRQ timer handler because these functions
 * are protected against timer IRQ and are "one shot" (no busy waiting
 * for a change in the tick's value).
 */
static struct sos_time last_tick_time;


sos_ret_t sos_time_inc(struct sos_time *dest,
		       const struct sos_time *to_add)
{
  /* nanosec is always < 1e9 so that their sum is always < 2e9, which
     is smaller than 2^32-1 */
  sos_ui32_t sigma_ns = dest->nanosec + to_add->nanosec;
  
  dest->sec     += to_add->sec;
  dest->sec     += sigma_ns / NS_IN_SEC;
  dest->nanosec  = sigma_ns % NS_IN_SEC;
  return SOS_OK;
}


sos_ret_t sos_time_dec(struct sos_time *dest,
		       const struct sos_time *to_dec)
{
  /* nanosec is always < 1e9 so that their difference is always in
     (-1e9, 1e9), which is compatible with the (-2^31, 2^31 - 1)
     cpacity of a signed dword */
  sos_si32_t diff_ns = ((sos_si32_t)dest->nanosec)
			- ((sos_si32_t)to_dec->nanosec);

  /* Make sure substraction is possible */
  SOS_ASSERT_FATAL(dest->sec >= to_dec->sec);
  if (dest->sec == to_dec->sec)
    SOS_ASSERT_FATAL(dest->nanosec >= to_dec->nanosec);

  dest->sec     -= to_dec->sec;
  if (diff_ns > 0)
    dest->sec     += diff_ns / NS_IN_SEC;
  else
    dest->sec     -= ((-diff_ns) / NS_IN_SEC);
  dest->nanosec  = (NS_IN_SEC + diff_ns) % NS_IN_SEC;
  if (diff_ns < 0)
    dest->sec --;
  return SOS_OK;
}


int sos_time_cmp(const struct sos_time *t1,
		 const struct sos_time *t2)
{
  /* Compare seconds */
  if (t1->sec < t2->sec)
    return -1;
  else if (t1->sec > t2->sec)
    return 1;

  /* seconds are equal => compare nanoseconds */
  else if (t1->nanosec < t2->nanosec)
    return -1;
  else if (t1->nanosec > t2->nanosec)
    return 1;

  /* else: sec and nanosecs are equal */
  return 0;
}


sos_bool_t sos_time_is_zero(const struct sos_time *tm)
{
  return ( (0 == tm->sec) && (0 == tm->nanosec) );
}


sos_ret_t sos_time_subsysem_setup(const struct sos_time *initial_resolution)
{
  timeout_action_list = NULL;
  last_tick_time = (struct sos_time) { .sec = 0, .nanosec = 0 };
  memcpy(& tick_resolution, initial_resolution, sizeof(struct sos_time));

  return SOS_OK;
}


sos_ret_t sos_time_get_tick_resolution(struct sos_time *resolution)
{
  sos_ui32_t flags;
  sos_disable_IRQs(flags);

  memcpy(resolution, & tick_resolution, sizeof(struct sos_time));

  sos_restore_IRQs(flags);
  return SOS_OK; 
}


sos_ret_t sos_time_set_tick_resolution(const struct sos_time *resolution)
{
  sos_ui32_t flags;

  sos_disable_IRQs(flags);
  memcpy(& tick_resolution, resolution, sizeof(struct sos_time));
  sos_restore_IRQs(flags);

  return SOS_OK;
}


sos_ret_t sos_time_get_now(struct sos_time *now)
{
  sos_ui32_t flags;
  sos_disable_IRQs(flags);

  memcpy(now, & last_tick_time, sizeof(struct sos_time));

  sos_restore_IRQs(flags);
  return SOS_OK;  
}


/**
 * Helper routine to add the action in the list. MUST be called with
 * interrupts disabled !
 */
static sos_ret_t _add_action(struct sos_timeout_action *act,
			     const struct sos_time *due_date,
			     sos_bool_t is_relative_due_date,
			     sos_timeout_routine_t *routine,
			     void *routine_data)
{
  struct sos_timeout_action *other, *insert_before;
  int nb_act;

  /* Delay must be specified */
  if (due_date == NULL)
    return -SOS_EINVAL;

  /* Action container MUST be specified */
  if (act == NULL)
    return -SOS_EINVAL;

  /* Refuse to add an empty action */
  if (NULL == routine)
    return -SOS_EINVAL;

  /* Refuse to add the action if it is already added */
  if (NULL != act->tmo_next)
    return -SOS_EBUSY;

  /* Compute action absolute due date */
  if (is_relative_due_date)
    {
      /* The provided due_date is relative to the current time */
      memcpy(& act->timeout, & last_tick_time, sizeof(struct sos_time));
      sos_time_inc(& act->timeout, due_date);
    }
  else
    {
      /* The provided due_date is absolute (ie relative to the system
	 boot instant) */

      if (sos_time_cmp(due_date, & last_tick_time) < 0)
	/* Refuse to add a past action ! */
	return -SOS_EINVAL;

      memcpy(& act->timeout, due_date, sizeof(struct sos_time));
    }    

  /* Prepare the action data structure */
  act->routine      = routine;
  act->routine_data = routine_data;

  /* Find the right place in the list of the timeout action. */
  insert_before = NULL;
  list_foreach_forward_named(timeout_action_list,
			     other, nb_act,
			     tmo_prev, tmo_next)
    {
      if (sos_time_cmp(& act->timeout, & other->timeout) < 0)
	{
	  insert_before = other;
	  break;
	}

      /* Loop over to next timeout */
    }

  /* Now insert the action in the list */
  if (insert_before != NULL)
    list_insert_before_named(timeout_action_list, insert_before, act,
			    tmo_prev, tmo_next);
  else
    list_add_tail_named(timeout_action_list, act,
			tmo_prev, tmo_next);

  return SOS_OK;  
}


sos_ret_t
sos_time_register_action_relative(struct sos_timeout_action *act,
				  const struct sos_time *delay,
				  sos_timeout_routine_t *routine,
				  void *routine_data)
{
  sos_ui32_t flags;
  sos_ret_t retval;

  sos_disable_IRQs(flags);
  retval = _add_action(act, delay, TRUE, routine, routine_data);
  sos_restore_IRQs(flags);

  return retval;
}


sos_ret_t
sos_time_register_action_absolute(struct sos_timeout_action *act,
				  const struct sos_time *date,
				  sos_timeout_routine_t *routine,
				  void *routine_data)
{
  sos_ui32_t flags;
  sos_ret_t retval;

  sos_disable_IRQs(flags);
  retval = _add_action(act, date, FALSE, routine, routine_data);
  sos_restore_IRQs(flags);

  return retval;
}


/**
 * Helper routine to remove the action from the list. MUST be called
 * with interrupts disabled !
 */
static sos_ret_t _remove_action(struct sos_timeout_action *act)
{
  /* Don't do anything if action is not in timeout list */
  if (NULL == act->tmo_next)
    return -SOS_EINVAL;

  /* Update the action's remaining timeout */
  if (sos_time_cmp(& act->timeout, & last_tick_time) <= 0)
    act->timeout = (struct sos_time){ .sec=0, .nanosec=0 };
  else
    sos_time_dec(& act->timeout, & last_tick_time);

  /* Actually remove the action from the list */
  list_delete_named(timeout_action_list, act,
		    tmo_prev, tmo_next);
  act->tmo_prev = act->tmo_next = NULL;

  return SOS_OK;  
}


sos_ret_t sos_time_unregister_action(struct sos_timeout_action *act)
{
  sos_ret_t retval;
  sos_ui32_t flags;

  sos_disable_IRQs(flags);
  retval = _remove_action(act);
  sos_restore_IRQs(flags);

  return SOS_OK;  
}


sos_ret_t sos_time_do_tick()
{
  sos_ui32_t flags;
  
  sos_disable_IRQs(flags);

  /* Update kernel time */
  sos_time_inc(& last_tick_time, & tick_resolution);

  while (! list_is_empty_named(timeout_action_list, tmo_prev, tmo_next))
    {
      struct sos_timeout_action *act;
      act = list_get_head_named(timeout_action_list, tmo_prev, tmo_next);

      /* Did we go too far in the actions' list ? */
      if (sos_time_cmp(& last_tick_time, & act->timeout) < 0)
	{
	  /* Yes: No need to look further. */
	  break;
	}

      /* Remove the action from the list */
      _remove_action(act);

      /* Call the action's routine */
      act->routine(act);
    }

  sos_restore_IRQs(flags);
  return SOS_OK;
}
