/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "Colour.h"
#include "../core/EnumUtils.hpp"
#include <algorithm>

namespace OpenRCT2::Drawing
{
    Colour clampColour(uint8_t colour)
    {
        return static_cast<Colour>(std::clamp<uint8_t>(colour, 0, kColourNumTotal - 1));
    }

    Colour clampColour(Colour colour)
    {
        return clampColour(EnumValue(colour));
    }
}
