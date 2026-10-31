/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once
#include "../object/ObjectTypes.h"
#include <span>
struct RCT12TileElement;
struct RCT12SurfaceElement;
struct RCT12PathElement;
namespace OpenRCT2
{
    struct TileElement;
    struct SurfaceElement;
    struct PathElement;
}
namespace OpenRCT2::RCT2
{
    void ImportS6TileBase(TileElement&, const RCT12TileElement&, bool invisible);
    void ImportS6SurfaceData(SurfaceElement&, const RCT12SurfaceElement&, std::span<const ObjectEntryIndex>);
    void ImportS6PathData(PathElement&, const RCT12PathElement&, std::span<const ObjectEntryIndex>,
        std::span<const ObjectEntryIndex>, std::span<const ObjectEntryIndex>);
}
