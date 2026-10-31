/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "RCT12.h"
#include "../world/tile_element/SurfaceElement.h"
#include "../world/tile_element/PathElement.h"
#include "../world/tile_element/Slope.h"

using namespace OpenRCT2;

RCT12TileElementType RCT12TileElementBase::getType() const
{
    auto elem_type = static_cast<RCT12TileElementType>((this->type & kTileElementTypeMask) >> 2);
    switch (elem_type)
    {
        case RCT12TileElementType::surface:
        case RCT12TileElementType::path:
        case RCT12TileElementType::track:
        case RCT12TileElementType::smallScenery:
        case RCT12TileElementType::entrance:
        case RCT12TileElementType::wall:
        case RCT12TileElementType::largeScenery:
        case RCT12TileElementType::banner:
        case RCT12TileElementType::corrupt:
        case RCT12TileElementType::eightCarsCorrupt14:
        case RCT12TileElementType::eightCarsCorrupt15:
            return elem_type;
        default:
            // Most corrupt elements were set to 0x255, but not all. Fallback to corrupt element if encountered unknown type.
            return RCT12TileElementType::eightCarsCorrupt15;
    }
}

uint8_t RCT12TileElementBase::getDirection() const
{
    return this->type & kTileElementDirectionMask;
}

uint8_t RCT12TileElementBase::getOccupiedQuadrants() const
{
    return flags & kTileElementOccupiedQuadrantsMask;
}

bool RCT12TileElementBase::isLastForTile() const
{
    return (this->flags & RCT12_TILE_ELEMENT_FLAG_LAST_TILE) != 0;
}

bool RCT12TileElementBase::isGhost() const
{
    return (this->flags & RCT12_TILE_ELEMENT_FLAG_GHOST) != 0;
}

uint8_t RCT12SurfaceElement::GetSlope() const
{
    return (Slope & kTileSlopeMask);
}

uint32_t RCT12SurfaceElement::GetSurfaceStyle() const
{
    uint32_t retVal = (Terrain >> 5) & 7;
    retVal |= (type & kRCT12SurfaceElementTypeSurfaceMask) << 3;
    return retVal;
}

uint32_t RCT12SurfaceElement::GetEdgeStyle() const
{
    uint32_t terrainEdge = (Slope >> 5) & 7;
    if (type & 128)
        terrainEdge |= (1 << 3);
    return terrainEdge;
}

uint8_t RCT12SurfaceElement::GetGrassLength() const
{
    return GrassLength;
}

OwnershipFlags RCT12SurfaceElement::GetOwnership() const
{
    OwnershipFlags ret;
    ret.holder = (Ownership & kTileElementSurfaceOwnershipMask) >> 4;
    return ret;
}

uint32_t RCT12SurfaceElement::GetWaterHeight() const
{
    return (Terrain & kRCT12SurfaceElementWaterHeightMask) * kWaterHeightStep;
}

uint8_t RCT12SurfaceElement::GetParkFences() const
{
    return (Ownership & kTileElementSurfaceParkFenceMask);
}

bool RCT12SurfaceElement::HasTrackThatNeedsWater() const
{
    return (type & SURFACE_ELEMENT_HAS_TRACK_THAT_NEEDS_WATER) != 0;
}

uint8_t RCT12PathElement::GetEntryIndex() const
{
    return (EntryIndex & RCT12_FOOTPATH_PROPERTIES_TYPE_MASK) >> 4;
}

uint8_t RCT12PathElement::GetQueueBannerDirection() const
{
    return ((type & FOOTPATH_ELEMENT_TYPE_DIRECTION_MASK) >> 6);
}

bool RCT12PathElement::IsSloped() const
{
    return (EntryIndex & RCT12_FOOTPATH_PROPERTIES_FLAG_IS_SLOPED) != 0;
}

uint8_t RCT12PathElement::GetSlopeDirection() const
{
    return EntryIndex & RCT12_FOOTPATH_PROPERTIES_SLOPE_DIRECTION_MASK;
}

uint8_t RCT12PathElement::GetRideIndex() const
{
    return RideIndex;
}

uint8_t RCT12PathElement::GetStationIndex() const
{
    return (Additions & RCT12_FOOTPATH_PROPERTIES_ADDITIONS_STATION_INDEX_MASK) >> 4;
}

bool RCT12PathElement::IsWide() const
{
    return (type & FOOTPATH_ELEMENT_TYPE_FLAG_IS_WIDE) != 0;
}

bool RCT12PathElement::IsQueue() const
{
    return (type & FOOTPATH_ELEMENT_TYPE_FLAG_IS_QUEUE) != 0;
}

bool RCT12PathElement::HasQueueBanner() const
{
    return (EntryIndex & RCT12_FOOTPATH_PROPERTIES_FLAG_HAS_QUEUE_BANNER) != 0;
}
uint8_t RCT12PathElement::GetEdges() const
{
    return Edges & FOOTPATH_PROPERTIES_EDGES_EDGES_MASK;
}

uint8_t RCT12PathElement::GetCorners() const
{
    return Edges >> 4;
}

uint8_t RCT12PathElement::GetAddition() const
{
    return Additions & RCT12_FOOTPATH_PROPERTIES_ADDITIONS_TYPE_MASK;
}

bool RCT12PathElement::AdditionIsGhost() const
{
    return (Additions & RCT12_FOOTPATH_PROPERTIES_ADDITIONS_FLAG_GHOST) != 0;
}

uint8_t RCT12PathElement::GetAdditionStatus() const
{
    return AdditionStatus;
}

uint8_t RCT12PathElement::GetRCT1PathType() const
{
    uint8_t pathColour = type & 3;
    uint8_t pathType2 = (EntryIndex & RCT12_FOOTPATH_PROPERTIES_TYPE_MASK) >> 2;

    pathType2 = pathType2 | pathColour;
    return pathType2;
}

uint8_t RCT12PathElement::GetRCT1SupportType() const
{
    return (flags & 0b01100000) >> 5;
}


bool RCT12PathElement::IsBroken() const
{
    return (flags & RCT12_TILE_ELEMENT_FLAG_BROKEN) != 0;
}

bool RCT12PathElement::IsBlockedByVehicle() const
{
    return (flags & RCT12_TILE_ELEMENT_FLAG_BLOCKED_BY_VEHICLE) != 0;
}

RideId RCT12RideIdToOpenRCT2RideId(const RCT12RideId rideId)
{
    if (rideId == kRCT12RideIdNull)
        return RideId::GetNull();

    return RideId::FromUnderlying(rideId);
}
