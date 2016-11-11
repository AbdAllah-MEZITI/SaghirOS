## Copyright (C) 2005  David Decotigny
##
## This program is free software; you can redistribute it and/or
## modify it under the terms of the GNU General Public License
## as published by the Free Software Foundation; either version 2
## of the License, or (at your option) any later version.
## 
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
## 
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
## USA. 

CC=gcc
AR=ar
CFLAGS  = -Wall -nostdinc -ffreestanding -I. -I..
LDFLAGS = -Wl,--warn-common -nostdlib -Wl,-Tldscript.lds

# Main target
all: userprogs.kimg

-include .mkvars


PWD := $(shell pwd)

# Programs generation
$(PROGS):
	$(CC) -static $(LDFLAGS) -o $@ $^

# Generation of the libC
libc.a: libc.o

# Create a program image to be integrated into the Kernel
userprogs.kimg: $(PROGS)
	@echo "# Generating ELF images for inclusion into the kernel image: $@"
	@echo "SECTIONS { .rodata . : { " > .userprogs.lds
	@i=0 ;                                                              \
	 for f in $^ ; do                                                   \
           i=`expr $$i + 1` ;                                               \
	   echo "extern char _begin_userprog$$i, _end_userprog$$i;"         \
                > .userprog$$i.c ;                                          \
	   echo "char *_userprog"$$i"_entry[]" >> .userprog$$i.c ;          \
	   echo "  __attribute__((section(\".userprogs_table\")))"          \
                >> .userprog$$i.c ;                                         \
	   echo "  = { \"$$f\", &_begin_userprog$$i, &_end_userprog$$i };"  \
                >> .userprog$$i.c ;                                         \
	   $(CC) $(CFLAGS) -c .userprog$$i.c -o .userprog$$i.o ;            \
           objcopy --add-section .userprog$$i=$$f .userprog$$i.o            \
                .userprog$$i.kimg ;                                         \
           echo "  _begin_userprog$$i = .;" >> .userprogs.lds ;             \
	   echo "  .userprog$$i.kimg(.userprog$$i);" >> .userprogs.lds ;    \
	   echo "  _end_userprog$$i = .;" >> .userprogs.lds ;               \
	   echo "  .userprog$$i.kimg(.rodata); .userprog$$i.kimg(.data);"   \
                >> .userprogs.lds ;                                         \
         done
	@echo "  _userprogs_table = .; *(.userprogs_table) ; LONG(0);"      \
                >> .userprogs.lds
	@echo "} /DISCARD/ : { *(.text) *(.data) *(.bss) } }"               \
                >> .userprogs.lds
	@ld -r -o $@ -T.userprogs.lds

# Create libraries from object files
%.a:
	$(AR) rcv $@ $^

# Create objects from C source code
%.o: %.c
	$(CC) -I$(PWD) -c $< $(CFLAGS) -o $@

# Create objects from assembler (.S) source code
%.o: %.S
	$(CC) -I$(PWD) -c $< $(CFLAGS) -DASM_SOURCE=1 -o $@

# Clean directory
clean:
	$(RM) *.o *.a *~ $(PROGS) *.kimg
	$(RM) .userprog*
