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
#include <os/physmem.h>
#include <lib/klibc.h>
#include <os/assert.h>

#include "paging.h"

/** The structure of a page directory entry. See Intel vol 3 section
    3.6.4 */
struct x86_pde
{
  sos_ui32_t present        :1; /* 1=PT mapped */
  sos_ui32_t write          :1; /* 0=read-only, 1=read/write */
  sos_ui32_t user           :1; /* 0=supervisor, 1=user */
  sos_ui32_t write_through  :1; /* 0=write-back, 1=write-through */
  sos_ui32_t cache_disabled :1; /* 1=cache disabled */
  sos_ui32_t accessed       :1; /* 1=read/write access since last clear */
  sos_ui32_t zero           :1; /* Intel reserved */
  sos_ui32_t page_size      :1; /* 0=4kB, 1=4MB or 2MB (depending on PAE) */
  sos_ui32_t global_page    :1; /* Ignored (Intel reserved) */
  sos_ui32_t custom         :3; /* Do what you want with them */
  sos_ui32_t pt_paddr       :20;
} __attribute__ ((packed));


/** The structure of a page table entry. See Intel vol 3 section
    3.6.4 */
struct x86_pte
{
  sos_ui32_t present        :1; /* 1=PT mapped */
  sos_ui32_t write          :1; /* 0=read-only, 1=read/write */
  sos_ui32_t user           :1; /* 0=supervisor, 1=user */
  sos_ui32_t write_through  :1; /* 0=write-back, 1=write-through */
  sos_ui32_t cache_disabled :1; /* 1=cache disabled */
  sos_ui32_t accessed       :1; /* 1=read/write access since last clear */
  sos_ui32_t dirty          :1; /* 1=write access since last clear */
  sos_ui32_t zero           :1; /* Intel reserved */
  sos_ui32_t global_page    :1; /* 1=No TLB invalidation upon cr3 switch
				   (when PG set in cr4) */
  sos_ui32_t custom         :3; /* Do what you want with them */
  sos_ui32_t paddr          :20;
} __attribute__ ((packed));


/** Structure of the x86 CR3 register: the Page Directory Base
    Register. See Intel x86 doc Vol 3 section 2.5 */
struct x86_pdbr
{
  sos_ui32_t zero1          :3; /* Intel reserved */
  sos_ui32_t write_through  :1; /* 0=write-back, 1=write-through */
  sos_ui32_t cache_disabled :1; /* 1=cache disabled */
  sos_ui32_t zero2          :7; /* Intel reserved */
  sos_ui32_t pd_paddr       :20;
} __attribute__ ((packed));


/**
 * Helper macro to control the MMU: invalidate the TLB entry for the
 * page located at the given virtual address. See Intel x86 vol 3
 * section 3.7.
 */
#define invlpg(vaddr) \
  do { \
       __asm__ __volatile__("invlpg %0"::"m"(*((unsigned *)(vaddr)))); \
  } while(0)


/**
 * Helper macro to control the MMU: invalidate the whole TLB. See
 * Intel x86 vol 3 section 3.7.
 */
#define flush_tlb() \
  do { \
        unsigned long tmpreg; \
        asm volatile("movl %%cr3,%0\n\tmovl %0,%%cr3" :"=r" \
                     (tmpreg) : :"memory"); \
  } while (0)


/**
 * Helper macro to compute the index in the PD for the given virtual
 * address
 */
#define virt_to_pd_index(vaddr) \
  (((unsigned)(vaddr)) >> 22)


/**
 * Helper macro to compute the index in the PT for the given virtual
 * address
 */
#define virt_to_pt_index(vaddr) \
  ( (((unsigned)(vaddr)) >> 12) & 0x3ff )


/**
 * Helper macro to compute the offset in the page for the given virtual
 * address
 */
#define virt_to_page_offset(vaddr) \
  (((unsigned)(vaddr)) & SOS_PAGE_MASK)


/**
 * Helper function to map a page in the pd.\ Suppose that the RAM
 * is identity mapped to resolve PT actual (CPU) address from the PD
 * entry
 */
