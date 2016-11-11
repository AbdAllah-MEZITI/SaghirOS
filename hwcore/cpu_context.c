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


#include <os/assert.h>
#include <lib/klibc.h>
//#include <drivers/bochs.h>
#include <lib/x86_videomem.h>
#include <hwcore/segment.h>

#include "cpu_context.h"


/**
 * Here is the definition of a CPU context for IA32 processors. This
 * is a SOS convention, not a specification given by the IA32
 * spec. However there is a strong constraint related to the x86
 * interrupt handling specification: the top of the stack MUST be
 * compatible with the 'iret' instruction, ie there must be the
 * err_code (might be 0), eip, cs and eflags of the destination
 * context in that order (see Intel x86 specs vol 3, figure 5-4).
 *
 * @note IMPORTANT: This definition MUST be consistent with the way
 * the registers are stored on the stack in
 * irq_wrappers.S/exception_wrappers.S !!! Hence the constraint above.
 */
struct sos_cpu_state {
  /* (Lower addresses) */

  /* These are SOS convention */
  sos_ui16_t  gs;
  sos_ui16_t  fs;
  sos_ui16_t  es;
  sos_ui16_t  ds;
  sos_ui16_t  cpl0_ss; /* This is ALWAYS the Stack Segment of the
			  Kernel context (CPL0) of the interrupted
			  thread, even for a user thread */
  sos_ui16_t  alignment_padding; /* unused */
  sos_ui32_t  eax;
  sos_ui32_t  ebx;
  sos_ui32_t  ecx;
  sos_ui32_t  edx;
  sos_ui32_t  esi;
  sos_ui32_t  edi;
  sos_ui32_t  ebp;

  /* MUST NEVER CHANGE (dependent on the IA32 iret instruction) */
  sos_ui32_t  error_code;
  sos_vaddr_t eip;
  sos_ui32_t  cs; /* 32bits according to the specs ! However, the CS
		     register is really 16bits long */
  sos_ui32_t  eflags;

  /* (Higher addresses) */
} __attribute__((packed));


/**
 * The CS value pushed on the stack by the CPU upon interrupt, and
 * needed by the iret instruction, is 32bits long while the real CPU
 * CS register is 16bits only: this macro simply retrieves the CPU
 * "CS" register value from the CS value pushed on the stack by the
 * CPU upon interrupt.
 *
 * The remaining 16bits pushed by the CPU should be considered
 * "reserved" and architecture dependent. IMHO, the specs don't say
 * anything about them. Considering that some architectures generate
 * non-zero values for these 16bits (at least Cyrix), we'd better
 * ignore them.
 */
#define GET_CPU_CS_REGISTER_VALUE(pushed_ui32_cs_value) \
  ( (pushed_ui32_cs_value) & 0xffff )


/**
 * Structure of an interrupted Kernel thread's context
 */
struct sos_cpu_kstate
{
  struct sos_cpu_state regs;
} __attribute__((packed));


/**
 * THE main operation of a kernel thread. This routine calls the
 * kernel thread function start_func and calls exit_func when
 * start_func returns.
 */
static void core_routine (sos_cpu_kstate_function_arg1_t *start_func,
			  sos_ui32_t start_arg,
			  sos_cpu_kstate_function_arg1_t *exit_func,
			  sos_ui32_t exit_arg)
     __attribute__((noreturn));

static void core_routine (sos_cpu_kstate_function_arg1_t *start_func,
			  sos_ui32_t start_arg,
			  sos_cpu_kstate_function_arg1_t *exit_func,
			  sos_ui32_t exit_arg)
{
  start_func(start_arg);
  exit_func(exit_arg);

  SOS_ASSERT_FATAL(! "The exit function of the thread should NOT return !");
  for(;;);
}


