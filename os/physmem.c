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
#include <os/list.h>
#include <os/macros.h>
#include <os/assert.h>
#include <klibc.h>

#include "physmem.h"

/** A descriptor for a physical page in SOS */
struct physical_page_descr
{
  /** The physical base address for the page */
  sos_paddr_t paddr;

  /** The reference count for this physical page. > 0 means that the
     page is in the used list. */
  sos_count_t ref_cnt;

  /** The other pages on the list (used, free) */
  struct physical_page_descr *prev, *next;
};

/** These are some markers present in the executable file (see sos.lds) */
extern char __b_kernel, __e_kernel;

/** The array of ppage descriptors will be located at this address */
#define PAGE_DESCR_ARRAY_ADDR \
  SOS_PAGE_ALIGN_SUP((sos_paddr_t) (& __e_kernel))
static struct physical_page_descr * physical_page_descr_array;

/** The list of physical pages currently available */
static struct physical_page_descr *free_ppage;

/** The list of physical pages currently in use */
static struct physical_page_descr *used_ppage;

/** We will store here the interval of valid physical addresses */
static sos_paddr_t physmem_base, physmem_top;

/** We store the number of pages used/free */
static sos_count_t physmem_total_pages, physmem_used_pages;

sos_ret_t sos_physmem_setup(sos_size_t ram_size,
			    /* out */sos_paddr_t *kernel_core_base,
			    /* out */sos_paddr_t *kernel_core_top)
{
  /* The iterator over the page descriptors */
  struct physical_page_descr *ppage_descr;

  /* The iterator over the physical addresses */
  sos_paddr_t ppage_addr;

  /* Make sure ram size is aligned on a page boundary */
  ram_size = SOS_PAGE_ALIGN_INF(ram_size);/* Yes, we may lose at most a page */

  /* Reset the used/free page lists before building them */
  free_ppage = used_ppage = NULL;
  physmem_total_pages = physmem_used_pages = 0;

  /* Make sure that there is enough memory to store the array of page
     descriptors */
  *kernel_core_base = SOS_PAGE_ALIGN_INF((sos_paddr_t)(& __b_kernel));
  *kernel_core_top
    = PAGE_DESCR_ARRAY_ADDR
      + SOS_PAGE_ALIGN_SUP( (ram_size >> SOS_PAGE_SHIFT)
			    * sizeof(struct physical_page_descr));
  if (*kernel_core_top > ram_size)
    return -SOS_ENOMEM;

  /* Page 0-4kB is not available in order to return address 0 as a
     means to signal "no page available" */
  physmem_base = SOS_PAGE_SIZE;
  physmem_top  = ram_size;

  /* Setup the page descriptor arrray */
  physical_page_descr_array
    = (struct physical_page_descr*)PAGE_DESCR_ARRAY_ADDR;

  /* Scan the list of physical pages */
  for (ppage_addr = 0,
	 ppage_descr = physical_page_descr_array ;
       ppage_addr < physmem_top ;
       ppage_addr += SOS_PAGE_SIZE,
	 ppage_descr ++)
    {
      enum { PPAGE_MARK_RESERVED, PPAGE_MARK_FREE,
	     PPAGE_MARK_KERNEL, PPAGE_MARK_HWMAP } todo;

      memset(ppage_descr, 0x0, sizeof(struct physical_page_descr));

      /* Init the page descriptor for this page */
      ppage_descr->paddr = ppage_addr;

      /* Reserved : 0 ... base */
      if (ppage_addr < physmem_base)
	todo = PPAGE_MARK_RESERVED;

      /* Free : base ... BIOS */
      else if ((ppage_addr >= physmem_base)
	       && (ppage_addr < BIOS_N_VIDEO_START))
	todo = PPAGE_MARK_FREE;

      /* Used : BIOS */
      else if ((ppage_addr >= BIOS_N_VIDEO_START)
	       && (ppage_addr < BIOS_N_VIDEO_END))
	todo = PPAGE_MARK_HWMAP;

      /* Free : BIOS ... kernel */
      else if ((ppage_addr >= BIOS_N_VIDEO_END)
	       && (ppage_addr < (sos_paddr_t) (& __b_kernel)))
	todo = PPAGE_MARK_FREE;

      /* Used : Kernel code/data/bss + physcal page descr array */
      else if ((ppage_addr >= *kernel_core_base)
		&& (ppage_addr < *kernel_core_top))
	todo = PPAGE_MARK_KERNEL;

      /* Free : first page of descr ... end of RAM */
      else
	todo = PPAGE_MARK_FREE;

      /* Actually does the insertion in the used/free page lists */
      physmem_total_pages ++;
      switch (todo)
	{
	case PPAGE_MARK_FREE:
	  ppage_descr->ref_cnt = 0;
	  list_add_head(free_ppage, ppage_descr);
	  break;

	case PPAGE_MARK_KERNEL:
	case PPAGE_MARK_HWMAP:
	  ppage_descr->ref_cnt = 1;
	  list_add_head(used_ppage, ppage_descr);
	  physmem_used_pages ++;
	  break;

	default:
	  /* Reserved page: nop */
	  break;
	}
    }

  return SOS_OK;
}


