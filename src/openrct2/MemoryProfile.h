/*****************************************************************************
 * Copyright (c) 2026 OpenRCT2 developers
 * SPDX-License-Identifier: GPL-3.0-only
 *****************************************************************************/
#pragma once

#include <cstdint>

namespace OpenRCT2::MemoryProfile
{
#ifdef OPENRCT2_SMALL_MEMORY
    // Original RCT2 capacities. IDs and file formats retain their normal widths.
    constexpr bool kSmall = true;
    constexpr uint16_t kEntities = 10000;
    constexpr uint16_t kRides = 255;
    constexpr uint16_t kMapSize = 256;
    constexpr uint32_t kTileElements = 0x30000;
#else
    constexpr bool kSmall = false;
    constexpr uint16_t kEntities = 65535;
    constexpr uint16_t kRides = 1000;
    constexpr uint16_t kMapSize = 1001;
    constexpr uint32_t kTileElements = 0x1000000 - 512;
#endif

    constexpr bool mapFits(int32_t width, int32_t height)
    {
        return width >= 5 && height >= 5 && width <= kMapSize && height <= kMapSize;
    }
}
