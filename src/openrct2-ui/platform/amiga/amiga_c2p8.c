/* SPDX-License-Identifier: GPL-3.0-only
 * Eight-plane adaptation of Ami3D's src/c2p.c transpose (boingball).
 * Fast-RAM planes plus a shadow of the Chip bitmap apply its PLANAR5FS
 * presentation technique to OpenRCT2's full 256-colour framebuffer.
 */
#include "amiga_c2p8.h"
#include <string.h>

#define MERGE(a, b, s, m) do { uint32_t t = (((a) >> (s)) ^ (b)) & (m); (b) ^= t; (a) ^= t << (s); } while (0)

static uint32_t load_be(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static void store_be(uint8_t* p, uint32_t value)
{
#ifdef __m68k__
    /* AllocVec, planeSize, row pitch and word x are all longword aligned. */
    *(uint32_t*)p = value;
#else
    p[0] = (uint8_t)(value >> 24); p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8); p[3] = (uint8_t)value;
#endif
}

static void convert32(const uint8_t* src, uint8_t* dst, int planeSize)
{
    uint32_t x7 = load_be(src), x6 = load_be(src + 4), x5 = load_be(src + 8), x4 = load_be(src + 12);
    uint32_t x3 = load_be(src + 16), x2 = load_be(src + 20), x1 = load_be(src + 24), x0 = load_be(src + 28);
    MERGE(x0,x4,16,0x0000ffffUL); MERGE(x1,x5,16,0x0000ffffUL);
    MERGE(x2,x6,16,0x0000ffffUL); MERGE(x3,x7,16,0x0000ffffUL);
    MERGE(x0,x2,8,0x00ff00ffUL); MERGE(x1,x3,8,0x00ff00ffUL);
    MERGE(x4,x6,8,0x00ff00ffUL); MERGE(x5,x7,8,0x00ff00ffUL);
    MERGE(x0,x1,4,0x0f0f0f0fUL); MERGE(x2,x3,4,0x0f0f0f0fUL);
    MERGE(x4,x5,4,0x0f0f0f0fUL); MERGE(x6,x7,4,0x0f0f0f0fUL);
    MERGE(x0,x4,2,0x33333333UL); MERGE(x1,x5,2,0x33333333UL);
    MERGE(x2,x6,2,0x33333333UL); MERGE(x3,x7,2,0x33333333UL);
    MERGE(x0,x2,1,0x55555555UL); MERGE(x1,x3,1,0x55555555UL);
    MERGE(x4,x6,1,0x55555555UL); MERGE(x5,x7,1,0x55555555UL);
    store_be(dst, x0); store_be(dst + planeSize, x2);
    store_be(dst + 2 * planeSize, x4); store_be(dst + 3 * planeSize, x6);
    store_be(dst + 4 * planeSize, x1); store_be(dst + 5 * planeSize, x3);
    store_be(dst + 6 * planeSize, x5); store_be(dst + 7 * planeSize, x7);
}

void amiga_c2p8_rect(const uint8_t* src, int srcPitch, uint8_t* planes,
    int planarPitch, int planeSize, int x, int y, int w, int h)
{
    int row;
    for (row = 0; row < h; ++row)
    {
        const uint8_t* in = src + row * srcPitch;
        int xx = x, left = w;
        while (left > 0)
        {
            uint8_t* out = planes + (y + row) * planarPitch + xx / 8;
            if ((xx & 31) == 0 && left >= 32)
            {
                convert32(in, out, planeSize);
                xx += 32; in += 32; left -= 32;
            }
            else
            {
                unsigned plane;
                uint8_t mask = (uint8_t)(0x80u >> (xx & 7));
                for (plane = 0; plane < 8; ++plane)
                {
                    uint8_t* p = out + plane * planeSize;
                    *p = (uint8_t)((*p & ~mask) | ((*in & (1u << plane)) ? mask : 0));
                }
                ++xx; ++in; --left;
            }
        }
    }
}

unsigned amiga_c2p8_present(const uint8_t* planes, uint8_t* shadow, uint8_t* const chip[8],
    int planarPitch, int planeSize, int x, int y, int w, int h)
{
    unsigned changed = 0;
    int plane, row, word;
    const int first = x / 32, last = (x + w + 31) / 32;
    for (plane = 0; plane < 8; ++plane)
        for (row = y; row < y + h; ++row)
            for (word = first; word < last; ++word)
            {
                const int offset = row * planarPitch + word * 4;
                uint32_t value, old;
                memcpy(&value, planes + plane * planeSize + offset, 4);
                memcpy(&old, shadow + plane * planeSize + offset, 4);
                if (value != old)
                {
                    memcpy(shadow + plane * planeSize + offset, &value, 4);
                    memcpy(chip[plane] + offset, &value, 4);
                    ++changed;
                }
            }
    return changed;
}
