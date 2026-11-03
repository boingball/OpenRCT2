/* SPDX-License-Identifier: GPL-3.0-only
 * Eight-plane adaptation of Ami3D's src/c2p.c transpose (boingball).
 *
 * One pass per 32-pixel group: the group's chunky pixels are compared with a
 * Fast-RAM chunky copy of what the Chip planes currently show, and only a
 * group that changed is transposed and written to Chip RAM. Unchanged parts
 * of a dirty rectangle therefore cost two 32-byte reads, no transpose and no
 * Chip access. The chunky shadow is the only Fast-RAM buffer needed, and it
 * also supplies the untouched pixels of a partially covered group, so Chip
 * RAM is never read.
 */
#include "amiga_c2p8.h"
#include <string.h>

#define MERGE(a, b, s, m) do { uint32_t t = (((a) >> (s)) ^ (b)) & (m); (b) ^= t; (a) ^= t << (s); } while (0)

static uint32_t load_be(const uint8_t* p)
{
#ifdef __m68k__
    /* The source is already big-endian. memcpy permits unaligned scanlines
     * without aliasing/alignment UB and GCC emits one 68020+ longword load. */
    uint32_t value;
    memcpy(&value, p, sizeof(value));
    return value;
#else
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
#endif
}

static void store_be(uint8_t* p, uint32_t value)
{
#ifdef __m68k__
    /* Chip plane bases, the row pitch and every group offset are longword aligned. */
    *(uint32_t*)p = value;
#else
    p[0] = (uint8_t)(value >> 24); p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8); p[3] = (uint8_t)value;
#endif
}

static uint32_t load_ne(const uint8_t* p)
{
    uint32_t value;
    memcpy(&value, p, sizeof(value));
    return value;
}

static void store_ne(uint8_t* p, uint32_t value)
{
    memcpy(p, &value, sizeof(value));
}

/* Transposes 32 chunky pixels (big-endian longwords x7..x0) into one longword per plane. */
static void convert32(uint32_t x7, uint32_t x6, uint32_t x5, uint32_t x4, uint32_t x3, uint32_t x2, uint32_t x1,
    uint32_t x0, uint8_t* const chip[8], int offset)
{
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
    store_be(chip[0] + offset, x0); store_be(chip[1] + offset, x2);
    store_be(chip[2] + offset, x4); store_be(chip[3] + offset, x6);
    store_be(chip[4] + offset, x1); store_be(chip[5] + offset, x3);
    store_be(chip[6] + offset, x5); store_be(chip[7] + offset, x7);
}

static void convert_from(const uint8_t* p, uint8_t* const chip[8], int offset)
{
    convert32(load_be(p), load_be(p + 4), load_be(p + 8), load_be(p + 12), load_be(p + 16), load_be(p + 20),
        load_be(p + 24), load_be(p + 28), chip, offset);
}

unsigned amiga_c2p8_update(const uint8_t* src, int srcPitch, uint8_t* shadow, int shadowPitch,
    uint8_t* const chip[8], int planarPitch, int x, int y, int w, int h)
{
    unsigned changed = 0;
    const int end = x + w;
    const int firstGroup = x & ~31;
    int row;
    for (row = 0; row < h; ++row)
    {
        const uint8_t* in = src + row * srcPitch; /* pixel x of this row */
        uint8_t* sh = shadow + (y + row) * shadowPitch;
        const int chipRow = (y + row) * planarPitch;
        int gx;
        for (gx = firstGroup; gx < end; gx += 32)
        {
            const int a = gx < x ? x : gx;
            const int b = gx + 32 > end ? end : gx + 32;
            if (b - a == 32)
            {
                const uint8_t* s = in + (gx - x);
                uint8_t* d = sh + gx;
                const uint32_t s0 = load_ne(s), s1 = load_ne(s + 4), s2 = load_ne(s + 8), s3 = load_ne(s + 12);
                const uint32_t s4 = load_ne(s + 16), s5 = load_ne(s + 20), s6 = load_ne(s + 24), s7 = load_ne(s + 28);
                if (((s0 ^ load_ne(d)) | (s1 ^ load_ne(d + 4)) | (s2 ^ load_ne(d + 8)) | (s3 ^ load_ne(d + 12))
                        | (s4 ^ load_ne(d + 16)) | (s5 ^ load_ne(d + 20)) | (s6 ^ load_ne(d + 24))
                        | (s7 ^ load_ne(d + 28)))
                    == 0)
                    continue;
                store_ne(d, s0); store_ne(d + 4, s1); store_ne(d + 8, s2); store_ne(d + 12, s3);
                store_ne(d + 16, s4); store_ne(d + 20, s5); store_ne(d + 24, s6); store_ne(d + 28, s7);
#ifdef __m68k__
                /* Native order is big-endian: the loaded values are the transpose inputs. */
                convert32(s0, s1, s2, s3, s4, s5, s6, s7, chip, chipRow + gx / 8);
#else
                convert_from(d, chip, chipRow + gx / 8);
#endif
            }
            else
            {
                /* Partly covered group: merge the covered pixels into the shadow, which holds the rest. */
                if (memcmp(sh + a, in + (a - x), (size_t)(b - a)) == 0)
                    continue;
                memcpy(sh + a, in + (a - x), (size_t)(b - a));
                convert_from(sh + gx, chip, chipRow + gx / 8);
            }
            ++changed;
        }
    }
    return changed;
}