sos_ret_t sos_cpu_kstate_init(struct sos_cpu_state **ctxt,
			      sos_cpu_kstate_function_arg1_t *start_func,
			      sos_ui32_t  start_arg,
			      sos_vaddr_t stack_bottom,
			      sos_size_t  stack_size,
			      sos_cpu_kstate_function_arg1_t *exit_func,
			      sos_ui32_t  exit_arg)
{
  /* We are initializing a Kernel thread's context */
  struct sos_cpu_kstate *kctxt;

  /* This is a critical internal function, so that it is assumed that
     the caller knows what he does: we legitimally assume that values
     for ctxt, start_func, stack_* and exit_func are allways VALID ! */

  /* Setup the stack.
   *
   * On x86, the stack goes downward. Each frame is configured this
   * way (higher addresses first):
   *
   *  - (optional unused space. As of gcc 3.3, this space is 24 bytes)
   *  - arg n
   *  - arg n-1
   *  - ...
   *  - arg 1
   *  - return instruction address: The address the function returns to
   *    once finished
   *  - local variables
   *
   * The remaining of the code should be read from the end upward to
   * understand how the processor will handle it.
   */

  sos_vaddr_t tmp_vaddr = stack_bottom + stack_size;
  sos_ui32_t *stack = (sos_ui32_t*)tmp_vaddr;

  /* If needed, poison the stack */
#ifdef SOS_CPU_STATE_DETECT_UNINIT_KERNEL_VARS
  memset((void*)stack_bottom, SOS_CPU_STATE_STACK_POISON, stack_size);
#elif defined(SOS_CPU_STATE_DETECT_KERNEL_STACK_OVERFLOW)
  sos_cpu_state_prepare_detect_kernel_stack_overflow(stack_bottom, stack_size);
#endif

  /* Simulate a call to the core_routine() function: prepare its
     arguments */
  *(--stack) = exit_arg;
  *(--stack) = (sos_ui32_t)exit_func;
  *(--stack) = start_arg;
  *(--stack) = (sos_ui32_t)start_func;
  *(--stack) = 0; /* Return address of core_routine => force page fault */

  /*
   * Setup the initial context structure, so that the CPU will execute
   * the function core_routine() once this new context has been
   * restored on CPU
   */

  /* Compute the base address of the structure, which must be located
     below the previous elements */
  tmp_vaddr  = ((sos_vaddr_t)stack) - sizeof(struct sos_cpu_kstate);
  kctxt = (struct sos_cpu_kstate*)tmp_vaddr;

  /* Initialize the CPU context structure */
  memset(kctxt, 0x0, sizeof(struct sos_cpu_kstate));

  /* Tell the CPU context structure that the first instruction to
     execute will be that of the core_routine() function */
  kctxt->regs.eip = (sos_ui32_t)core_routine;

  /* Setup the segment registers */
  kctxt->regs.cs
    = SOS_BUILD_SEGMENT_REG_VALUE(0, FALSE, SOS_SEG_KCODE); /* Code */
  kctxt->regs.ds
    = SOS_BUILD_SEGMENT_REG_VALUE(0, FALSE, SOS_SEG_KDATA); /* Data */
  kctxt->regs.es
    = SOS_BUILD_SEGMENT_REG_VALUE(0, FALSE, SOS_SEG_KDATA); /* Data */
  kctxt->regs.cpl0_ss
    = SOS_BUILD_SEGMENT_REG_VALUE(0, FALSE, SOS_SEG_KDATA); /* Stack */
  /* fs and gs unused for the moment. */

  /* The newly created context is initially interruptible */
  kctxt->regs.eflags = (1 << 9); /* set IF bit */

  /* Finally, update the generic kernel/user thread context */
  *ctxt = (struct sos_cpu_state*) kctxt;

  return SOS_OK;
}


#if defined(SOS_CPU_STATE_DETECT_KERNEL_STACK_OVERFLOW)
void
sos_cpu_state_prepare_detect_kernel_stack_overflow(const struct sos_cpu_state *ctxt,
						   sos_vaddr_t stack_bottom,
						   sos_size_t stack_size)
{
  sos_size_t poison_size = SOS_CPU_STATE_DETECT_KERNEL_STACK_OVERFLOW;
  if (poison_size > stack_size)
    poison_size = stack_size;

  memset((void*)stack_bottom, SOS_CPU_STATE_STACK_POISON, poison_size);
}


void
sos_cpu_state_detect_kernel_stack_overflow(const struct sos_cpu_state *ctxt,
					   sos_vaddr_t stack_bottom,
					   sos_size_t stack_size)
{
  unsigned char *c;
  int i;

  /* On SOS, "ctxt" corresponds to the address of the esp register of
     the saved context in Kernel mode (always, even for the interrupted
     context of a user thread). Here we make sure that this stack
     pointer is within the allowed stack area */
  SOS_ASSERT_FATAL(((sos_vaddr_t)ctxt) >= stack_bottom);
  SOS_ASSERT_FATAL(((sos_vaddr_t)ctxt) + sizeof(struct sos_cpu_kstate)
		   <= stack_bottom + stack_size);

  /* Check that the bottom of the stack has not been altered */
  for (c = (unsigned char*) stack_bottom, i = 0 ;
       (i < SOS_CPU_STATE_DETECT_KERNEL_STACK_OVERFLOW) && (i < stack_size) ;
       c++, i++)
    {
      SOS_ASSERT_FATAL(SOS_CPU_STATE_STACK_POISON == *c);
    }
}
#endif


/* =======================================================================
 * Public Accessor functions
 */


sos_vaddr_t sos_cpu_context_get_PC(const struct sos_cpu_state *ctxt)
{
  SOS_ASSERT_FATAL(NULL != ctxt);

  /* This is the PC of the interrupted context (ie kernel or user
     context). */
  return ctxt->eip;
}


