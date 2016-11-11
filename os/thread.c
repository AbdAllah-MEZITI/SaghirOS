/* Copyright (C) 2004,2005 David Decotigny

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

#include <os/physmem.h>
#include <os/kmem_slab.h>
#include <os/kmalloc.h>
#include <lib/klibc.h>
#include <os/list.h>
#include <os/assert.h>

#include <hwcore/irq.h>

#include "thread.h"


/**
 * The size of the stack of a kernel thread
 */
#define SOS_THREAD_KERNEL_STACK_SIZE (1*SOS_PAGE_SIZE)


/**
 * The identifier of the thread currently running on CPU.
 *
 * We only support a SINGLE processor, ie a SINGLE thread
 * running at any time in the system. This greatly simplifies the
 * implementation of the system, since we don't have to complicate
 * things in order to retrieve the identifier of the threads running
 * on the CPU. On multiprocessor systems the current_thread below is
 * an array indexed by the id of the CPU, so that the challenge is to
 * retrieve the identifier of the CPU. This is usually done based on
 * the stack address (Linux implementation) or on some form of TLS
 * ("Thread Local Storage": can be implemented by way of LDTs for the
 * processes, accessed through the fs or gs registers).
 */
static volatile struct sos_thread *current_thread = NULL;


/*
 * The list of threads currently in the system.
 *
 * @note We could have used current_thread for that...
 */
static struct sos_thread *thread_list = NULL;


/**
 * The Cache of thread structures
 */
static struct sos_kslab_cache *cache_thread;


struct sos_thread *sos_thread_get_current()
{
  SOS_ASSERT_FATAL(current_thread->state == SOS_THR_RUNNING);
  return (struct sos_thread*)current_thread;
}


inline static sos_ret_t _set_current(struct sos_thread *thr)
{
  SOS_ASSERT_FATAL(thr->state == SOS_THR_READY);
  current_thread = thr;
  current_thread->state = SOS_THR_RUNNING;
  return SOS_OK;
}


sos_ret_t sos_thread_subsystem_setup(sos_vaddr_t init_thread_stack_base_addr,
				     sos_size_t init_thread_stack_size)
{
  struct sos_thread *myself;

  /* Allocate the cache of threads */
  cache_thread = sos_kmem_cache_create("thread",
				       sizeof(struct sos_thread),
				       2,
				       0,
				       SOS_KSLAB_CREATE_MAP
				       | SOS_KSLAB_CREATE_ZERO);
  if (! cache_thread)
    return -SOS_ENOMEM;

  /* Allocate a new thread structure for the current running thread */
  myself = (struct sos_thread*) sos_kmem_cache_alloc(cache_thread,
						     SOS_KSLAB_ALLOC_ATOMIC);
  if (! myself)
    return -SOS_ENOMEM;

  /* Initialize the thread attributes */
  strzcpy(myself->name, "[kinit]", SOS_THR_MAX_NAMELEN);
  myself->state           = SOS_THR_CREATED;
  myself->kernel_stack_base_addr = init_thread_stack_base_addr;
  myself->kernel_stack_size      = init_thread_stack_size;

  /* Do some stack poisoning on the bottom of the stack, if needed */
  sos_cpu_state_prepare_detect_kernel_stack_overflow(myself->cpu_state,
						     myself->kernel_stack_base_addr,
						     myself->kernel_stack_size);

  /* Add the thread in the global list */
  list_singleton_named(thread_list, myself, gbl_prev, gbl_next);

  /* Ok, now pretend that the running thread is ourselves */
  myself->state = SOS_THR_READY;
  _set_current(myself);

  return SOS_OK;
}


