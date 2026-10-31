/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "S6ElementImport.h"
#include "RCT2.h"
#include "../world/tile_element/TileElement.h"
#include "../world/tile_element/SurfaceElement.h"
#include "../world/tile_element/PathElement.h"
#include <stdexcept>

namespace OpenRCT2::RCT2
{
    void ImportS6TileBase(TileElement& dst, const RCT12TileElement& src, bool invisible)
    {
        dst.clearAs(ToOpenRCT2TileElementType(src.getType()));
        dst.setDirection(src.getDirection());
        dst.setBaseZ(src.baseHeight * kCoordsZStep);
        dst.setClearanceZ(src.clearanceHeight * kCoordsZStep);

        // All saved in "flags"
        dst.setOccupiedQuadrants(src.getOccupiedQuadrants());
        dst.setGhost(src.isGhost());
        dst.setLastForTile(src.isLastForTile());
        dst.setInvisible(invisible);
    }

    void ImportS6SurfaceData(SurfaceElement& dst, const RCT12SurfaceElement& src,
        std::span<const ObjectEntryIndex> terrainEntries)
    {
        if (src.GetSurfaceStyle() >= terrainEntries.size()
            || src.GetSurfaceStyle() >= kTerrainSurfaceColours.size()
            || src.GetEdgeStyle() >= kTerrainEdgeColours.size())
            throw std::runtime_error("Unsupported legacy terrain style");

        dst.setSlope(src.GetSlope());

        dst.setSurfaceObjectIndex(terrainEntries[src.GetSurfaceStyle()]);
        dst.setEdgeObjectIndex(src.GetEdgeStyle());

        dst.setGrassLength(src.GetGrassLength());
        dst.setOwnership(src.GetOwnership());
        dst.setParkFences(src.GetParkFences());
        dst.setWaterHeight(src.GetWaterHeight());
        dst.setHasTrackThatNeedsWater(src.HasTrackThatNeedsWater());
        dst.setPrimarySurfaceColour(kTerrainSurfaceColours[src.GetSurfaceStyle()]);
        dst.setPrimaryEdgeColour(kTerrainEdgeColours[src.GetEdgeStyle()]);
    }

    void ImportS6PathData(PathElement& dst, const RCT12PathElement& src,
        std::span<const ObjectEntryIndex> surfaceEntries,
        std::span<const ObjectEntryIndex> queueEntries,
        std::span<const ObjectEntryIndex> railingEntries)
    {
        if (src.GetEntryIndex() >= surfaceEntries.size() || src.GetEntryIndex() >= queueEntries.size()
            || src.GetEntryIndex() >= railingEntries.size())
            throw std::runtime_error("Unsupported legacy path style");

        auto pathEntryIndex = src.GetEntryIndex();
        auto surfaceEntry = src.IsQueue() ? queueEntries[pathEntryIndex]
                                            : surfaceEntries[pathEntryIndex];
        if (surfaceEntry == kObjectEntryIndexNull)
        {
            // Legacy footpath object
            dst.setLegacyPathEntryIndex(pathEntryIndex);
        }
        else
        {
            // Surface / railing
            dst.setSurfaceEntryIndex(surfaceEntry);
            dst.setRailingsEntryIndex(railingEntries[pathEntryIndex]);
        }

        dst.setQueueBannerDirection(src.GetQueueBannerDirection());
        dst.setSloped(src.IsSloped());
        dst.setSlopeDirection(src.GetSlopeDirection());
        dst.setRideIndex(RCT12RideIdToOpenRCT2RideId(src.GetRideIndex()));
        dst.setStationIndex(StationIndex::FromUnderlying(src.GetStationIndex()));
        dst.setWide(src.IsWide());
        dst.setIsQueue(src.IsQueue());
        dst.setHasQueueBanner(src.HasQueueBanner());
        dst.setEdges(src.GetEdges());
        dst.setCorners(src.GetCorners());
        dst.setAddition(src.GetAddition());
        dst.setAdditionIsGhost(src.AdditionIsGhost());
        dst.setAdditionStatus(src.GetAdditionStatus());
        dst.setIsBroken(src.IsBroken());
        dst.setIsBlockedByVehicle(src.IsBlockedByVehicle());
    }
}
