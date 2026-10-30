/* Link everything into one code hunk plus one BSS hunk.
 * tools/elf2hunk.py turns the result into an AmigaDOS executable. */
OUTPUT_FORMAT("elf32-m68k")
ENTRY(_start)
SECTIONS
{
  . = 0;
  CODE : {
    KEEP(*(.text.entry)) *(.text.startup) *(.text .text.*)
    __text_end = .;
    . = ALIGN(4);
    *(.rodata .rodata.*)
    . = ALIGN(4);
    *(.data .data.*)
    . = ALIGN(4);
  }
  . = 0x40000000;
  BSS (NOLOAD) : {
    *(.bss .bss.*) *(COMMON)
    . = ALIGN(4);
  }
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.gnu*) }
}
