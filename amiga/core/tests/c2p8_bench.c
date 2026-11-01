/* SPDX-License-Identifier: GPL-3.0-only */
#include "../../../src/openrct2-ui/platform/amiga/amiga_c2p8.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Conversion only, using Fast-RAM buffers: emulator comparison is not a
 * physical AGA bandwidth/gameplay benchmark. */
int main(void)
{
    const unsigned pixels = 640 * 512;
    uint8_t* src = malloc(pixels);
    uint8_t* dst = malloc(pixels);
    unsigned i, round;
    uint32_t hash = 2166136261u;
    if (!src || !dst) return 10;
    for (i = 0; i < pixels; ++i) src[i] = (uint8_t)(i * 13 + i / 16);
    for (round = 0; round < 24; ++round)
    {
        src[round] ^= (uint8_t)round;
        amiga_c2p8_rect(src, 640, dst, 80, 80 * 512, 0, 0, 640, 512);
    }
    for (i = 0; i < pixels; ++i) hash = (hash ^ dst[i]) * 16777619u;
    printf("C2P8 full-PAL conversion x24 hash %08lx\n", (unsigned long)hash);
    free(dst); free(src);
    return 0;
}