static sos_ret_t paging_setup_map_helper(struct x86_pde * pd,
					 sos_paddr_t ppage,
					 sos_vaddr_t vaddr)
{
  /* Get the page directory entry and table entry index for this
     address */
  unsigned index_in_pd = virt_to_pd_index(vaddr);
  unsigned index_in_pt = virt_to_pt_index(vaddr);

  /* Make sure the page table was mapped */
  struct x86_pte * pt;
  if (pd[index_in_pd].present)
    {
      pt = (struct x86_pte*) (pd[index_in_pd].pt_paddr << 12);

      /* If we allocate a new entry in the PT, increase its reference
	 count. This test will always be TRUE here, since the setup
	 routine scans the kernel pages in a strictly increasing
	 order: at each step, the map will result in the allocation of
	 a new PT entry. For the sake of clarity, we keep the test
	 here. */
      if (! pt[index_in_pt].present)
	sos_physmem_ref_physpage_at((sos_paddr_t)pt);

      /* The previous test should always be TRUE */
      else
	SOS_ASSERT_FATAL(FALSE); /* indicate a fatal error */
    }
  else
    {
      /* No : allocate a new one */
      pt = (struct x86_pte*) sos_physmem_ref_physpage_new(FALSE);
      if (! pt)
	return -SOS_ENOMEM;
      
      memset((void*)pt, 0x0, SOS_PAGE_SIZE);

      pd[index_in_pd].present  = TRUE;
      pd[index_in_pd].write    = 1; /* It would be too complicated to
				       determine whether it
				       corresponds to a real R/W area
				       of the kernel code/data or
				       read-only */
      pd[index_in_pd].pt_paddr = ((sos_paddr_t)pt) >> 12;
    }

  
  /* Map the page in the page table */
  pt[index_in_pt].present = 1;
  pt[index_in_pt].write   = 1;  /* It would be too complicated to
				   determine whether it corresponds to
				   a real R/W area of the kernel
				   code/data or R/O only */
  pt[index_in_pt].user    = 0;
  pt[index_in_pt].paddr   = ppage >> 12;

  return SOS_OK;
}


sos_ret_t sos_paging_setup(sos_paddr_t identity_mapping_base,
			   sos_paddr_t identity_mapping_top)
{
  /* The PDBR we will setup below */
  struct x86_pdbr cr3;  

  /* Get the PD for the kernel */
  struct x86_pde * pd
    = (struct x86_pde*) sos_physmem_ref_physpage_new(FALSE);

  /* The iterator for scanning the kernel area */
  sos_paddr_t paddr;

  /* Reset the PD. For the moment, there is still an IM for the whole
     RAM, so that the paddr are also vaddr */
  memset((void*)pd,
	 0x0,
	 SOS_PAGE_SIZE);

  /* Identity-map the identity_mapping_* area */
  for (paddr = identity_mapping_base ;
       paddr < identity_mapping_top ;
       paddr += SOS_PAGE_SIZE)
    {
      if (paging_setup_map_helper(pd, paddr, paddr))
	return -SOS_ENOMEM;
    }

  /* Identity-map the PC-specific BIOS/Video area */
  for (paddr = BIOS_N_VIDEO_START ;
       paddr < BIOS_N_VIDEO_END ;
       paddr += SOS_PAGE_SIZE)
    {
      if (paging_setup_map_helper(pd, paddr, paddr))
	return -SOS_ENOMEM;
    }

  /* Ok, kernel is now identity mapped in the PD. We still have to set
     up the mirroring */
  pd[virt_to_pd_index(SOS_PAGING_MIRROR_VADDR)].present = TRUE;
  pd[virt_to_pd_index(SOS_PAGING_MIRROR_VADDR)].write = 1;
  pd[virt_to_pd_index(SOS_PAGING_MIRROR_VADDR)].user  = 0;
  pd[virt_to_pd_index(SOS_PAGING_MIRROR_VADDR)].pt_paddr 
    = ((sos_paddr_t)pd)>>12;

  /* We now just have to configure the MMU to use our PD. See Intel
     x86 doc vol 3, section 3.6.3 */
  memset(& cr3, 0x0, sizeof(struct x86_pdbr)); /* Reset the PDBR */
  cr3.pd_paddr = ((sos_paddr_t)pd) >> 12;

 /* Actual loading of the PDBR in the MMU: setup cr3 + bits 31[Paging
    Enabled] and 16[Write Protect] of cr0, see Intel x86 doc vol 3,
    sections 2.5, 3.6.1 and 4.11.3 + note table 4-2 */
  asm volatile ("movl %0,%%cr3\n\t"
		"movl %%cr0,%%eax\n\t"
		"orl $0x80010000, %%eax\n\t" /* bit 31 | bit 16 */
		"movl %%eax,%%cr0\n\t"
		"jmp 1f\n\t"
		"1:\n\t"
		"movl $2f, %%eax\n\t"
		"jmp *%%eax\n\t"
		"2:\n\t" ::"r"(cr3):"memory","eax");

  /*
   * Here, the only memory available is:
   * - The BIOS+video area
   * - the identity_mapping_base .. identity_mapping_top area
   * - the PD mirroring area (4M)
   * All accesses to other virtual addresses will generate a #PF
   */

  return SOS_OK;
}