struct sos_thread *
sos_create_kernel_thread(const char *name,
			 sos_kernel_thread_start_routine_t start_func,
			 void *start_arg)
{
  __label__ undo_creation;
  sos_ui32_t flags;
  struct sos_thread *new_thread;

  if (! start_func)
    return NULL;

  /* Allocate a new thread structure for the current running thread */
  new_thread
    = (struct sos_thread*) sos_kmem_cache_alloc(cache_thread,
						SOS_KSLAB_ALLOC_ATOMIC);
  if (! new_thread)
    return NULL;

  /* Initialize the thread attributes */
  strzcpy(new_thread->name, ((name)?name:"[NONAME]"), SOS_THR_MAX_NAMELEN);
  new_thread->state    = SOS_THR_CREATED;

  /* Allocate the stack for the new thread */
  new_thread->kernel_stack_base_addr = sos_kmalloc(SOS_THREAD_KERNEL_STACK_SIZE, 0);
  new_thread->kernel_stack_size      = SOS_THREAD_KERNEL_STACK_SIZE;
  if (! new_thread->kernel_stack_base_addr)
    goto undo_creation;

  /* Initialize the CPU context of the new thread */
  if (SOS_OK
      != sos_cpu_kstate_init(& new_thread->cpu_state,
			     (sos_cpu_kstate_function_arg1_t*) start_func,
			     (sos_ui32_t) start_arg,
			     new_thread->kernel_stack_base_addr,
			     new_thread->kernel_stack_size,
			     (sos_cpu_kstate_function_arg1_t*) sos_thread_exit,
			     (sos_ui32_t) NULL))
    goto undo_creation;

  /* Add the thread in the global list */
  sos_disable_IRQs(flags);
  list_add_tail_named(thread_list, new_thread, gbl_prev, gbl_next);
  sos_restore_IRQs(flags);

  /* Mark the thread ready */
  if (SOS_OK != sos_sched_set_ready(new_thread))
    goto undo_creation;

  /* Normal non-erroneous end of function */
  return new_thread;

 undo_creation:
  if (new_thread->kernel_stack_base_addr)
    sos_kfree((sos_vaddr_t) new_thread->kernel_stack_base_addr);
  sos_kmem_cache_free((sos_vaddr_t) new_thread);
  return NULL;
}


/** Function called after thr has terminated. Called from inside the context
    of another thread, interrupts disabled */
static void delete_thread(struct sos_thread *thr)
{
  sos_ui32_t flags;

  sos_disable_IRQs(flags);
  list_delete_named(thread_list, thr, gbl_prev, gbl_next);
  sos_restore_IRQs(flags);

  sos_kfree((sos_vaddr_t) thr->kernel_stack_base_addr);
  memset(thr, 0x0, sizeof(struct sos_thread));
  sos_kmem_cache_free((sos_vaddr_t) thr);
}


void sos_thread_exit()
{
  sos_ui32_t flags;
  struct sos_thread *myself, *next_thread;

  /* Interrupt handlers are NOT allowed to exit the current thread ! */
  SOS_ASSERT_FATAL(! sos_servicing_irq());

  myself = sos_thread_get_current();

  /* Refuse to end the current executing thread if it still holds a
     resource ! */
  SOS_ASSERT_FATAL(list_is_empty_named(myself->kwaitq_list,
				       prev_entry_for_thread,
				       next_entry_for_thread));

  /* Prepare to run the next thread */
  sos_disable_IRQs(flags);
  myself->state = SOS_THR_ZOMBIE;
  next_thread = sos_reschedule(myself, FALSE);

  /* Make sure that the next_thread is valid */
  sos_cpu_state_detect_kernel_stack_overflow(next_thread->cpu_state,
					     next_thread->kernel_stack_base_addr,
					     next_thread->kernel_stack_size);

  /* No need for sos_restore_IRQs() here because the IRQ flag will be
     restored to that of the next thread upon context switch */

  /* Immediate switch to next thread */
  _set_current(next_thread);
  sos_cpu_context_exit_to(next_thread->cpu_state,
			  (sos_cpu_kstate_function_arg1_t*) delete_thread,
			  (sos_ui32_t) myself);
}


sos_thread_state_t sos_thread_get_state(struct sos_thread *thr)
{
  if (! thr)
    thr = (struct sos_thread*)current_thread;

  return thr->state;
}


typedef enum { YIELD_MYSELF, BLOCK_MYSELF } switch_type_t;
/**
 * Helper function to initiate a context switch in case the current
 * thread becomes blocked, waiting for a timeout, or calls yield.
 */
