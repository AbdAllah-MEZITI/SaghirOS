/* Copyright (C) 2000 Thomas Petazzoni
   Copyright (C) 2004 David Decotigny

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

#include <os/list.h>
#include <os/physmem.h>
#include <hwcore/paging.h>
#include <os/assert.h>

#include "kmem_vmm.h"

/** The structure of a range of kernel-space virtual addresses */
struct sos_kmem_range
{
  sos_vaddr_t base_vaddr;
  sos_count_t nb_pages;

  /* The slab owning this range, or NULL */
  struct sos_kslab *slab;

  struct sos_kmem_range *prev, *next;
};
const int sizeof_struct_sos_kmem_range = sizeof(struct sos_kmem_range);

/** The ranges are SORTED in (strictly) ascending base addresses */
static struct sos_kmem_range *kmem_free_range_list, *kmem_used_range_list;

/** The slab cache for the kmem ranges */
static struct sos_kslab_cache *kmem_range_cache;



/** Helper function to get the closest preceding or containing
    range for the given virtual address */
static struct sos_kmem_range *
get_closest_preceding_kmem_range(struct sos_kmem_range *the_list,
				 sos_vaddr_t vaddr)
{
  int nb_elements;
  struct sos_kmem_range *a_range, *ret_range;

  /* kmem_range list is kept SORTED, so we exit as soon as vaddr >= a
     range base address */
  ret_range = NULL;
  list_foreach(the_list, a_range, nb_elements)
    {
      if (vaddr < a_range->base_vaddr)
	return ret_range;
      ret_range = a_range;
    }

  /* This will always be the LAST range in the kmem area */
  return ret_range;
}


/**
 * Helper function to lookup a free range large enough to hold nb_pages
 * pages (first fit)
 */
static struct sos_kmem_range *find_suitable_free_range(sos_count_t nb_pages)
{
  int nb_elements;
  struct sos_kmem_range *r;

  list_foreach(kmem_free_range_list, r, nb_elements)
  {
    if (r->nb_pages >= nb_pages)
      return r;
  }

  return NULL;
}


/**
 * Helper function to add a_range in the_list, in strictly ascending order.
 *
 * @return The (possibly) new head of the_list
 */
static struct sos_kmem_range *insert_range(struct sos_kmem_range *the_list,
					   struct sos_kmem_range *a_range)
{
  struct sos_kmem_range *prec_used;

  /** Look for any preceding range */
  prec_used = get_closest_preceding_kmem_range(the_list,
					       a_range->base_vaddr);
  /** insert a_range /after/ this prec_used */
  if (prec_used != NULL)
    list_insert_after(the_list, prec_used, a_range);
  else /* Insert at the beginning of the list */
    list_add_head(the_list, a_range);

  return the_list;
}


/**
 * Helper function to retrieve the range owning the given vaddr, by
 * scanning the physical memory first if vaddr is mapped in RAM
 */
static struct sos_kmem_range *lookup_range(sos_vaddr_t vaddr)
{
  struct sos_kmem_range *range;

  /* First: try to retrieve the physical page mapped at this address */
  sos_paddr_t ppage_paddr = SOS_PAGE_ALIGN_INF(sos_paging_get_paddr(vaddr));
  if (ppage_paddr)
    {
      range = sos_physmem_get_kmem_range(ppage_paddr);

      /* If a page is mapped at this address, it is EXPECTED that it
	 is really associated with a range */
      SOS_ASSERT_FATAL(range != NULL);
    }

  /* Otherwise scan the list of used ranges, looking for the range
     owning the address */
  else
    {
      range = get_closest_preceding_kmem_range(kmem_used_range_list,
					       vaddr);
      /* Not found */
      if (! range)
	return NULL;

      /* vaddr not covered by this range */
      if ( (vaddr < range->base_vaddr)
	   || (vaddr >= (range->base_vaddr + range->nb_pages*SOS_PAGE_SIZE)) )
	return NULL;
    }

  return range;
}


/**
 * Helper function for sos_kmem_vmm_setup() to initialize a new range
 * that maps a given area as free or as already used.
 * This function either succeeds or halts the whole system.
 */
