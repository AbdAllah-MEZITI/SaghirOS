/* Copyright (C) 2004  David Decotigny

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
#ifndef _SOS_PHYSMEM_H_
#define _SOS_PHYSMEM_H_

/**
 * @file physmem.h
 *
 * Physical pages of memory
 */

#include <os/errno.h>
#include <types.h>
#include <os/macros.h>

/** The size of a physical page (arch-dependent) */
#define SOS_PAGE_SIZE  (4*1024)

/** The corresponding shift */
#define SOS_PAGE_SHIFT 12 /* 4 kB = 2^12 B */

/** The corresponding mask */
#define SOS_PAGE_MASK  ((1<<12) - 1)

#define SOS_PAGE_ALIGN_INF(val) \
  SOS_ALIGN_INF((val), SOS_PAGE_SIZE)
#define SOS_PAGE_ALIGN_SUP(val) \
  SOS_ALIGN_SUP((val), SOS_PAGE_SIZE)


/**
 * This is the reserved physical interval for the x86 video memory and
 * BIOS area. In physmem.c, we have to mark this area as "used" in
 * order to prevent from allocating it. And in paging.c, we'd better
 * map it in virtual space if we really want to be able to print to
 * the screen (for debugging purpose, at least): for this, the
 * simplest is to identity-map this area in virtual space (note
 * however that this mapping could also be non-identical).
 */
#define BIOS_N_VIDEO_START 0xa0000
#define BIOS_N_VIDEO_END   0x100000


/**
 * Initialize the physical memory subsystem, for the physical area [0,
 * ram_size). This routine takes into account the BIOS and video
 * areas, to prevent them from future allocations.
 *
 * @param ram_size The size of the RAM that will be managed by this subsystem
 *
 * @param kernel_core_base The lowest address for which the kernel
 * assumes identity mapping (ie virtual address == physical address)
 * will be stored here
 *
 * @param kernel_core_top The top address for which the kernel
 * assumes identity mapping (ie virtual address == physical address)
 * will be stored here
 */
sos_ret_t sos_physmem_setup(sos_size_t ram_size,
			    /* out */sos_paddr_t *kernel_core_base,
			    /* out */sos_paddr_t *kernel_core_top);

/**
 * Retrieve the total number of pages, and the number of free pages
 */
sos_ret_t sos_physmem_get_state(/* out */sos_count_t *total_ppages,
				/* out */sos_count_t *used_ppages);


/**
 * Get a free page.
 *
 * @return The (physical) address of the (physical) page allocated, or
 * NULL when none currently available.
 *
 * @param can_block TRUE if the function is allowed to block
 * @note The page returned has a reference count equal to 1.
 */
sos_paddr_t sos_physmem_ref_physpage_new(sos_bool_t can_block);


/**
 * Increment the reference count of a given physical page. Useful for
 * VM code which tries to map a precise physical address.
 *
 * @return TRUE when the page was previously in use, FALSE when the
 * page was previously in the free list, <0 when the page address is
 * invalid.
 */
sos_ret_t sos_physmem_ref_physpage_at(sos_paddr_t ppage_paddr);


/**
 * Decrement the reference count of the given physical page. When this
 * reference count reaches 0, the page is marked free, ie is available
 * for future sos_physmem_get_physpage()
 *
 * @return FALSE when the page is still in use, TRUE when the page is now
 * unreferenced, <0 when the page address is invalid
 */
sos_ret_t sos_physmem_unref_physpage(sos_paddr_t ppage_paddr);


#endif /* _SOS_PHYSMEM_H_ */