static sos_ret_t _switch_to_next_thread(switch_type_t operation)
{
  struct sos_thread *myself, *next_thread;

  SOS_ASSERT_FATAL(current_thread->state == SOS_THR_RUNNING);

  /* Interrupt handlers are NOT allowed to block ! */
  SOS_ASSERT_FATAL(! sos_servicing_irq());

  myself = (struct sos_thread*)current_thread;

  /* Make sure that if we are to be marked "BLOCKED", we have any
     reason of effectively being blocked */
  if (BLOCK_MYSELF == operation)
    {
      myself->state = SOS_THR_BLOCKED;
    }

  /* Identify the next thread */
  next_thread = sos_reschedule(myself, YIELD_MYSELF == operation);

  /* Avoid context switch if the context does not change */
  if (myself != next_thread)
    {
      /* Sanity checks for the next thread */
      sos_cpu_state_detect_kernel_stack_overflow(next_thread->cpu_state,
						 next_thread->kernel_stack_base_addr,
						 next_thread->kernel_stack_size);


      /*
       * Actual CPU context switch
       */
      _set_current(next_thread);
      sos_cpu_context_switch(& myself->cpu_state, next_thread->cpu_state);
      
      /* Back here ! */
      SOS_ASSERT_FATAL(current_thread == myself);
      SOS_ASSERT_FATAL(current_thread->state == SOS_THR_RUNNING);
    }
  else
    {
      /* No context switch but still update ID of current thread */
      _set_current(next_thread);
    }

  return SOS_OK;
}


sos_ret_t sos_thread_yield()
{
  sos_ui32_t flags;
  sos_ret_t retval;

  sos_disable_IRQs(flags);

  retval = _switch_to_next_thread(YIELD_MYSELF);

  sos_restore_IRQs(flags);
  return retval;
}


/**
 * Internal sleep timeout management
 */
struct sleep_timeout_params
{
  struct sos_thread *thread_to_wakeup;
  sos_bool_t timeout_triggered;
};


/**
 * Callback called when a timeout happened
 */
static void sleep_timeout(struct sos_timeout_action *act)
{
  struct sleep_timeout_params *sleep_timeout_params
    = (struct sleep_timeout_params*) act->routine_data;

  /* Signal that we have been woken up by the timeout */
  sleep_timeout_params->timeout_triggered = TRUE;

  /* Mark the thread ready */
  SOS_ASSERT_FATAL(SOS_OK ==
		   sos_thread_force_unblock(sleep_timeout_params
					     ->thread_to_wakeup));
}


sos_ret_t sos_thread_sleep(struct sos_time *timeout)
{
  sos_ui32_t flags;
  struct sleep_timeout_params sleep_timeout_params;
  struct sos_timeout_action timeout_action;
  sos_ret_t retval;

  /* Block forever if no timeout is given */
  if (NULL == timeout)
    {
      sos_disable_IRQs(flags);
      retval = _switch_to_next_thread(BLOCK_MYSELF);
      sos_restore_IRQs(flags);

      return retval;
    }

  /* Initialize the timeout action */
  sos_time_init_action(& timeout_action);

  /* Prepare parameters used by the sleep timeout callback */
  sleep_timeout_params.thread_to_wakeup 
    = (struct sos_thread*)current_thread;
  sleep_timeout_params.timeout_triggered = FALSE;

  sos_disable_IRQs(flags);

  /* Now program the timeout ! */
  SOS_ASSERT_FATAL(SOS_OK ==
		   sos_time_register_action_relative(& timeout_action,
						     timeout,
						     sleep_timeout,
						     & sleep_timeout_params));

  /* Prepare to block: wait for sleep_timeout() to wakeup us in the
     timeout kwaitq, or for someone to wake us up in any other
     waitq */
  retval = _switch_to_next_thread(BLOCK_MYSELF);
  /* Unblocked by something ! */

  /* Unblocked by timeout ? */
  if (sleep_timeout_params.timeout_triggered)
    {
      /* Yes */
      SOS_ASSERT_FATAL(sos_time_is_zero(& timeout_action.timeout));
      retval = SOS_OK;
    }
  else
    {
      /* No: We have probably been woken up while in some other
	 kwaitq */
      SOS_ASSERT_FATAL(SOS_OK == sos_time_unregister_action(& timeout_action));
      retval = -SOS_EINTR;
    }

  sos_restore_IRQs(flags);

  /* Update the remaining timeout */
  memcpy(timeout, & timeout_action.timeout, sizeof(struct sos_time));

  return retval;
}


sos_ret_t sos_thread_force_unblock(struct sos_thread *thread)
{
  sos_ret_t retval;
  sos_ui32_t flags;

  if (! thread)
    return -SOS_EINVAL;
  
  sos_disable_IRQs(flags);

  /* Thread already woken up ? */
  retval = SOS_OK;
  switch(sos_thread_get_state(thread))
    {
    case SOS_THR_RUNNING:
    case SOS_THR_READY:
      /* Do nothing */
      break;

    case SOS_THR_ZOMBIE:
      retval = -SOS_EFATAL;
      break;

    default:
      retval = sos_sched_set_ready(thread);
      break;
    }

  sos_restore_IRQs(flags);

  return retval;
}
