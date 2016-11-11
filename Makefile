## Copyright (C) 2004,2005  The SOS Team
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
# compiler, compiler options
CC=gcc
CFLAGS  = -m32 -g -O3 -Wall -nostdlib -nostdinc -ffreestanding -fno-asynchronous-unwind-tables -DKERNEL_SOS -I . -I include -I lib -I os

ASM=gcc
ASMFLAGS= -m32 -g -I . -I include/

LD=ld
LDFLAGS= --warn-common 
LDFLAGS+= -n
LDFLAGS+= -m elf_i386
LDFLAGS+= -T linker/link.ld


TARGET=SaghirOS
CDROM=$(TARGET).iso

# files and directories
build_dir=build
kernel_name=$(build_dir)/kernel


ASM_SOURCES= $(shell find ./boot ./hwcore -type f -name '*.S')
C_SOURCES= $(shell find ./os ./lib ./hwcore -type f -name '*.c')

OBJECTS= $(ASM_SOURCES:.S=.o)
OBJECTS+=$(C_SOURCES:.c=.o)


.PHONY: all clean run debug doc

all: $(kernel_name)

FORCE:
	@

# clean
clean:
	rm -rf $(OBJECTS)
	rm -rf $(build_dir)
	rm -rf $(CDROM)

# lunch qemu and boot from the .iso cdrom
run: $(kernel_name)
	qemu-system-i386 -kernel $(kernel_name)
runiso: iso
	qemu-system-i386 -cdrom $(CDROM)

# debugging with ddd https://www.gnu.org/software/ddd/
# make debug
# launch ddd
#   ddd console (gdb): target remote localhost:1234
#   ddd console (gdb): file path_to_[kernel_name]
#   ddd console (gdb): break cmain (or use the GUI)
#   ddd console (gdb): continue  
debug: iso
	qemu-system-i386 -s -S -cdrom $(CDROM)
#debug: $(kernel_name)
#	qemu-system-i386 -s -S -kernel $(kernel_name)

# Create the .iso cdrom image for booting from qemu
iso:$(kernel_name)
	mkdir	$(build_dir)/isodir
	mkdir	$(build_dir)/isodir/boot
	cp	$(kernel_name)	$(build_dir)/isodir/boot/$(TARGET).bin
	mkdir	$(build_dir)/isodir/boot/grub
	cp	tools/grub.cfg $(build_dir)/isodir/boot/grub/grub.cfg
	grub-mkrescue -o $(CDROM) $(build_dir)/isodir

doc:
	doxygen

# linker
$(kernel_name) : $(OBJECTS) output_dir
	echo $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@  $(OBJECTS)
	-nm -C $@ | cut -d ' ' -f 1,3 > $(kernel_name).map
	size $@
	objdump -xDts $(kernel_name) > $(build_dir)/dump.txt

# Create objects from assembler (.c) source code
%.o: %.S output_dir
	$(CC) -c $< $(ASMFLAGS) -DASM_SOURCE=1 -o $@

# Create objects from assembler (.S) source code
%.o : %.c output_dir
	$(CC) $(CFLAGS) -c $*.c -o $@

output_dir:
	mkdir -p $(build_dir)