static struct sos_kmem_range *
create_range(sos_bool_t  is_free,
	     sos_vaddr_t base_vaddr,
	     sos_vaddr_t top_vaddr,
	     struct sos_kslab *associated_slab)
{
  struct sos_kmem_range *range;

  SOS_ASSERT_FATAL(SOS_IS_PAGE_ALIGNED(base_vaddr));
  SOS_ASSERT_FATAL(SOS_IS_PAGE_ALIGNED(top_vaddr));

  if ((top_vaddr - base_vaddr) < SOS_PAGE_SIZE)
    return NULL;

  range = (struct sos_kmem_range*)sos_kmem_cache_alloc(kmem_range_cache,
						       SOS_KSLAB_ALLOC_ATOMIC);
  SOS_ASSERT_FATAL(range != NULL);

  range->base_vaddr = base_vaddr;
  range->nb_pages   = (top_vaddr - base_vaddr) / SOS_PAGE_SIZE;

  if (is_free)
    {
      list_add_tail(kmem_free_range_list,
		    range);
    }
  else
    {
      sos_vaddr_t vaddr;
      range->slab = associated_slab;
      list_add_tail(kmem_used_range_list,
		    range);

      /* Ok, set the range owner for the pages in this page */
      for (vaddr = base_vaddr ;
	   vaddr < top_vaddr ;
	   vaddr += SOS_PAGE_SIZE)
      {
	sos_paddr_t ppage_paddr = sos_paging_get_paddr(vaddr);
	SOS_ASSERT_FATAL((void*)ppage_paddr != NULL);
	sos_physmem_set_kmem_range(ppage_paddr, range);
      }
    }

  return range;
}


