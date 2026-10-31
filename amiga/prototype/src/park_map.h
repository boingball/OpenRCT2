/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AMIGA_PARK_MAP_H
#define AMIGA_PARK_MAP_H
#include <stdint.h>
#define PARK_MAP_HEADER 32u
#define PARK_MAP_RECORD 8u
#define PARK_MAP_MAX_ELEMENTS 196608u
typedef struct {
    const uint8_t *offsets, *records;
    uint32_t elements, fingerprint;
    unsigned side;
} ParkMap;
int park_map_open(ParkMap *, const void *, uint32_t);
uint32_t park_map_offset(const ParkMap *, unsigned);
#endif
