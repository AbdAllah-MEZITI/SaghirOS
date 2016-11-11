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
#ifndef _SOS_PAGING_H_
#define _SOS_PAGING_H_

/**
 * @file paging.h
 *
 * MMU management routines (arch-dependent). Setup the MMU without
 * identity-mapping physical<->virtual addresses over the whole
 * physical address space: a single, restricted and known, area is
 * identity-mapped, the remaining kernel/user space is not. To access
 * and manage the MMU translation tables (PD/PT on x86), we rely on a
 * particular configuration, called "mirroring", where the top-level
 * translation table (PD on x86) maps itself at a known and fixed (virtual)
 * address. The only assumption for this to be possible is that the
 * structure of the translation table entries are compatible at the
 * different levels of vadddr->paddr translation process (PDE and PTE
 * on x86 are Ok). Credits go to Christophe Avoinne for that.
 */

#include <os/types.h>
#include <os/errno.h>


/**
 * Basic SOS virtual memory organization
 */
/** Frontier between kernel and user space virtual addresses */
#define SOS_PAGING_BASE_USER_ADDRESS (0x40000000) /* 1GB */
#define SOS_PAGING_TOP_USER_ADDRESS (0xFFFFFFFF) /* 4GB */

/** Length of the space reserved for the mirroring in the kernel
    virtual space */
#define SOS_PAGING_MIRROR_SIZE  (1 << 22)  /* 1 PD = 1024 Page Tables = 4MB */

/** Virtual address where the mirroring takes place */
#define SOS_PAGING_MIRROR_VADDR \
   (SOS_PAGING_BASE_USER_ADDRESS - SOS_PAGING_MIRROR_SIZE)


/**
 * sos_paging_map flags
 */
/** Usual virtual memory access rights */
#define SOS_VM_MAP_PROT_NONE  0
#define SOS_VM_MAP_PROT_READ  (1<<0)
#define SOS_VM_MAP_PROT_WRITE (1<<1)
/* EXEC not supported */

/** Mapping a page may involve an physical page allocation (for a new
    PT), hence may potentially block */
#define SOS_VM_MAP_ATOMIC     (1<<31)


/**
 * Setup initial page directory structure where the kernel is
 * identically-mapped, and the mirroring. This routine also
 * identity-maps the BIOS and video areas, to allow some debugging
 * text to be printed to the console. Finally, this routine installs
 * the whole configuration into the MMU.
 */
sos_ret_t sos_paging_subsystem_setup(sos_paddr_t identity_mapping_base,
			   sos_paddr_t identity_mapping_top);

/**
 * Map the given physical page at the given virtual address in the
 * current address space.
 *
 * @note *IMPORTANT*: The physical page ppage_paddr *MUST* have been
 * referenced by the caller through either a call to
 * sos_physmem_ref_physpage_new() or sos_physmem_ref_physpage_at(). It
 * would work if this were untrue, but this would be INCORRECT (it is
 * expected that one is owning the page before mapping it, or
 * otherwise the page could have been stolen by an interrupt or
 * another thread).
 *
 * @param ppage_paddr  The address of a physical page (page-aligned)
 * @param vpage_vaddr  The address of the virtual page (page-aligned)
 * @param is_user_page TRUE when the page is available from user space
 * @param flags        A mask made of SOS_VM_* bits
 *
 * @note Unless the SOS_VM_MAP_ATOMIC bit is set in the flags, the
 * function may potentially block, because a physical page may be
 * allocated for a new PT.
 */
sos_ret_t sos_paging_map(sos_paddr_t ppage_paddr,
			 sos_vaddr_t vpage_vaddr,
			 sos_bool_t is_user_page,
			 sos_ui32_t flags);

/**
 * Undo the mapping from vaddr to the underlying physical page (if any)
 * @param vpage_vaddr  The address of the virtual page (page-aligned)
 */
sos_ret_t sos_paging_unmap(sos_vaddr_t vpage_vaddr);

/**
 * Return the page protection flags (SOS_VM_MAP_PROT_*) associated
 * with the address, or SOS_VM_MAP_PROT_NONE when page is not mapped
 */
int sos_paging_get_prot(sos_vaddr_t vaddr);

/**
 * Return the physical address of the given virtual address. Since page
 * at physical addr 0 is not mapped, the NULL result means "page not
 * mapped".
 */
sos_paddr_t sos_paging_get_paddr(sos_vaddr_t vaddr);

/**
 * Tell whether the address is physically mapped
 */
#define sos_paging_check_present(vaddr) \
  (sos_paging_get_paddr(vaddr) != NULL)


#endif /* _SOS_PAGING_H_ */
