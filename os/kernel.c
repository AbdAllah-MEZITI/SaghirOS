/* kernel.c - the C part of the kernel */
/* Copyright (C) 1999, 2010  Free Software Foundation, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "multiboot2.h"
#include "stdio.h"

/* Forward declarations.  */
//void cmain (unsigned long magic, unsigned long addr);

/* Check if MAGIC is valid and print the Multiboot information structure
   pointed by ADDR.  */
void
cmain (unsigned long magic, unsigned long addr)
{  
  struct multiboot_tag *tag;
  unsigned size;

  /* Clear the screen.  */
  cls ();

	printf("Welcom to SaghirOS\n");

  /* Am I booted by a Multiboot-compliant boot loader?  */
  if (magic != MULTIBOOT2_BOOTLOADER_MAGIC)
    {
      printf ("Invalid magic number: 0x%x\n", (unsigned) magic);
      return;
    }

  if (addr & 7)
    {
      printf ("Unaligned mbi: 0x%x\n", addr);
      return;
    }

  size = *(unsigned *) addr;
  printf ("Announced mbi size 0x%x\n", size);
  for (tag = (struct multiboot_tag *) (addr + 8);
       tag->type != MULTIBOOT_TAG_TYPE_END;
       tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag 
				       + ((tag->size + 7) & ~7)))
    {
      printf ("Tag 0x%x, Size 0x%x\n", tag->type, tag->size);
      switch (tag->type)
	{
	case MULTIBOOT_TAG_TYPE_CMDLINE:
	  printf ("Command line = %s\n",
		  ((struct multiboot_tag_string *) tag)->string);
	  break;
	case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
	  printf ("Boot loader name = %s\n",
		  ((struct multiboot_tag_string *) tag)->string);
	  break;
	case MULTIBOOT_TAG_TYPE_MODULE:
	  printf ("Module at 0x%x-0x%x. Command line %s\n",
		  ((struct multiboot_tag_module *) tag)->mod_start,
		  ((struct multiboot_tag_module *) tag)->mod_end,
		  ((struct multiboot_tag_module *) tag)->cmdline);
	  break;
	case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
	  printf ("mem_lower = %uKB, mem_upper = %uKB\n",
		  ((struct multiboot_tag_basic_meminfo *) tag)->mem_lower,
		  ((struct multiboot_tag_basic_meminfo *) tag)->mem_upper);
	  break;
	case MULTIBOOT_TAG_TYPE_BOOTDEV:
	  printf ("Boot device 0x%x,%u,%u\n",
		  ((struct multiboot_tag_bootdev *) tag)->biosdev,
		  ((struct multiboot_tag_bootdev *) tag)->slice,
		  ((struct multiboot_tag_bootdev *) tag)->part);
	  break;
	case MULTIBOOT_TAG_TYPE_MMAP:
	  {
	    multiboot_memory_map_t *mmap;

	    printf ("mmap\n");
      
	    for (mmap = ((struct multiboot_tag_mmap *) tag)->entries;
		 (multiboot_uint8_t *) mmap 
		   < (multiboot_uint8_t *) tag + tag->size;
		 mmap = (multiboot_memory_map_t *) 
		   ((unsigned long) mmap
		    + ((struct multiboot_tag_mmap *) tag)->entry_size))
	      printf (" base_addr = 0x%x%x,"
		      " length = 0x%x%x, type = 0x%x\n",
		      (unsigned) (mmap->addr >> 32),
		      (unsigned) (mmap->addr & 0xffffffff),
		      (unsigned) (mmap->len >> 32),
		      (unsigned) (mmap->len & 0xffffffff),
		      (unsigned) mmap->type);
	  }
	  break;
	case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
	  {
	    multiboot_uint32_t color;
	    unsigned i;
	    struct multiboot_tag_framebuffer *tagfb
	      = (struct multiboot_tag_framebuffer *) tag;
	    void *fb = (void *) (unsigned long) tagfb->common.framebuffer_addr;

	    switch (tagfb->common.framebuffer_type)
	      {
	      case MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED:
		{
		  unsigned best_distance, distance;
		  struct multiboot_color *palette;
	    
		  palette = tagfb->framebuffer_palette;

		  color = 0;
		  best_distance = 4*256*256;
	    
		  for (i = 0; i < tagfb->framebuffer_palette_num_colors; i++)
		    {
		      distance = (0xff - palette[i].blue) 
			* (0xff - palette[i].blue)
			+ palette[i].red * palette[i].red
			+ palette[i].green * palette[i].green;
		      if (distance < best_distance)
			{
			  color = i;
			  best_distance = distance;
			}
		    }
		}
		break;

	      case MULTIBOOT_FRAMEBUFFER_TYPE_RGB:
		color = ((1 << tagfb->framebuffer_blue_mask_size) - 1) 
		  << tagfb->framebuffer_blue_field_position;
		break;

	      case MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT:
		color = '\\' | 0x0100;
		break;

	      default:
		color = 0xffffffff;
		break;
	      }
	    
	    for (i = 0; i < tagfb->common.framebuffer_width
		   && i < tagfb->common.framebuffer_height; i++)
	      {
		switch (tagfb->common.framebuffer_bpp)
		  {
		  case 8:
		    {
		      multiboot_uint8_t *pixel = fb
			+ tagfb->common.framebuffer_pitch * i + i;
		      *pixel = color;
		    }
		    break;
		  case 15:
		  case 16:
		    {
		      multiboot_uint16_t *pixel
			= fb + tagfb->common.framebuffer_pitch * i + 2 * i;
		      *pixel = color;
		    }
		    break;
		  case 24:
		    {
		      multiboot_uint32_t *pixel
			= fb + tagfb->common.framebuffer_pitch * i + 3 * i;
		      *pixel = (color & 0xffffff) | (*pixel & 0xff000000);
		    }
		    break;

		  case 32:
		    {
		      multiboot_uint32_t *pixel
			= fb + tagfb->common.framebuffer_pitch * i + 4 * i;
		      *pixel = color;
		    }
		    break;
		  }
	      }
	    break;
	  }

	}
    }
  tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag 
				  + ((tag->size + 7) & ~7));
  printf ("Total mbi size 0x%x\n", (unsigned) tag - addr);
}    


