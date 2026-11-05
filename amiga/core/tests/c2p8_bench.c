/* SPDX-License-Identifier: GPL-3.0-only */
#include "../../../src/openrct2-ui/platform/amiga/amiga_c2p8.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __amigaos__
    #include <exec/memory.h>
    #include <proto/exec.h>
#endif

/* Times the AGA presenter on a PAL 640x512 frame in two cases:
 *   all:    every pixel changes each frame (worst case, e.g. a full scroll);
 *   sparse: a full-screen dirty rectangle in which only a 64x64 area changed
 *           (the toolbar and status bar are dirty together most frames).
 * On AmigaOS the planes are in Chip RAM, so this measures the real bus cost
 * of the machine it runs on; elsewhere it only compares the converter. */
#define WIDTH 640
#define HEIGHT 512
#define PITCH (WIDTH / 8)
#define FRAMES 24

static uint8_t* alloc_planes(void)
{
#ifdef __amigaos__
    return (uint8_t*)AllocMem(8 * PITCH * HEIGHT, MEMF_CHIP | MEMF_CLEAR);
#else
    return (uint8_t*)calloc(8 * PITCH * HEIGHT, 1);
#endif
}

static void free_planes(uint8_t* planes)
{
#ifdef __amigaos__
    FreeMem(planes, 8 * PITCH * HEIGHT);
#else
    free(planes);
#endif
}

static unsigned long ms_since(clock_t start)
{
    return (unsigned long)((clock() - start) * 1000 / CLOCKS_PER_SEC);
}

int main(void)
{
    const unsigned pixels = WIDTH * HEIGHT;
    uint8_t* src = malloc(pixels);
    uint8_t* shadow = calloc(pixels, 1);
    uint8_t* planes = alloc_planes();
    uint8_t* chip[8];
    unsigned i, round, groups = 0;
    uint32_t hash = 2166136261u;
    clock_t start;
    if (!src || !shadow || !planes) return 10;
    for (i = 0; i < 8; ++i) chip[i] = planes + i * PITCH * HEIGHT;
    for (i = 0; i < pixels; ++i) src[i] = (uint8_t)(i * 13 + i / 16);

    start = clock();
    for (round = 0; round < FRAMES; ++round)
    {
        for (i = 0; i < pixels; i += 32) src[i] ^= (uint8_t)(round + 1); /* touch every group */
        groups += amiga_c2p8_update(src, WIDTH, shadow, WIDTH, chip, PITCH, 0, 0, WIDTH, HEIGHT);
    }
    printf("C2P8 all changed:  %u frames, %lu ms, %u groups written\n", FRAMES, ms_since(start), groups);

    groups = 0;
    start = clock();
    for (round = 0; round < FRAMES; ++round)
    {
        unsigned y;
        for (y = 224; y < 288; ++y) src[y * WIDTH + 288 + round] ^= 0x33;
        groups += amiga_c2p8_update(src, WIDTH, shadow, WIDTH, chip, PITCH, 0, 0, WIDTH, HEIGHT);
    }
    printf("C2P8 sparse:       %u frames, %lu ms, %u groups written\n", FRAMES, ms_since(start), groups);

    for (i = 0; i < 8 * PITCH * HEIGHT; ++i) hash = (hash ^ planes[i]) * 16777619u;
    printf("C2P8 plane hash %08lx\n", (unsigned long)hash);
    free_planes(planes);
    free(shadow);
    free(src);
    return 0;
}
