/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AMIGA_PARK_MAP_H
#define AMIGA_PARK_MAP_H
#include <stdint.h>
#define PARK_MAP_HEADER 32u
#define PARK_MAP_RECORD 8u
#define PARK_MAP_RIDE_RECORD 8u
#define PARK_MAP_MAX_RIDES 255u
#define PARK_MAP_GUEST_RECORD 16u
#define PARK_MAP_MAX_GUESTS 10000u
#define PARK_MAP_MAX_ELEMENTS 196608u
typedef struct {
    const uint8_t *offsets, *records, *guest_records, *ride_records;
    uint32_t elements, fingerprint, guest_count, ride_count, track_count;
    unsigned side;
} ParkMap;
int park_map_open(ParkMap *, const void *, uint32_t);
unsigned park_map_u16(const uint8_t *);
unsigned park_map_guest_key(const uint8_t *);
uint32_t park_map_offset(const ParkMap *, unsigned);
#endif
