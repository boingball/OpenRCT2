/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>

/* Presents a chunky rectangle on eight Chip bitplanes.
 *
 * src points at the rectangle's top-left pixel, not the whole framebuffer.
 * shadow is a Fast-RAM chunky copy of what the planes show, shadowPitch bytes
 * per row; it must start out equal to the planes (for example both cleared)
 * and is kept equal by this call. Only 32-pixel groups whose pixels differ
 * from the shadow are transposed and written; Chip RAM is never read.
 * The bitmap width is a multiple of 32, planarPitch a multiple of four and
 * shadowPitch at least the bitmap width; rectangles lie inside the bitmap.
 * Returns the number of 32-pixel groups written. */
unsigned amiga_c2p8_update(const uint8_t* src, int srcPitch, uint8_t* shadow, int shadowPitch,
    uint8_t* const chip[8], int planarPitch, int x, int y, int w, int h);
