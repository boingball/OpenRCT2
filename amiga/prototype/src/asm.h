/* Ami3D - assembler glue for both supported toolchains.
 *
 * m68k-linux-gnu-gcc (ELF, turned into a hunk file by tools/elf2hunk.py)
 * and m68k-amigaos-gcc (hunk output directly) differ in two ways that
 * matter to the .S files: amiga-gcc puts an underscore in front of every
 * C symbol, and its objects only know the plain code/data/bss sections. */
#ifndef ASM_H
#define ASM_H

#define ASM_CAT_(a, b) a##b
#define ASM_CAT(a, b) ASM_CAT_(a, b)
#define SYM(x) ASM_CAT(__USER_LABEL_PREFIX__, x)

#if defined(__amigaos__) || defined(__AMIGA__) || defined(AMIGA)
#define SECTION_ENTRY  .text      /* start.o is linked first */
#define SECTION_RODATA .data
#define NO_EXEC_STACK
#else
#define SECTION_ENTRY  .section .text.entry,"ax"
#define SECTION_RODATA .section .rodata
#define NO_EXEC_STACK  .section .note.GNU-stack,"",%progbits
#endif

#endif