/* Suppose that the current address is configured with the mirroring
 * enabled to access the PD and PT. */
sos_ret_t sos_paging_map(sos_paddr_t ppage_paddr,
			 sos_vaddr_t vpage_vaddr,
			 sos_bool_t is_user_page,
			 int flags)
{
  /* Get the page directory entry and table entry index for this
     address */
  unsigned index_in_pd = virt_to_pd_index(vpage_vaddr);
  unsigned index_in_pt = virt_to_pt_index(vpage_vaddr);
  
  /* Get the PD of the current context */
  struct x86_pde *pd = (struct x86_pde*)
    (SOS_PAGING_MIRROR_VADDR
     + SOS_PAGE_SIZE*virt_to_pd_index(SOS_PAGING_MIRROR_VADDR));

  /* Address of the PT in the mirroring */
  struct x86_pte * pt = (struct x86_pte*) (SOS_PAGING_MIRROR_VADDR
					   + SOS_PAGE_SIZE*index_in_pd);

  /* The mapping of anywhere in the PD mirroring is FORBIDDEN ;) */
  if ((vpage_vaddr >= SOS_PAGING_MIRROR_VADDR)
      && (vpage_vaddr < SOS_PAGING_MIRROR_VADDR + SOS_PAGING_MIRROR_SIZE))
    return -SOS_EINVAL;

  /* Map a page for the PT if necessary */
  if (! pd[index_in_pd].present)
    {
      /* No : allocate a new one */
      sos_paddr_t pt_ppage
	= sos_physmem_ref_physpage_new(! (flags & SOS_VM_MAP_ATOMIC));
      if (! pt_ppage)
	{
	  return -SOS_ENOMEM;
	}

      pd[index_in_pd].present  = TRUE;
      pd[index_in_pd].write    = 1; /* Ignored in supervisor mode, see
				       Intel vol 3 section 4.12 */
      pd[index_in_pd].user     |= (is_user_page)?1:0;
      pd[index_in_pd].pt_paddr = ((sos_paddr_t)pt_ppage) >> 12;
      
      /*
       * The PT is now mapped in the PD mirroring
       */

      /* Invalidate TLB for the page we just added */
      invlpg(pt);
     
      /* Reset this new PT */
      memset((void*)pt, 0x0, SOS_PAGE_SIZE);
    }

  /* If we allocate a new entry in the PT, increase its reference
     count. */
  else if (! pt[index_in_pt].present)
    sos_physmem_ref_physpage_at(pd[index_in_pd].pt_paddr << 12);
  
  /* Otherwise, that means that a physical page is implicitely
     unmapped */
  else
    sos_physmem_unref_physpage(pt[index_in_pt].paddr << 12);

  /* Map the page in the page table */
  pt[index_in_pt].present = TRUE;
  pt[index_in_pt].write   = (flags & SOS_VM_MAP_PROT_WRITE)?1:0;
  pt[index_in_pt].user    = (is_user_page)?1:0;
  pt[index_in_pt].paddr   = ppage_paddr >> 12;
  sos_physmem_ref_physpage_at(ppage_paddr);

  /*
   * The page is now mapped in the current address space
   */
  
  /* Invalidate TLB for the page we just added */
  invlpg(vpage_vaddr);

  return SOS_OK;
}


