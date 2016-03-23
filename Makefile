CC=gcc
#CFLAGS= -Wall -Werror -pedantic -g -I. -I include/
CFLAGS  = -m32 -O3 -Wall -nostdlib -nostdinc -ffreestanding -DKERNEL_SOS -I include/
ASM=gcc
ASMFLAGS= -m32 -I include/
LD=ld
LDFLAGS= --warn-common
LDFLAGS+= -n
LDFLAGS+= -m elf_i386
LDFLAGS+= -T linker/link.ld
#LDFLAGS+= -T linker/sos.lds


build_dir=build
kernel_name=$(build_dir)/kernel

#ASM_SOURCES= $(shell find . -type f -name '*.S')
ASM_SOURCES= boot/boot.S
#C_SOURCES= $(shell find . -type f -name '*.c')
C_SOURCES= os/kernel.c

OBJECTS= $(ASM_SOURCES:.S=.o)
OBJECTS+=$(C_SOURCES:.c=.o)

.PHONY: all clean run debug doc

all: $(kernel_name)

clean:
	rm $(OBJECTS) $(kernel_name)
	rm myos.iso
	rm -rf $(build_dir)/isodir

run: $(kernel_name)
	qemu-system-i386 -kernel $(kernel_name)
runiso: iso
	qemu-system-i386 -cdrom myos.iso

debug: $(kernel_name)
	qemu-system-i386 -s -S -kernel $(kernel_name)

iso:$(kernel_name)
	mkdir	$(build_dir)/isodir
	mkdir	$(build_dir)/isodir/boot
	cp	$(kernel_name)	$(build_dir)/isodir/boot/myOS.bin
	mkdir	$(build_dir)/isodir/boot/grub
	cp	tools/grub.cfg $(build_dir)/isodir/boot/grub/grub.cfg
	grub-mkrescue -o myos.iso $(build_dir)/isodir

doc:
	doxygen

$(kernel_name) : $(OBJECTS)
	echo $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@  $(OBJECTS)
	-nm -C $@ | cut -d ' ' -f 1,3 > $(kernel_name).map
	objdump -xDts $(kernel_name) > $(build_dir)/dump.txt

#%.o : %.S
#	$(ASM) $(ASMFLAGS)  $*.S -o $@
# Create objects from assembler (.S) source code
%.o: %.S
	$(CC) -c $< $(ASMFLAGS) -DASM_SOURCE=1 -o $@

%.o : %.c
	$(CC) $(CFLAGS) -c $*.c -o $@

