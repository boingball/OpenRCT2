/*
 * Ami3D's demo - AmigaOS startup and OS call glue (MinTee Golf's).
 *
 * The game is built freestanding with a stock m68k ELF gcc and converted to
 * an AmigaDOS hunk executable by tools/elf2hunk.py, so there is no libnix,
 * no NDK and no amiga-gcc.  This file is everything the C code needs from
 * the OS: CLI/Workbench startup, a CPU check, a private stack, and a handful
 * of library calls wrapped as ordinary C functions (args on the stack,
 * result in d0, d0-d1/a0-a1 scratch - the normal gcc m68k convention).
 *
 * This file is plain 68000 code: the CPU check has to run before anything
 * that was compiled for the 68020.  The OCS edition (-DOCS) is all 68000
 * code and skips the check.
 */

#include "asm.h"

        .equ    LVO_Forbid,      -132
        .equ    LVO_Permit,      -138
        .equ    LVO_AllocMem,    -198
        .equ    LVO_FreeMem,     -210
        .equ    LVO_FindTask,    -294
        .equ    LVO_GetMsg,      -372
        .equ    LVO_ReplyMsg,    -378
        .equ    LVO_WaitPort,    -384
        .equ    LVO_CloseLibrary,-414
        .equ    LVO_OpenLibrary, -552
        .equ    LVO_CacheClearU, -636

        .equ    LVO_LoadView,    -222
        .equ    LVO_WaitBlit,    -228
        .equ    LVO_WaitTOF,     -270
        .equ    LVO_OwnBlitter,  -456
        .equ    LVO_DisownBlitter,-462

        .equ    LVO_Open,        -30
        .equ    LVO_Close,       -36
        .equ    LVO_Read,        -42
        .equ    LVO_Write,       -48
        .equ    LVO_Output,      -60

#ifdef OCS
/* The OCS edition's code and data have to fit in an A500's 512K of
 * expansion memory beside the system's, or they go into chip RAM and
 * leave too little of it for the screens and the tune.  Its deepest
 * calls take about 4K of stack, and nothing recurses: 24K is plenty. */
        .equ    STACK_SIZE,      24576
#else
        .equ    STACK_SIZE,      65536
#endif

        SECTION_ENTRY
        .globl  _start
_start:
        /* From the Shell: a0 = argument line, d0 = its length. */
        move.l  %a0,SYM(cli_args)
        move.l  %d0,SYM(cli_arglen)
        movem.l %d2-%d7/%a2-%a6,-(%sp)
        move.l  %sp,saved_sp
        move.l  4.w,%a6
        move.l  %a6,SYM(SysBase)

        /* Workbench launch? Then collect the startup message first. */
        sub.l   %a1,%a1
        jsr     LVO_FindTask(%a6)
        move.l  %d0,%a4
        tst.l   172(%a4)                /* pr_CLI */
        bne.s   1f
        lea     92(%a4),%a0             /* pr_MsgPort */
        jsr     LVO_WaitPort(%a6)
        lea     92(%a4),%a0
        jsr     LVO_GetMsg(%a6)
        move.l  %d0,SYM(WBenchMsg)
        clr.l   SYM(cli_arglen)         /* no argument line from Workbench */
1:
        /* Open dos.library for messages. */
        lea     dosname,%a1
        moveq   #0,%d0
        jsr     LVO_OpenLibrary(%a6)
        move.l  %d0,SYM(DOSBase)

#ifndef OCS
        /* 68020 or better? (AttnFlags bit AFB_68020) */
        move.b  297(%a6),%d0            /* 020, 030, 040 or 060 bits */
        and.b   #0x8e,%d0
        bne.s   2f
        /* No flags (AROS leaves them clear on some CPUs): the 68000 and
         * 68010 ignore the index scale factor, the 68020 honours it. */
        moveq   #1,%d0
        sub.l   %a0,%a0
        lea     0(%a0,%d0.w*4),%a0
        cmp.w   #4,%a0
        beq.s   2f
        pea     cpumsg_len
        pea     cpumsg
        jsr     SYM(os_print)
        addq.l  #8,%sp
        moveq   #20,%d7
        bra.s   9f
