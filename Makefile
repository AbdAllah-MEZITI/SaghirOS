# compiler, compiler options
CC=gcc
CFLAGS  = -m32 -O3 -Wall -nostdlib -nostdinc -ffreestanding -fno-asynchronous-unwind-tables -DKERNEL_SOS -I . -I include -I lib

ASM=gcc
ASMFLAGS= -m32 -I . -I include/

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

debug: $(kernel_name)
	qemu-system-i386 -s -S -kernel $(kernel_name)

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
	objdump -xDts $(kernel_name) > $(build_dir)/dump.txt

# Create objects from assembler (.c) source code
%.o: %.S output_dir
	$(CC) -c $< $(ASMFLAGS) -DASM_SOURCE=1 -o $@

# Create objects from assembler (.S) source code
%.o : %.c output_dir
	$(CC) $(CFLAGS) -c $*.c -o $@

output_dir:
	mkdir -p $(build_dir)

