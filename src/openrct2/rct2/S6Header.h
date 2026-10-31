/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once
#include <cstdint>

namespace OpenRCT2::RCT2
{
#pragma pack(push, 1)
    /**
     * SV6/SC6 header chunk
     * size: 0x20
     */
    struct S6Header
    {
        uint8_t Type;              // 0x00
        uint8_t ClassicFlag;       // 0x01
        uint16_t NumPackedObjects; // 0x02
        uint32_t Version;          // 0x04
        uint32_t MagicNumber;      // 0x08
        uint8_t Pad0C[0x14];
    };
    static_assert(sizeof(S6Header) == 0x20);

#pragma pack(pop)
}