sos_paddr_t sos_physmem_ref_physpage_new(sos_bool_t can_block)
{
  struct physical_page_descr *ppage_descr;

  if (! free_ppage)
    return (sos_paddr_t)NULL;

  /* Retrieve a page in the free list */
  ppage_descr = list_pop_head(free_ppage);

  /* The page is assumed not to be already used */
  SOS_ASSERT_FATAL(ppage_descr->ref_cnt == 0);

  /* Mark the page as used (this of course sets the ref count to 1) */
  ppage_descr->ref_cnt ++;

  /* Put the page in the used list */
  list_add_tail(used_ppage, ppage_descr);
  physmem_used_pages ++;

  return ppage_descr->paddr;
}


/**
 * Helper function to get the physical page descriptor for the given
 * physical page address.
 *
 * @return NULL when out-of-bounds or non-page-aligned
 */
inline static struct physical_page_descr *
get_page_descr_at_paddr(sos_paddr_t ppage_paddr)
{
  /* Don't handle non-page-aligned addresses */
  if (ppage_paddr & SOS_PAGE_MASK)
    return NULL;
  
  /* Don't support out-of-bounds requests */
  if ((ppage_paddr < physmem_base) || (ppage_paddr >= physmem_top))
    return NULL;

  return physical_page_descr_array + (ppage_paddr >> SOS_PAGE_SHIFT);
}


sos_ret_t sos_physmem_ref_physpage_at(sos_paddr_t ppage_paddr)
{
  struct physical_page_descr *ppage_descr
    = get_page_descr_at_paddr(ppage_paddr);

  if (! ppage_descr)
    return -SOS_EINVAL;

  /* Increment the reference count for the page */
  ppage_descr->ref_cnt ++;

  /* If the page is newly referenced (ie we are the only owners of the
     page => ref cnt == 1), transfer it in the used pages list */
  if (ppage_descr->ref_cnt == 1)
    {
      list_delete(free_ppage, ppage_descr);
      list_add_tail(used_ppage, ppage_descr);
      physmem_used_pages ++;

      /* The page is newly referenced */
      return FALSE;
    }

  /* The page was already referenced by someone */
  return TRUE;
}


sos_ret_t
sos_physmem_unref_physpage(sos_paddr_t ppage_paddr)
{
  /* By default the return value indicates that the page is still
     used */
  sos_ret_t retval = FALSE;

  struct physical_page_descr *ppage_descr
    = get_page_descr_at_paddr(ppage_paddr);

  if (! ppage_descr)
    return -SOS_EINVAL;

  /* Don't do anything if the page is not in the used list */
  if (ppage_descr->ref_cnt <= 0)
    return -SOS_EINVAL;

  /* Unreference the page, and, when no mapping is active anymore, put
     the page in the free list */
  ppage_descr->ref_cnt--;
  if (ppage_descr->ref_cnt <= 0)
    {
      /* Transfer the page, considered USED, to the free list */
      list_delete(used_ppage, ppage_descr);
      physmem_used_pages --;
      list_add_head(free_ppage, ppage_descr);

      /* Indicate that the page is now unreferenced */
      retval = TRUE;
    }

  return retval;
}