sos_ret_t
sos_kmem_vmm_subsystem_setup(sos_vaddr_t kernel_core_base,
			     sos_vaddr_t kernel_core_top,
			     sos_vaddr_t bootstrap_stack_bottom_vaddr,
			     sos_vaddr_t bootstrap_stack_top_vaddr)
{
  struct sos_kslab *first_struct_slab_of_caches,
    *first_struct_slab_of_ranges;
  sos_vaddr_t first_slab_of_caches_base,
    first_slab_of_caches_nb_pages,
    first_slab_of_ranges_base,
    first_slab_of_ranges_nb_pages;
  struct sos_kmem_range *first_range_of_caches,
    *first_range_of_ranges;

  list_init(kmem_free_range_list);
  list_init(kmem_used_range_list);

  kmem_range_cache
    = sos_kmem_cache_subsystem_setup_prepare(kernel_core_base,
					     kernel_core_top,
					     sizeof(struct sos_kmem_range),
					     & first_struct_slab_of_caches,
					     & first_slab_of_caches_base,
					     & first_slab_of_caches_nb_pages,
					     & first_struct_slab_of_ranges,
					     & first_slab_of_ranges_base,
					     & first_slab_of_ranges_nb_pages);
  SOS_ASSERT_FATAL(kmem_range_cache != NULL);

  /* Mark virtual addresses 16kB - Video as FREE */
  create_range(TRUE,
	       SOS_KMEM_VMM_BASE,
	       SOS_PAGE_ALIGN_INF(BIOS_N_VIDEO_START),
	       NULL);
  
  /* Mark virtual addresses in Video hardware mapping as NOT FREE */
  create_range(FALSE,
	       SOS_PAGE_ALIGN_INF(BIOS_N_VIDEO_START),
	       SOS_PAGE_ALIGN_SUP(BIOS_N_VIDEO_END),
	       NULL);
  
  /* Mark virtual addresses Video - Kernel as FREE */
  create_range(TRUE,
	       SOS_PAGE_ALIGN_SUP(BIOS_N_VIDEO_END),
	       SOS_PAGE_ALIGN_INF(kernel_core_base),
	       NULL);
  
  /* Mark virtual addresses in Kernel code/data up to the bootstrap stack
     as NOT FREE */
  create_range(FALSE,
	       SOS_PAGE_ALIGN_INF(kernel_core_base),
	       bootstrap_stack_bottom_vaddr,
	       NULL);

  /* Mark virtual addresses in the bootstrap stack as NOT FREE too,
     but in another vmm region in order to be un-allocated later */
  create_range(FALSE,
	       bootstrap_stack_bottom_vaddr,
	       bootstrap_stack_top_vaddr,
	       NULL);

  /* Mark the remaining virtual addresses in Kernel code/data after
     the bootstrap stack as NOT FREE */
  create_range(FALSE,
	       bootstrap_stack_top_vaddr,
	       SOS_PAGE_ALIGN_SUP(kernel_core_top),
	       NULL);

  /* Mark virtual addresses in the first slab of the cache of caches
     as NOT FREE */
  SOS_ASSERT_FATAL(SOS_PAGE_ALIGN_SUP(kernel_core_top)
		   == first_slab_of_caches_base);
  SOS_ASSERT_FATAL(first_struct_slab_of_caches != NULL);
  first_range_of_caches
    = create_range(FALSE,
		   first_slab_of_caches_base,
		   first_slab_of_caches_base
		   + first_slab_of_caches_nb_pages*SOS_PAGE_SIZE,
		   first_struct_slab_of_caches);

  /* Mark virtual addresses in the first slab of the cache of ranges
     as NOT FREE */
  SOS_ASSERT_FATAL((first_slab_of_caches_base
		    + first_slab_of_caches_nb_pages*SOS_PAGE_SIZE)
		   == first_slab_of_ranges_base);
  SOS_ASSERT_FATAL(first_struct_slab_of_ranges != NULL);
  first_range_of_ranges
    = create_range(FALSE,
		   first_slab_of_ranges_base,
		   first_slab_of_ranges_base
		   + first_slab_of_ranges_nb_pages*SOS_PAGE_SIZE,
		   first_struct_slab_of_ranges);
  
  /* Mark virtual addresses after these slabs as FREE */
  create_range(TRUE,
	       first_slab_of_ranges_base
	       + first_slab_of_ranges_nb_pages*SOS_PAGE_SIZE,
	       SOS_KMEM_VMM_TOP,
	       NULL);

  /* Update the cache subsystem so that the artificially-created
     caches of caches and ranges really behave like *normal* caches (ie
     those allocated by the normal slab API) */
  sos_kmem_cache_subsystem_setup_commit(first_struct_slab_of_caches,
					first_range_of_caches,
					first_struct_slab_of_ranges,
					first_range_of_ranges);

  return SOS_OK;
}


/**
 * Allocate a new kernel area spanning one or multiple pages.
 *
 * @eturn a new range structure
 */
