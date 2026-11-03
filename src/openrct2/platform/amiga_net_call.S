/* SPDX-License-Identifier: GPL-3.0-only
 * AmiLAN bridge: d0-d7/a0-a5 from regs[14], a6 library base. */
.text
.globl _os_call
_os_call:
    movem.l %d2-%d7/%a2-%a6,-(%sp)
    move.l 48(%sp),%a6
    move.l %a6,%a1
    add.l 52(%sp),%a1
    move.l 56(%sp),%a0
    pea .Lnet_return(%pc)
    move.l %a1,-(%sp)
    movem.l (%a0),%d0-%d7/%a0-%a5
    rts
.Lnet_return:
    movem.l (%sp)+,%d2-%d7/%a2-%a6
    rts
