/* SPDX-License-Identifier: GPL-3.0-only
 * Shared entity spatial bucket calculation, OpenRCT2 developers 2014-2026. */
#pragma once
#include "EntityRegistry.h"
#include "../world/Location.hpp"
#include <cstdlib>
namespace OpenRCT2
{
    constexpr uint32_t ComputeSpatialIndex(const CoordsXY& loc)
    {
        if (loc.isNull())
            return kSpatialIndexNullBucket;

        // NOTE: The input coordinate is rotated and can have negative components.
        const auto tileX = std::abs(loc.x) / kCoordsXYStep;
        const auto tileY = std::abs(loc.y) / kCoordsXYStep;

        if (tileX >= kMaximumMapSizeTechnical || tileY >= kMaximumMapSizeTechnical)
            return kSpatialIndexNullBucket;

        return tileX * kMaximumMapSizeTechnical + tileY;
    }
}
