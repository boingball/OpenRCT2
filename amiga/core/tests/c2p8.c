/* SPDX-License-Identifier: GPL-3.0-only */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../../src/openrct2-ui/platform/amiga/amiga_c2p8.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void verify(const uint8_t* chunky, uint8_t* const chip[8], int width, int height, int pitch)
{
    int x, y, plane;
    for (y = 0; y < height; ++y)
        for (x = 0; x < width; ++x)
        {
            unsigned value = 0;
            for (plane = 0; plane < 8; ++plane)
                if (chip[plane][y * pitch + x / 8] & (0x80 >> (x & 7))) value |= 1u << plane;
            assert(value == chunky[y * width + x]);
        }
}

static void exercise(int width, int height, int pitch, int iterations)
{
    const int planeSize = pitch * height, bytes = planeSize * 8;
    uint8_t* chunky = malloc((size_t)width * height);
    uint8_t* fast = calloc((size_t)bytes, 1);
    uint8_t* shadow = calloc((size_t)bytes, 1);
    uint8_t* chip[8];
    uint8_t* allocations[8];
    unsigned seed = 12345;
    int plane, i;
    assert(chunky && fast && shadow);
    for (plane = 0; plane < 8; ++plane)
    {
        allocations[plane] = malloc((size_t)planeSize + 32);
        assert(allocations[plane]);
        memset(allocations[plane], 0xA5, (size_t)planeSize + 32);
        chip[plane] = allocations[plane] + 16;
        memset(chip[plane], 0, (size_t)planeSize);
    }
    for (i = 0; i < width * height; ++i) chunky[i] = (uint8_t)(i * 37u + i / width * 19u);
    amiga_c2p8_rect(chunky, width, fast, pitch, planeSize, 0, 0, width, height);
    assert(amiga_c2p8_present(fast, shadow, chip, pitch, planeSize, 0, 0, width, height) > 0);
    verify(chunky, chip, width, height, pitch);
    assert(amiga_c2p8_present(fast, shadow, chip, pitch, planeSize, 0, 0, width, height) == 0);
    for (i = 0; i < iterations; ++i)
    {
        int x, y, w, h, xx, yy;
        uint8_t* rectangle;
        seed = seed * 1664525u + 1013904223u; x = (int)(seed % (unsigned)width);
        seed = seed * 1664525u + 1013904223u; y = (int)(seed % (unsigned)height);
        w = 1 + (int)(seed % (unsigned)(width - x));
        h = 1 + (int)((seed >> 8) % (unsigned)(height - y));
        rectangle = malloc((size_t)(w + 3) * h);
        assert(rectangle);
        memset(rectangle, 0xDD, (size_t)(w + 3) * h);
        for (yy = 0; yy < h; ++yy)
            for (xx = 0; xx < w; ++xx)
                rectangle[yy * (w + 3) + xx] = chunky[(y + yy) * width + x + xx] ^= (uint8_t)(i + 1);
        amiga_c2p8_rect(rectangle, w + 3, fast, pitch, planeSize, x, y, w, h);
        amiga_c2p8_present(fast, shadow, chip, pitch, planeSize, x, y, w, h);
        verify(chunky, chip, width, height, pitch);
        assert(amiga_c2p8_present(fast, shadow, chip, pitch, planeSize, x, y, w, h) == 0);
        free(rectangle);
    }
    for (plane = 0; plane < 8; ++plane)
    {
        int j;
        for (j = 0; j < 16; ++j)
            assert(allocations[plane][j] == 0xA5 && allocations[plane][16 + planeSize + j] == 0xA5);
        for (j = 0; j < height; ++j)
        {
            int b;
            for (b = width / 8; b < pitch; ++b) assert(chip[plane][j * pitch + b] == 0);
        }
        free(allocations[plane]);
    }
    free(chunky); free(fast); free(shadow);
}

int main(void)
{
    exercise(96, 17, 16, 128);
    exercise(640, 512, 80, 2);
    puts("AGA c2p8: 256 colours, PAL frame, partial rectangles, stride, guards and unchanged-word skip PASS");
    return 0;
}