sos_vaddr_t sos_cpu_context_get_SP(const struct sos_cpu_state *ctxt)
{
  SOS_ASSERT_FATAL(NULL != ctxt);

  /* On SOS, "ctxt" corresponds to the address of the esp register of
     the saved context in Kernel mode (always, even for the interrupted
     context of a user thread). */
  return (sos_vaddr_t)ctxt;
}


void sos_cpu_context_dump(const struct sos_cpu_state *ctxt)
{
  char buf[128];
  snprintf(buf, sizeof(buf),
	   "CPU: eip=%x esp=%x eflags=%x cs=%x ds=%x ss=%x err=%x",
	   (unsigned)ctxt->eip, (unsigned)ctxt, (unsigned)ctxt->eflags,
	   (unsigned)GET_CPU_CS_REGISTER_VALUE(ctxt->cs), (unsigned)ctxt->ds,
	   (unsigned)ctxt->cpl0_ss,
	   (unsigned)ctxt->error_code);
  /*sos_bochs_putstring(buf); sos_bochs_putstring("\n");
  sos_x86_videomem_putstring(23, 0,
			  SOS_X86_VIDEO_FG_BLACK | SOS_X86_VIDEO_BG_LTGRAY,
			  buf);*/
  printf("%s\n",buf);
  os_printf (23, 0, SOS_X86_VIDEO_FG_BLACK | SOS_X86_VIDEO_BG_LTGRAY, "%s", buf);

}


/* =======================================================================
 * Public Accessor functions TO BE USED ONLY BY Exception handlers
 */


sos_ui32_t sos_cpu_context_get_EX_info(const struct sos_cpu_state *ctxt)
{
  SOS_ASSERT_FATAL(NULL != ctxt);
  return ctxt->error_code;
}


sos_vaddr_t
sos_cpu_context_get_EX_faulting_vaddr(const struct sos_cpu_state *ctxt)
{
  sos_ui32_t cr2;

  /*
   * See Intel Vol 3 (section 5.14): the address of the faulting
   * virtual address of a page fault is stored in the cr2
   * register.
   *
   * Actually, we do not store the cr2 register in a saved
   * kernel thread's context. So we retrieve the cr2's value directly
   * from the processor. The value we retrieve in an exception handler
   * is actually the correct one because an exception is synchronous
   * with the code causing the fault, and cannot be interrupted since
   * the IDT entries in SOS are "interrupt gates" (ie IRQ are
   * disabled).
   */
  asm volatile ("movl %%cr2, %0"
		:"=r"(cr2)
		: );

  return cr2;
}


/* =======================================================================
 * Backtrace facility. To be used for DEBUGging purpose ONLY.
 */


sos_ui32_t sos_backtrace(const struct sos_cpu_state *cpu_state,
			 sos_ui32_t max_depth,
			 sos_vaddr_t stack_bottom,
			 sos_size_t stack_size,
			 sos_backtrace_callback_t * backtracer,
			 void *custom_arg)
{
  int depth;
  sos_vaddr_t callee_PC, caller_frame;

  /*
   * Layout of a frame on the x86 (compiler=gcc):
   *
   * funcA calls funcB calls funcC
   *
   *         ....
   *         funcB Argument 2
   *         funcB Argument 1
   *         funcA Return eip
   * frameB: funcA ebp (ie previous stack frame)
   *         ....
   *         (funcB local variables)
   *         ....
   *         funcC Argument 2
   *         funcC Argument 1
   *         funcB Return eip
   * frameC: funcB ebp (ie previous stack frame == A0) <---- a frame address
   *         ....
   *         (funcC local variables)
   *         ....
   *
   * The presence of "ebp" on the stack depends on 2 things:
   *   + the compiler is gcc
   *   + the source is compiled WITHOUT the -fomit-frame-pointer option
   * In the absence of "ebp", chances are high that the value pushed
   * at that address is outside the stack boundaries, meaning that the
   * function will return -SOS_ENOSUP.
   */

  if (cpu_state)
    {
      callee_PC    = cpu_state->eip;
      caller_frame = cpu_state->ebp;
    }
  else
    {
      /* Skip the sos_backtrace() frame */
      callee_PC    = (sos_vaddr_t)__builtin_return_address(0);
      caller_frame = (sos_vaddr_t)__builtin_frame_address(1);
    }

  for(depth=0 ; depth < max_depth ; depth ++)
    {
      /* Call the callback */
      backtracer(callee_PC, caller_frame + 8, depth, custom_arg);

      /* If the frame address is funky, don't go further */
      if ( (caller_frame < stack_bottom)
	   || (caller_frame + 4 >= stack_bottom + stack_size) )
	return depth;

      /* Go to caller frame */
      callee_PC    = *((sos_vaddr_t*) (caller_frame + 4));
      caller_frame = *((sos_vaddr_t*) caller_frame);
    }
  
  return depth;
}
