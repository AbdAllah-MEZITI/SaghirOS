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
#include <os/macros.h>

#include "physmem.h"
#include "kmem_vmm.h"
#include "kmem_slab.h"

#include "kmalloc.h"

/* The cache structures for these caches, the object size, their
   names, and some number of pages that contain them. They might not
   necessarily be powers of 2s. */
static struct {
  const char             *name;
  sos_size_t             object_size;
  sos_count_t            pages_per_slab;
  struct sos_kslab_cache *cache;
} kmalloc_cache[] =
  {
    { "kmalloc 8B objects",     8,     1  },
    { "kmalloc 16B objects",    16,    1  },
    { "kmalloc 32B objects",    32,    1  },
    { "kmalloc 64B objects",    64,    1  },
    { "kmalloc 128B objects",   128,   1  },
    { "kmalloc 256B objects",   256,   2  },
    { "kmalloc 1024B objects",  1024,  2  },
    { "kmalloc 2048B objects",  2048,  3  },
    { "kmalloc 4096B objects",  4096,  4  },
    { "kmalloc 8192B objects",  8192,  8  },
    { "kmalloc 16384B objects", 16384, 12 },
    { NULL, 0, 0, NULL }
  };


sos_ret_t sos_kmalloc_subsystem_setup()
{
  int i;
  for (i = 0 ; kmalloc_cache[i].object_size != 0 ; i ++)
    {
      struct sos_kslab_cache *new_cache;
      new_cache = sos_kmem_cache_create(kmalloc_cache[i].name,
					kmalloc_cache[i].object_size,
					kmalloc_cache[i].pages_per_slab,
					0,
					SOS_KSLAB_CREATE_MAP
					);
      SOS_ASSERT_FATAL(new_cache != NULL);
      kmalloc_cache[i].cache = new_cache;
    }
  return SOS_OK;
}


sos_vaddr_t sos_kmalloc(sos_size_t size, sos_ui32_t flags)
{
  /* Look for a suitable pre-allocated kmalloc cache */
  int i;
  for (i = 0 ; kmalloc_cache[i].object_size != 0 ; i ++)
    {
      if (kmalloc_cache[i].object_size >= size)
	return sos_kmem_cache_alloc(kmalloc_cache[i].cache,
				    (flags
				     & SOS_KMALLOC_ATOMIC)?
				    SOS_KSLAB_ALLOC_ATOMIC:0);
    }

  /* none found yet => we directly use the kmem_vmm subsystem to
     allocate whole pages */
  return sos_kmem_vmm_alloc(SOS_PAGE_ALIGN_SUP(size) / SOS_PAGE_SIZE,
			    ( (flags
			       & SOS_KMALLOC_ATOMIC)?
			      SOS_KMEM_VMM_ATOMIC:0)
			    | SOS_KMEM_VMM_MAP
			    );
}


sos_ret_t sos_kfree(sos_vaddr_t vaddr)
{
  /* The trouble here is that we aren't sure whether this object is a
     slab object in a pre-allocated kmalloc cache, or an object
     directly allocated as a kmem_vmm region. */
  
  /* We first pretend this object is allocated in a pre-allocated
     kmalloc cache */
  if (! sos_kmem_cache_free(vaddr))
    return SOS_OK; /* Great ! We guessed right ! */
    
  /* Here we're wrong: it appears not to be an object in a
     pre-allocated kmalloc cache. So we try to pretend this is a
     kmem_vmm area */
  return sos_kmem_vmm_free(vaddr);
}


