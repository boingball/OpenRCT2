/* SPDX-License-Identifier: GPL-3.0-only */
.text
.globl _bridge_vector
_bridge_vector:
    movem.l %d0-%d7/%a0-%a6,_bridge_seen
    move.l #0x12345678,%d0
    rts