struct sos_kmem_range *sos_kmem_vmm_new_range(sos_count_t nb_pages,
					      sos_ui32_t  flags,
					      sos_vaddr_t * range_start)
{
  struct sos_kmem_range *free_range, *new_range;

  if (nb_pages <= 0)
    return NULL;

  /* Find a suitable free range to hold the size-sized object */
  free_range = find_suitable_free_range(nb_pages);
  if (free_range == NULL)
    return NULL;

  /* If range has exactly the requested size, just move it to the
     "used" list */
  if(free_range->nb_pages == nb_pages)
    {
      list_delete(kmem_free_range_list, free_range);
      kmem_used_range_list = insert_range(kmem_used_range_list,
					  free_range);
      /* The new_range is exactly the free_range */
      new_range = free_range;
    }

  /* Otherwise the range is bigger than the requested size, split it.
     This involves reducing its size, and allocate a new range, which
     is going to be added to the "used" list */
  else
    {
      /* free_range split in { new_range | free_range } */
      new_range = (struct sos_kmem_range*)
	sos_kmem_cache_alloc(kmem_range_cache,
			     (flags & SOS_KMEM_VMM_ATOMIC)?
			     SOS_KSLAB_ALLOC_ATOMIC:0);
      if (! new_range)
	return NULL;

      new_range->base_vaddr   = free_range->base_vaddr;
      new_range->nb_pages     = nb_pages;
      free_range->base_vaddr += nb_pages*SOS_PAGE_SIZE;
      free_range->nb_pages   -= nb_pages;

      /* free_range is still at the same place in the list */
      /* insert new_range in the used list */
      kmem_used_range_list = insert_range(kmem_used_range_list,
					  new_range);
    }

  /* By default, the range is not associated with any slab */
  new_range->slab = NULL;

  /* If mapping of physical pages is needed, map them now */
  if (flags & SOS_KMEM_VMM_MAP)
    {
      int i;
      for (i = 0 ; i < nb_pages ; i ++)
	{
	  /* Get a new physical page */
	  sos_paddr_t ppage_paddr
	    = sos_physmem_ref_physpage_new(! (flags & SOS_KMEM_VMM_ATOMIC));
	  
	  /* Map the page in kernel space */
	  if (ppage_paddr)
	    {
	      if (sos_paging_map(ppage_paddr,
				 new_range->base_vaddr
				   + i * SOS_PAGE_SIZE,
				 FALSE /* Not a user page */,
				 ((flags & SOS_KMEM_VMM_ATOMIC)?
				  SOS_VM_MAP_ATOMIC:0)
				 | SOS_VM_MAP_PROT_READ
				 | SOS_VM_MAP_PROT_WRITE))
		{
		  /* Failed => force unallocation, see below */
		  sos_physmem_unref_physpage(ppage_paddr);
		  ppage_paddr = (sos_paddr_t)NULL;
		}
	      else
		{
		  /* Success : page can be unreferenced since it is
		     now mapped */
		  sos_physmem_unref_physpage(ppage_paddr);
		}
	    }

	  /* Undo the allocation if failed to allocate or map a new page */
	  if (! ppage_paddr)
	    {
	      sos_kmem_vmm_del_range(new_range);
	      return NULL;
	    }

	  /* Ok, set the range owner for this page */
	  sos_physmem_set_kmem_range(ppage_paddr, new_range);
	}
    }
  /* ... Otherwise: Demand Paging will do the job */

  if (range_start)
    *range_start = new_range->base_vaddr;

  return new_range;
}


