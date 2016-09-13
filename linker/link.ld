/**
 * @file link.ld
 *
 * Linker script
 *
 * @author AbdAllah MEZITI
 * @date 2016-03-29
 */

/* We generate binary in the ELF format */
OUTPUT_FORMAT("elf32-i386","elf32-i386","elf32-i386")

/* The entry point is _start (defined in boot.S) */
ENTRY(_start)

/* The architecture is i386 */
OUTPUT_ARCH("i386")

/* --------- */
/* SECTIONS  */
/* --------- */
SECTIONS
{
	/* our kernel is loaded at 0x200000 */
	. = 2M;
	__b_load   = .;

		/* ------------------- */
		/* SECTION : MULIBOOT  */
		/* ------------------- */
		/* the multiboot header MUST come early enough in the output object file */
		.multiboot :
		{	*(.multiboot);

			/*
			 * With the following line, we force this section to be
			 * allocated in the output file as soon as possible, no matter
			 * when the file containing the multiboot header (multiboot.S)
			 * is compiled. This is to conform to the multiboot spec, which
			 * says "The Multiboot header must be contained completely
			 * within the first 8192 bytes of the OS image, and must be
			 * longword (32-bit) aligned."
			 */
			LONG(0);
		}

	/* Defines a symbol '__b_kernel to mark the start of the kernel code/data */
	. = ALIGN(4096);
	__b_kernel = .;
  

		/* ------------------- */
		/* SECTION : TEXT ---- */
		/* ------------------- */
		.text ALIGN(4096):
		{	*(.text)

			/* Defines the 'etext' and '_etext' at the end */
			PROVIDE(etext = .);
			PROVIDE(_etext = .);
		}

		/* ------------------- */
		/* SECTION : DATA ---- */
		/* ------------------- */
		.data :
		{	*(.data)

			PROVIDE(edata = .);
			PROVIDE(_edata = .);
		}

		/* ------------------- */
		/* SECTION : RODATA -- */
		/* ------------------- */
		.rodata :
		{	*(.rodata)
			*(.rodata.*)

			PROVIDE(erodata = .);
			PROVIDE(_erodata = .);
		}

	/* We take note of the end of the data to load */
	__e_load = .;

		/* ------------------- */
		/* SECTION : BSS ----- */
		/* ------------------- */
		/* provide an address */
		.bss ADDR(.rodata) + SIZEOF(.rodata): /* : */
		{	*(.bss)
			*(COMMON)
			
			/* We put the stack of the bootstrap thread on a page boundary, because it can be un-allocater later */
			. = ALIGN(4096);
			*(.init_stack)

			PROVIDE(ebss = .);
			PROVIDE(_ebss = .);
		}

	/* We take note of the end of the kernel */
	__e_kernel = .;

		/* ------------------- */
		/* SECTION : GCC ----- */
		/* ------------------- */
        /DISCARD/ :{
                *(.note*)
                *(.indent)
                *(.comment)
                *(.stab)
                *(.stabstr)
        }
}
