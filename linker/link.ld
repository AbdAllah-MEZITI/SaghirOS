/**
 * @file link.ld
 *
 * Linker script
 *
 * @author 
 * @date 2014-05-24
 */

/* We generate binary in the ELF format */
OUTPUT_FORMAT("elf32-i386","elf32-i386","elf32-i386")
/*OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")*/

/* The entry point is _start (defined in boot.S) */
ENTRY(_start)

/* The architecture is i386 */
OUTPUT_ARCH("i386")
/*OUTPUT_ARCH("i386:x86-64)*/

SECTIONS
{
  . = 1M;
  .boot : { *(.boot) }
  .text : { *(.text) }
  .rodata : { *(.rodata) }
  .data : { *(.data) }
  .bss  : { *(.bss)  }
}