sos_ret_t sos_kmem_vmm_del_range(struct sos_kmem_range *range)
{
  int i;
  struct sos_kmem_range *ranges_to_free;
  list_init(ranges_to_free);

  SOS_ASSERT_FATAL(range != NULL);
  SOS_ASSERT_FATAL(range->slab == NULL);

  /* Remove the range from the 'USED' list now */
  list_delete(kmem_used_range_list, range);

  /*
   * The following do..while() loop is here to avoid an indirect
   * recursion: if we call directly kmem_cache_free() from inside the
   * current function, we take the risk to re-enter the current function
   * (sos_kmem_vmm_del_range()) again, which may cause problem if it
   * in turn calls kmem_slab again and sos_kmem_vmm_del_range again,
   * and again and again. This may happen while freeing ranges of
   * struct sos_kslab...
   *
   * To avoid this,we choose to call a special function of kmem_slab
   * doing almost the same as sos_kmem_cache_free(), but which does
   * NOT call us (ie sos_kmem_vmm_del_range()): instead WE add the
   * range that is to be freed to a list, and the do..while() loop is
   * here to process this list ! The recursion is replaced by
   * classical iterations.
   */
  do
    {
      /* Ok, we got the range. Now, insert this range in the free list */
      kmem_free_range_list = insert_range(kmem_free_range_list, range);

      /* Unmap the physical pages */
      for (i = 0 ; i < range->nb_pages ; i ++)
	{
	  /* This will work even if no page is mapped at this address */
	  sos_paging_unmap(range->base_vaddr + i*SOS_PAGE_SIZE);
	}
      
      /* Eventually coalesce it with prev/next free ranges (there is
	 always a valid prev/next link since the list is circular). Note:
	 the tests below will lead to correct behaviour even if the list
	 is limited to the 'range' singleton, at least as long as the
	 range is not zero-sized */
      /* Merge with preceding one ? */
      if (range->prev->base_vaddr + range->prev->nb_pages*SOS_PAGE_SIZE
	  == range->base_vaddr)
	{
	  struct sos_kmem_range *empty_range_of_ranges = NULL;
	  struct sos_kmem_range *prec_free = range->prev;
	  
	  /* Merge them */
	  prec_free->nb_pages += range->nb_pages;
	  list_delete(kmem_free_range_list, range);
	  
	  /* Mark the range as free. This may cause the slab owning
	     the range to become empty */
	  empty_range_of_ranges = 
	    sos_kmem_cache_release_struct_range(range);

	  /* If this causes the slab owning the range to become empty,
	     add the range corresponding to the slab at the end of the
	     list of the ranges to be freed: it will be actually freed
	     in one of the next iterations of the do{} loop. */
	  if (empty_range_of_ranges != NULL)
	    {
	      list_delete(kmem_used_range_list, empty_range_of_ranges);
	      list_add_tail(ranges_to_free, empty_range_of_ranges);
	    }
	  
	  /* Set range to the beginning of this coelescion */
	  range = prec_free;
	}
      
      /* Merge with next one ? [NO 'else' since range may be the result of
	 the merge above] */
      if (range->base_vaddr + range->nb_pages*SOS_PAGE_SIZE
	  == range->next->base_vaddr)
	{
	  struct sos_kmem_range *empty_range_of_ranges = NULL;
	  struct sos_kmem_range *next_range = range->next;
	  
	  /* Merge them */
	  range->nb_pages += next_range->nb_pages;
	  list_delete(kmem_free_range_list, next_range);
	  
	  /* Mark the next_range as free. This may cause the slab
	     owning the next_range to become empty */
	  empty_range_of_ranges = 
	    sos_kmem_cache_release_struct_range(next_range);

	  /* If this causes the slab owning the next_range to become
	     empty, add the range corresponding to the slab at the end
	     of the list of the ranges to be freed: it will be
	     actually freed in one of the next iterations of the
	     do{} loop. */
	  if (empty_range_of_ranges != NULL)
	    {
	      list_delete(kmem_used_range_list, empty_range_of_ranges);
	      list_add_tail(ranges_to_free, empty_range_of_ranges);
	    }
	}
      

      /* If deleting the range(s) caused one or more range(s) to be
	 freed, get the next one to free */
      if (list_is_empty(ranges_to_free))
	range = NULL; /* No range left to free */
      else
	range = list_pop_head(ranges_to_free);

    }
  /* Stop when there is no range left to be freed for now */
  while (range != NULL);

  return SOS_OK;
}


sos_vaddr_t sos_kmem_vmm_alloc(sos_count_t nb_pages,
			       sos_ui32_t  flags)
{
  struct sos_kmem_range *range
    = sos_kmem_vmm_new_range(nb_pages,
			     flags,
			     NULL);
  if (! range)
    return (sos_vaddr_t)NULL;
  
  return range->base_vaddr;
}


sos_ret_t sos_kmem_vmm_free(sos_vaddr_t vaddr)
{
  struct sos_kmem_range *range = lookup_range(vaddr);

  /* We expect that the given address is the base address of the
     range */
  if (!range || (range->base_vaddr != vaddr))
    return -SOS_EINVAL;

  /* We expect that this range is not held by any cache */
  if (range->slab != NULL)
    return -SOS_EBUSY;

  return sos_kmem_vmm_del_range(range);
}


sos_ret_t sos_kmem_vmm_set_slab(struct sos_kmem_range *range,
				struct sos_kslab *slab)
{
  if (! range)
    return -SOS_EINVAL;

  range->slab = slab;
  return SOS_OK;
}

struct sos_kslab * sos_kmem_vmm_resolve_slab(sos_vaddr_t vaddr)
{
  struct sos_kmem_range *range = lookup_range(vaddr);
  if (! range)
    return NULL;

  return range->slab;
}


sos_bool_t sos_kmem_vmm_is_valid_vaddr(sos_vaddr_t vaddr)
{
  struct sos_kmem_range *range = lookup_range(vaddr);
  return (range != NULL);
}
