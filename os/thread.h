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
#ifndef _SOS_THREAD_H_
#define _SOS_THREAD_H_

/**
 * @file thread.h
 *
 * SOS Thread management API
 */

#include <os/errno.h>

/* Forward declaration */
struct sos_thread;

#include <hwcore/cpu_context.h>
#include <os/sched.h>
#include <os/kwaitq.h>
#include <os/time.h>

/**
 * The possible states of a valid thread
 */
typedef enum { SOS_THR_CREATED, /**< Thread created, not fully initialized */
	       SOS_THR_READY,   /**< Thread fully initialized or
				     waiting for CPU after having been
				     blocked or preempted */
	       SOS_THR_RUNNING, /**< Thread currently running on CPU */
	       SOS_THR_BLOCKED, /**< Thread waiting for I/O (+ in at LEAST
				     one kwaitq) and/or sleeping (+ in NO
				     kwaitq) */
	       SOS_THR_ZOMBIE,  /**< Thread terminated execution, waiting to
				     be deleted by kernel */
             } sos_thread_state_t;


/**
 * TCB (Thread Control Block): structure describing a thread. Don't
 * access these fields directly: prefer using the accessor functions
 * below.
 */
struct sos_thread
{
#define SOS_THR_MAX_NAMELEN 32
  char name[SOS_THR_MAX_NAMELEN];

  sos_thread_state_t  state;

  /**
   * The hardware context of the thread.
   *
   * It will reflect the CPU state of the thread:
   *  - From an interrupt handler: the state of the thread at the time
   *    of the OUTERMOST irq. An IRQ is not allowed to make context
   *    switches, so this context will remain valid from the begining of
   *    the outermost IRQ handler to the end of it, no matter if there
   *    are other IRQ handlers nesting in one another. You may safely
   *    use it from IRQ handlers to query the state of the interrupted
   *    thread, no matter if there has been other IRQ handlers
   *    executing meanwhile.
   *  - From normal kernel code, exceptions and syscall: the state of
   *    the thread the last time there was a context switch from this
   *    thread to another one. Thus this field WON'T reflect the
   *    current's thread cpu_state in these cases. So, in these cases,
   *    simply DO NOT USE IT outside thread.c ! Note: for syscall and
   *    exception handlers, the VALID state of the interrupted thread is
   *    passed as an argument to the handlers.
   */
  struct sos_cpu_state *cpu_state;

  /* Kernel stack parameters */
  sos_vaddr_t kernel_stack_base_addr;
  sos_size_t  kernel_stack_size;

  /* Data specific to each state */
  union
  {
    struct
    {
      struct sos_thread     *rdy_prev, *rdy_next;
    } ready;
  }; /* Anonymous union (gcc extenion) */


  /*
   * Data used by the kwaitq subsystem: list of kwaitqueues the thread
   * is waiting for.
   *
   * @note: a RUNNING or READY thread might be in one or more
   * waitqueues ! The only property we have is that, among these
   * waitqueues (if any), _at least_ one has woken the thread.
   */
  struct sos_kwaitq_entry *kwaitq_list;


  /**
   * Chaining pointers for global ("gbl") list of threads (debug)
   */
  struct sos_thread *gbl_prev, *gbl_next;
};


/**
 * Definition of the function executed by a kernel thread
 */
typedef void (*sos_kernel_thread_start_routine_t)(void *arg);


/**
 * Initialize the subsystem responsible for thread management
 *
 * Initialize the primary kernel thread so that it can be handled the
 * same way as an ordinary thread created by sos_thread_create().
 */
sos_ret_t sos_thread_subsystem_setup(sos_vaddr_t init_thread_stack_base_addr,
				     sos_size_t init_thread_stack_size);


/**
 * Create a new kernel thread
 */
struct sos_thread *
sos_create_kernel_thread(const char *name,
			 sos_kernel_thread_start_routine_t start_func,
			 void *start_arg);


/**
 * Terminate the execution of the current thread. For kernel threads,
 * it is called by default when the start routine returns.
 */
void sos_thread_exit() __attribute__((noreturn));


/**
 * Get the identifier of the thread currently running on CPU. Trivial
 * function.
 */
struct sos_thread *sos_thread_get_current();


/**
 * If thr == NULL, get the state of the current thread. Trivial
 * function.
 *
 * @note NOT protected against interrupts
 */
sos_thread_state_t sos_thread_get_state(struct sos_thread *thr);


/**
 * Yield CPU to another ready thread.
 *
 * @note This is a BLOCKING FUNCTION
 */
sos_ret_t sos_thread_yield();


/**
 * Release the CPU for (at least) the given delay.
 *
 * @param delay The delay to wait for. If delay == NULL then wait
 * forever that any event occurs.
 *
 * @return SOS_OK when delay expired (and delay is reset to zero),
 * -SOS_EINTR otherwise (and delay contains the amount of time
 * remaining).
 *
 * @note This is a BLOCKING FUNCTION
 */
sos_ret_t sos_thread_sleep(/* in/out */struct sos_time *delay);


/**
 * Mark the given thread as READY (if not already ready) even if it is
 * blocked in a kwaitq or in a sleep ! As a result, the interrupted
 * kwaitq/sleep function call of the thread will return with
 * -SOS_EINTR.
 *
 * @return -SOS_EINVAL if thread does not exist, or -SOS_EFATAL if
 * marked ZOMBIE.
 *
 * @note As a result, the semaphore/mutex/conditions/... functions
 * return values SHOULD ALWAYS be checked ! If they are != SOS_OK,
 * then the caller should consider that the resource is not aquired
 * because somebody woke the thread by some way.
 */
sos_ret_t sos_thread_force_unblock(struct sos_thread *thread);


#endif /* _SOS_THREAD_H_ */