sos_ret_t sos_paging_unmap(sos_vaddr_t vpage_vaddr)
{
  sos_ret_t pt_unref_retval;

  /* Get the page directory entry and table entry index for this
     address */
  unsigned index_in_pd = virt_to_pd_index(vpage_vaddr);
  unsigned index_in_pt = virt_to_pt_index(vpage_vaddr);
  
  /* Get the PD of the current context */
  struct x86_pde *pd = (struct x86_pde*)
    (SOS_PAGING_MIRROR_VADDR
     + SOS_PAGE_SIZE*virt_to_pd_index(SOS_PAGING_MIRROR_VADDR));

  /* Address of the PT in the mirroring */
  struct x86_pte * pt = (struct x86_pte*) (SOS_PAGING_MIRROR_VADDR
					   + SOS_PAGE_SIZE*index_in_pd);

  /* No page mapped at this address ? */
  if (! pd[index_in_pd].present)
    return -SOS_EINVAL;
  if (! pt[index_in_pt].present)
    return -SOS_EINVAL;

  /* The unmapping of anywhere in the PD mirroring is FORBIDDEN ;) */
  if ((vpage_vaddr >= SOS_PAGING_MIRROR_VADDR)
      && (vpage_vaddr < SOS_PAGING_MIRROR_VADDR + SOS_PAGING_MIRROR_SIZE))
    return -SOS_EINVAL;

  /* Reclaim the physical page */
  sos_physmem_unref_physpage(pt[index_in_pt].paddr << 12);

  /* Unmap the page in the page table */
  memset(pt + index_in_pt, 0x0, sizeof(struct x86_pte));

  /* Invalidate TLB for the page we just unmapped */
  invlpg(vpage_vaddr);

  /* Reclaim this entry in the PT, which may free the PT */
  pt_unref_retval = sos_physmem_unref_physpage(pd[index_in_pd].pt_paddr << 12);
  SOS_ASSERT_FATAL(pt_unref_retval >= 0);
  if (pt_unref_retval > 0)
    /* If the PT is now completely unused... */
    {
      /* Release the PDE */
      memset(pd + index_in_pd, 0x0, sizeof(struct x86_pde));
      
      /* Update the TLB */
      invlpg(pt);
    }

  return SOS_OK;  
}


int sos_paging_get_prot(sos_vaddr_t vaddr)
{
  int retval;

  /* Get the page directory entry and table entry index for this
     address */
  unsigned index_in_pd = virt_to_pd_index(vaddr);
  unsigned index_in_pt = virt_to_pt_index(vaddr);
  
  /* Get the PD of the current context */
  struct x86_pde *pd = (struct x86_pde*)
    (SOS_PAGING_MIRROR_VADDR
     + SOS_PAGE_SIZE*virt_to_pd_index(SOS_PAGING_MIRROR_VADDR));

  /* Address of the PT in the mirroring */
  struct x86_pte * pt = (struct x86_pte*) (SOS_PAGING_MIRROR_VADDR
					   + SOS_PAGE_SIZE*index_in_pd);

  /* No page mapped at this address ? */
  if (! pd[index_in_pd].present)
    return SOS_VM_MAP_PROT_NONE;
  if (! pt[index_in_pt].present)
    return SOS_VM_MAP_PROT_NONE;
  
  /* Default access right of an available page is "read" on x86 */
  retval = SOS_VM_MAP_PROT_READ;
  if (pd[index_in_pd].write && pt[index_in_pt].write)
    retval |= SOS_VM_MAP_PROT_WRITE;

  return retval;
}


sos_paddr_t sos_paging_get_paddr(sos_vaddr_t vaddr)
{
  /* Get the page directory entry and table entry index for this
     address */
  unsigned index_in_pd = virt_to_pd_index(vaddr);
  unsigned index_in_pt = virt_to_pt_index(vaddr);
  unsigned offset_in_page = virt_to_page_offset(vaddr);
  
  /* Get the PD of the current context */
  struct x86_pde *pd = (struct x86_pde*)
    (SOS_PAGING_MIRROR_VADDR
     + SOS_PAGE_SIZE*virt_to_pd_index(SOS_PAGING_MIRROR_VADDR));

  /* Address of the PT in the mirroring */
  struct x86_pte * pt = (struct x86_pte*) (SOS_PAGING_MIRROR_VADDR
					   + SOS_PAGE_SIZE*index_in_pd);

  /* No page mapped at this address ? */
  if (! pd[index_in_pd].present)
    return (sos_paddr_t)NULL;
  if (! pt[index_in_pt].present)
    return (sos_paddr_t)NULL;

  return (pt[index_in_pt].paddr << 12) + offset_in_page;
}