2:
#endif
        /* Run the game on a private stack: the CLI default is 4K. */
        lea     stack_top,%sp
        jsr     SYM(main)
        move.l  saved_sp,%sp
        move.l  %d0,%d7
9:
        move.l  SYM(SysBase),%a6
        move.l  SYM(DOSBase),%d0
        beq.s   3f
        move.l  %d0,%a1
        jsr     LVO_CloseLibrary(%a6)
3:
        move.l  SYM(WBenchMsg),%d0
        beq.s   4f
        jsr     LVO_Forbid(%a6)
        move.l  SYM(WBenchMsg),%a1
        jsr     LVO_ReplyMsg(%a6)
4:
        move.l  %d7,%d0
        movem.l (%sp)+,%d2-%d7/%a2-%a6
        rts

/* void os_print(const char *s, long len) */
        .globl  SYM(os_print)
SYM(os_print):
        movem.l %d2-%d3/%a6,-(%sp)
        move.l  SYM(DOSBase),%d0
        beq.s   1f
        tst.l   SYM(WBenchMsg)
        bne.s   1f
        move.l  %d0,%a6
        jsr     LVO_Output(%a6)
        move.l  %d0,%d1
        beq.s   1f
        move.l  16(%sp),%d2
        move.l  20(%sp),%d3
        jsr     LVO_Write(%a6)
1:      movem.l (%sp)+,%d2-%d3/%a6
        rts

/* void *os_alloc(unsigned long size, unsigned long flags) */
        .globl  SYM(os_alloc)
SYM(os_alloc):
        move.l  %a6,-(%sp)
        move.l  8(%sp),%d0
        move.l  12(%sp),%d1
        move.l  SYM(SysBase),%a6
        jsr     LVO_AllocMem(%a6)
        move.l  %d0,%a0
        move.l  (%sp)+,%a6
        rts

/* void os_free(void *p, unsigned long size) */
        .globl  SYM(os_free)
SYM(os_free):
        move.l  %a6,-(%sp)
        move.l  8(%sp),%a1
        move.l  12(%sp),%d0
        move.l  SYM(SysBase),%a6
        jsr     LVO_FreeMem(%a6)
        move.l  (%sp)+,%a6
        rts

/* void *os_openlib(const char *name, long version) */
        .globl  SYM(os_openlib)
SYM(os_openlib):
        move.l  %a6,-(%sp)
        move.l  8(%sp),%a1
        move.l  12(%sp),%d0
        move.l  SYM(SysBase),%a6
        jsr     LVO_OpenLibrary(%a6)
        move.l  %d0,%a0
        move.l  (%sp)+,%a6
        rts

/* void os_closelib(void *base) */
        .globl  SYM(os_closelib)
SYM(os_closelib):
        move.l  %a6,-(%sp)
        move.l  8(%sp),%a1
        move.l  SYM(SysBase),%a6
        jsr     LVO_CloseLibrary(%a6)
        move.l  (%sp)+,%a6
        rts

        .globl  SYM(os_forbid)
SYM(os_forbid):
        move.l  %a6,-(%sp)
        move.l  SYM(SysBase),%a6
        jsr     LVO_Forbid(%a6)
        move.l  (%sp)+,%a6
        rts

        .globl  SYM(os_permit)
SYM(os_permit):
        move.l  %a6,-(%sp)
        move.l  SYM(SysBase),%a6
        jsr     LVO_Permit(%a6)
        move.l  (%sp)+,%a6
        rts

/* void os_cacheclear(void) - only on Kickstart 2.0+ */
        .globl  SYM(os_cacheclear)
SYM(os_cacheclear):
        move.l  %a6,-(%sp)
        move.l  SYM(SysBase),%a6
        cmp.w   #37,20(%a6)             /* lib_Version */
        blo.s   1f
        jsr     LVO_CacheClearU(%a6)
1:      move.l  (%sp)+,%a6
        rts

/* graphics.library: void gfx_loadview(void *gfxbase, void *view) */
        .globl  SYM(gfx_loadview)
SYM(gfx_loadview):
        move.l  %a6,-(%sp)
        move.l  8(%sp),%a6
        move.l  12(%sp),%a1
        jsr     LVO_LoadView(%a6)
        move.l  (%sp)+,%a6
        rts

/* void gfx_waittof(void *gfxbase) */
        .globl  SYM(gfx_waittof)
