/* Copyright (C) 2005  David Decotigny
   Copyright (C) 2000-2004, The KOS team

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
#ifndef _SOS_CPUCTXT_H_
#define _SOS_CPUCTXT_H_


/**
 * @file cpu_context.h
 *
 * Low level API to manage kernel and user thread CPU contexts. Should
 * be some kind of architecture-independent.
 */

#include <os/types.h>
#include <os/errno.h>


/**
 * Opaque structure storing the CPU context of an inactive kernel or
 * user thread, as saved by the low level primitives below or by the
 * interrupt/exception handlers.
 *
 * @note This is an (architecture-independent) forward declaration:
 * see cpu_context.c and the *.S files for its
 * (architecture-dependent) definition.
 */
struct sos_cpu_state;


/**
 * The type of the functions passed as arguments to the Kernel thread
 * related functions.
 */
typedef void (sos_cpu_kstate_function_arg1_t(sos_ui32_t arg1));


/**
 * Function to create an initial context for a kernel thread starting
 * its execution at function start_func with the argument initial_arg,
 * and having the stack defined by stack_bottom/stack_size. When the
 * start_func function returns, the function exit_func is called with
 * argument exit_arg.
 *
 * @param kctxt The kernel thread CPU context to initialize. The
 * address of the newly-initialized struct sos_cpu_state will be
 * stored in this variable. The contents of this struct sos_cpu_state
 * are actually located /inside/ the stack.
 *
 * @param start_func The address of the first instruction that will be
 * executed when this context will be first transferred on
 * CPU. Practically speaking, this is the address of a function that
 * is assumed to take 1 argument.
 *
 * @param start_arg The value that will be passed as the argument to
 * start_func when the thread starts. The stack will be setup
 * accordingly to simulate a real call to the function and really
 * passing this arguement.
 *
 * @param stack_bottom The lowest address of the stack.
 *
 * @param stack_size The size of the stack.
 *
 * @param exit_func The address of the instruction executed after the
 * function start_func has returned. This function takes 1 parameter
 * as argument: exit_arg.
 *
 * @param exit_arg The argument passed to the function exit_func.
 *
 * @note the newly created context is INTERRUPTIBLE by default !
 */
sos_ret_t sos_cpu_kstate_init(struct sos_cpu_state **kctxt,
			      sos_cpu_kstate_function_arg1_t *start_func,
			      sos_ui32_t  start_arg,
			      sos_vaddr_t stack_bottom,
			      sos_size_t  stack_size,
			      sos_cpu_kstate_function_arg1_t *exit_func,
			      sos_ui32_t  exit_arg);


/**
 * Function that performs an immediate context-switch from one
 * kernel/user thread to another one. It stores the current executing
 * context in from_ctxt, and restores to_context on CPU.
 *
 * @param from_ctxt The address of the struct sos_cpu_state will be
 * stored in this variable. Must NOT be NULL.
 *
 * @param to_ctxt The CPU will resume its execution with the struct
 * sos_cpu_state located at this address. Must NOT be NULL.
 */
void sos_cpu_context_switch(struct sos_cpu_state **from_ctxt,
			    struct sos_cpu_state *to_ctxt);


/*
 * Switch to the new given context (of a kernel/user thread) without
 * saving the old context (of another kernel/user thread), and call
 * the function reclaiming_func passing it the recalining_arg
 * argument. The reclaining function is called from within the stack
 * of the new context, so that it can (among other things) safely
 * destroy the stack of the former context.
 *
 * @param switch_to_ctxt The context that will be restored on the CPU
 *
 * @param reclaiming_func The address of the function that will be
 * called after having changed the stack, but before restoring the CPU
 * context to switch_to_ctxt.
 */
void
sos_cpu_context_exit_to(struct sos_cpu_state *switch_to_ctxt,
			sos_cpu_kstate_function_arg1_t *reclaiming_func,
			sos_ui32_t reclaiming_arg) __attribute__((noreturn));

/* =======================================================================
 * Public Accessor functions
 */


/**
 * Return Program Counter stored in the saved kernel/user context
 */
sos_vaddr_t sos_cpu_context_get_PC(const struct sos_cpu_state *ctxt);


/**
 * Return Stack Pointer stored in the saved kernel/user context
 */
sos_vaddr_t sos_cpu_context_get_SP(const struct sos_cpu_state *ctxt);


