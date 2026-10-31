/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>

/* src points at the rectangle's top-left pixel, not the whole framebuffer.
 * Fast planes/shadow are contiguous, planeSize bytes apart. Chip planes may
 * be separately allocated. Bitmap width is a multiple of 32, planarPitch
 * a multiple of four; rectangles must lie inside the allocated bitmap. */
void amiga_c2p8_rect(const uint8_t* src, int srcPitch, uint8_t* planes,
    int planarPitch, int planeSize, int x, int y, int w, int h);
unsigned amiga_c2p8_present(const uint8_t* planes, uint8_t* shadow, uint8_t* const chip[8],
    int planarPitch, int planeSize, int x, int y, int w, int h);