SYM(gfx_waittof):
        move.l  %a6,-(%sp)
        move.l  8(%sp),%a6
        jsr     LVO_WaitTOF(%a6)
        move.l  (%sp)+,%a6
        rts

/* void gfx_ownblitter(void *gfxbase) - also waits for the blitter */
        .globl  SYM(gfx_ownblitter)
SYM(gfx_ownblitter):
        move.l  %a6,-(%sp)
        move.l  8(%sp),%a6
        jsr     LVO_OwnBlitter(%a6)
        jsr     LVO_WaitBlit(%a6)
        move.l  (%sp)+,%a6
        rts

/* void gfx_disownblitter(void *gfxbase) */
        .globl  SYM(gfx_disownblitter)
SYM(gfx_disownblitter):
        move.l  %a6,-(%sp)
        move.l  8(%sp),%a6
        jsr     LVO_DisownBlitter(%a6)
        move.l  (%sp)+,%a6
        rts

/* Any library call: long os_call(void *base, long lvo, const long *regs)
 * with regs[] = d0-d7, a0-a5 to load before the call (a6 = base).  The
 * result comes back in d0 (and a0, for pointers). */
        .globl  SYM(os_call)
SYM(os_call):
        movem.l %d2-%d7/%a2-%a6,-(%sp)
        move.l  48(%sp),%a6
        move.l  52(%sp),%d0
        move.l  56(%sp),%a5
        pea     1f(%pc)
        lea     0(%a6,%d0.l),%a0
        move.l  %a0,-(%sp)
        movem.l (%a5),%d0-%d7/%a0-%a5
        rts                             /* into the library; back at 1: */
1:      move.l  %d0,%a0
        movem.l (%sp)+,%d2-%d7/%a2-%a6
        rts

/* dos.library file access for the options file.
 * long dos_open(const char *name, long mode) */
        .globl  SYM(dos_open)
SYM(dos_open):
        movem.l %d2/%a6,-(%sp)
        move.l  SYM(DOSBase),%a6
        move.l  12(%sp),%d1
        move.l  16(%sp),%d2
        jsr     LVO_Open(%a6)
        movem.l (%sp)+,%d2/%a6
        rts

/* void dos_close(long fh) */
        .globl  SYM(dos_close)
SYM(dos_close):
        move.l  %a6,-(%sp)
        move.l  SYM(DOSBase),%a6
        move.l  8(%sp),%d1
        jsr     LVO_Close(%a6)
        move.l  (%sp)+,%a6
        rts

/* long dos_read(long fh, void *buf, long len) */
        .globl  SYM(dos_read)
SYM(dos_read):
        movem.l %d2-%d3/%a6,-(%sp)
        move.l  SYM(DOSBase),%a6
        move.l  16(%sp),%d1
        move.l  20(%sp),%d2
        move.l  24(%sp),%d3
        jsr     LVO_Read(%a6)
        movem.l (%sp)+,%d2-%d3/%a6
        rts

/* long dos_write(long fh, const void *buf, long len) */
        .globl  SYM(dos_write)
SYM(dos_write):
        movem.l %d2-%d3/%a6,-(%sp)
        move.l  SYM(DOSBase),%a6
        move.l  16(%sp),%d1
        move.l  20(%sp),%d2
        move.l  24(%sp),%d3
        jsr     LVO_Write(%a6)
        movem.l (%sp)+,%d2-%d3/%a6
        rts

        SECTION_RODATA
dosname:
        .asciz  "dos.library"
cpumsg:
        .ascii  "OpenRCT2 graphics runner needs 68020+; 68060, 64 MB and RTG recommended.\n"
        .equ    cpumsg_len, . - cpumsg
        .even

        .bss
        .globl  SYM(SysBase)
        .globl  SYM(DOSBase)
        .globl  SYM(WBenchMsg)
        .globl  SYM(cli_args)
        .globl  SYM(cli_arglen)
SYM(SysBase):        .skip 4
SYM(cli_args):       .skip 4
SYM(cli_arglen):     .skip 4
SYM(DOSBase):        .skip 4
SYM(WBenchMsg):      .skip 4
saved_sp:       .skip 4
        .balign 4
stack_bottom:   .skip STACK_SIZE
stack_top:

        NO_EXEC_STACK
