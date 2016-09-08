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
#ifndef _SOS_KMALLOC_H_
#define _SOS_KMALLOC_H_

/**
 * @file kmalloc.h
 *
 * Simple malloc-style wrapper to kmem_vmm.h and kmem_slab.h for
 * "anonymous" objects (ie not associated to any precise slab cache).
 */

#include <os/types.h>
#include <os/errno.h>


/**
 * Iniatilize the kmalloc subsystem, ie pre-allocate a series of caches.
 */
sos_ret_t sos_kmalloc_setup(void);

/*
 * sos_kmalloc flags
 */
/** sos_kmalloc() should succeed without blocking, or return NULL */
#define SOS_KMALLOC_ATOMIC  1

/**
 * Allocate a kernel object of the given size in the most suited slab
 * cache if size can be handled by one of the pre-allocated caches, or
 * using directly the range allocator otherwise. The object will
 * allways be mapped in physical memory (ie implies
 * SOS_KSLAB_CREATE_MAP and SOS_KMEM_VMM_MAP).
 *
 * @param size  The size of the object
 * @param flags The allocation flags (SOS_KMALLOC_* flags)
 */
sos_vaddr_t sos_kmalloc(sos_size_t size, sos_ui32_t flags);

/**
 * @note you are perfectly allowed to give the address of the
 * kernel image, or the address of the bios area here, it will work:
 * the kernel/bios WILL be "deallocated". But if you really want to do
 * this, well..., do expect some "surprises" ;)
 */
sos_ret_t sos_kfree(sos_vaddr_t vaddr);

#endif /* _SOS_KMALLOC_H_ */