/**
 * Dump the contents of the CPU context (bochs + x86_videomem)
 */
void sos_cpu_context_dump(const struct sos_cpu_state *ctxt);


/* =======================================================================
 * Public Accessor functions TO BE USED ONLY BY Exception handlers
 */


/**
 * Return the argument passed by the CPU upon exception, as stored in the
 * saved context
 */
sos_ui32_t sos_cpu_context_get_EX_info(const struct sos_cpu_state *ctxt);


/**
 * Return the faulting address of the exception
 */
sos_vaddr_t
sos_cpu_context_get_EX_faulting_vaddr(const struct sos_cpu_state *ctxt);


/* =======================================================================
 * Macros controlling stack poisoning.
 * Stack poisoning can be used to detect:
 *  - unitialized local variables
 *  - when the thread might have gone too deep in the stack
 */
/** The signature of the poison */
#define SOS_CPU_STATE_STACK_POISON 0xa5

/**
 * When set, mean that the whole stack is poisoned to detect use of
 * unititialized variables
 */
#define SOS_CPU_STATE_DETECT_UNINIT_KERNEL_VARS
/* #undef SOS_CPU_STATE_DETECT_UNINIT_KERNEL_VARS */

/**
 * When set, mean that the bottom of the stack is poisoned to detect
 * probable stack overflow. Its value indicates the number of bytes
 * used for this detection.
 */
#define SOS_CPU_STATE_DETECT_KERNEL_STACK_OVERFLOW  64
/* #undef SOS_CPU_STATE_DETECT_KERNEL_STACK_OVERFLOW */

#if defined(SOS_CPU_STATE_DETECT_KERNEL_STACK_OVERFLOW)
void
sos_cpu_state_prepare_detect_kernel_stack_overflow(const struct sos_cpu_state *ctxt,
						   sos_vaddr_t kernel_stack_bottom,
						   sos_size_t kernel_stack_size);
void sos_cpu_state_detect_kernel_stack_overflow(const struct sos_cpu_state *ctxt,
						sos_vaddr_t kernel_stack_bottom,
						sos_size_t kernel_stack_size);
#else
# define sos_cpu_state_prepare_detect_kernel_stack_overflow(ctxt,stkbottom,stksize) \
  ({ /* nop */ })
# define sos_cpu_state_detect_kernel_stack_overflow(ctxt,stkbottom,stksize) \
  ({ /* nop */ })
#endif


/* =======================================================================
 * Backtrace facility. To be used for DEBUGging purpose ONLY.
 */


/**
 * The function called at each step of the backtrace iterations
 *
 * @param PC The address of the next instruction of the function that
 * will be executed
 *
 * @param params The address of the array of the parameteres that have
 * been passed to the function considered
 *
 * @param depth The index of the iteration (ie the depth of the
 * current frame into the stack)
 *
 * @param custom_arg Whatever you want: this is the argument passed as
 * custom_arg to sos_backtrace()
 */
typedef void (sos_backtrace_callback_t)(sos_vaddr_t PC,
					sos_vaddr_t params,
					sos_ui32_t depth,
					void *custom_arg);


/**
 * Call the backtracer callback on each frame stored in the cpu_state
 *
 * @param cpu_state The CPU context we want to explore. MUST be the
 * context of a thread in Kernel mode, or NULL. When NULL: backtrace
 * the current CPU context.
 *
 * @param max_depth The maximum number of frames to explore
 *
 * @param stack_bottom The lower boundary of the stack. This is used
 * to make sure that the frame addresses fit inside the stack
 * boudaries (ie are potentially correct).
 *
 * @param stack_size The size of the stack. Same comment.
 *
 * @param backtracer The function to call to handle the frame for each
 * iteration
 *
 * @param custom_arg The arg passed as custom_arg to the backtracer
 *
 * @return The number of frames explored.
 *
 * @note Might be inaccurate when gcc's -fomit-frame-pointer has been
 * used.
 */
sos_ui32_t sos_backtrace(const struct sos_cpu_state *cpu_state,
			 sos_ui32_t max_depth,
			 sos_vaddr_t stack_bottom,
			 sos_size_t stack_size,
			 sos_backtrace_callback_t * backtracer,
			 void *custom_arg);

#endif /* _SOS_CPUCTXT_H_ */
