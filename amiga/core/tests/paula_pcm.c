/* SPDX-License-Identifier: GPL-3.0-only */
#include "../../../src/openrct2-ui/platform/amiga/amiga_paula_pcm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    unsigned char pcm[2048 * 4 + 2], left[2050], right[2050];
    unsigned i;
    memset(left, 0xa5, sizeof left); memset(right, 0x5a, sizeof right);
    /* Unaligned source, distinct L/R and low bytes; every signed high-byte value. */
    for (i = 0; i < 2048; ++i)
    {
        pcm[1 + i*4] = (unsigned char)i;
        pcm[2 + i*4] = 255 - (unsigned char)i;
        pcm[3 + i*4] = (unsigned char)(i * 37 + 19);
        pcm[4 + i*4] = (unsigned char)(i * 13);
    }
    amiga_paula_split(pcm + 1, left + 1, right + 1, 2048);
    for (i = 0; i < 2048; ++i)
    {
        assert(left[i+1] == (unsigned char)i);
        assert(right[i+1] == (unsigned char)(i * 37 + 19));
    }
    assert(left[0] == 0xa5 && left[2049] == 0xa5);
    assert(right[0] == 0x5a && right[2049] == 0x5a);
    amiga_paula_split(NULL, NULL, NULL, 0);
    puts("Paula PCM: all signed sample high bytes, stereo separation, unaligned input and guards PASS");
    return 0;
}
