/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
/* The mixer supplies signed 16-bit big-endian interleaved stereo. Paula
 * plays signed 8-bit planar stereo: retain each sample's high byte. */
static inline void amiga_paula_split(const unsigned char* pcm, unsigned char* left,
    unsigned char* right, unsigned frames)
{
    unsigned i;
    for (i = 0; i < frames; ++i)
    {
        left[i] = pcm[i * 4];
        right[i] = pcm[i * 4 + 2];
    }
}
