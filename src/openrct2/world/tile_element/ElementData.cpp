/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "TileElement.h"
#include "SurfaceElement.h"
#include "PathElement.h"
#include "../MapLimits.h"
#include "../../core/EnumUtils.hpp"
#include "../../drawing/Colour.h"
#include <algorithm>

namespace OpenRCT2
{
    TileElementType TileElementBase::getType() const
    {
        return static_cast<TileElementType>((this->type & kTileElementTypeMask) >> 2);
    }

    void TileElementBase::setType(TileElementType newType)
    {
        this->type &= ~kTileElementTypeMask;
        this->type |= ((EnumValue(newType) << 2) & kTileElementTypeMask);
    }

    Direction TileElementBase::getDirection() const
    {
        return this->type & kTileElementDirectionMask;
    }

    void TileElementBase::setDirection(Direction direction)
    {
        this->type &= ~kTileElementDirectionMask;
        this->type |= (direction & kTileElementDirectionMask);
    }

    bool TileElementBase::isLastForTile() const
    {
        return (this->flags & TILE_ELEMENT_FLAG_LAST_TILE) != 0;
    }

    void TileElementBase::setLastForTile(bool on)
    {
        if (on)
            flags |= TILE_ELEMENT_FLAG_LAST_TILE;
        else
            flags &= ~TILE_ELEMENT_FLAG_LAST_TILE;
    }

    bool TileElementBase::isInvisible() const
    {
        return (this->flags & TILE_ELEMENT_FLAG_INVISIBLE) != 0;
    }

    void TileElementBase::setInvisible(bool on)
    {
        if (on)
            flags |= TILE_ELEMENT_FLAG_INVISIBLE;
        else
            flags &= ~TILE_ELEMENT_FLAG_INVISIBLE;
    }

    bool TileElementBase::isGhost() const
    {
        return (this->flags & TILE_ELEMENT_FLAG_GHOST) != 0;
    }

    void TileElementBase::setGhost(bool isGhost)
    {
        if (isGhost)
        {
            this->flags |= TILE_ELEMENT_FLAG_GHOST;
        }
        else
        {
            this->flags &= ~TILE_ELEMENT_FLAG_GHOST;
        }
    }

    uint8_t TileElementBase::getOccupiedQuadrants() const
    {
        return flags & kTileElementOccupiedQuadrantsMask;
    }

    void TileElementBase::setOccupiedQuadrants(uint8_t quadrants)
    {
        flags &= ~kTileElementOccupiedQuadrantsMask;
        flags |= (quadrants & kTileElementOccupiedQuadrantsMask);
    }

    int32_t TileElementBase::getBaseZ() const
    {
        return baseHeight * kCoordsZStep;
    }

    void TileElementBase::setBaseZ(int32_t newZ)
    {
        baseHeight = (newZ / kCoordsZStep);
    }

    int32_t TileElementBase::getClearanceZ() const
    {
        return clearanceHeight * kCoordsZStep;
    }

    void TileElementBase::setClearanceZ(int32_t newZ)
    {
        clearanceHeight = (newZ / kCoordsZStep);
    }

    const SurfaceElement* TileElementBase::asSurface() const
    {
        return as<SurfaceElement>();
    }

    SurfaceElement* TileElementBase::asSurface()
    {
        return as<SurfaceElement>();
    }

    const PathElement* TileElementBase::asPath() const
    {
        return as<PathElement>();
    }

    PathElement* TileElementBase::asPath()
    {
        return as<PathElement>();
    }

    void TileElement::clearAs(TileElementType newType)
    {
        type = 0;
        setType(newType);
        flags = 0;
        baseHeight = kMinimumLandHeight;
        clearanceHeight = kMinimumLandHeight;
        owner = 0;
        std::fill_n(pad05, sizeof(pad05), 0x00);
        std::fill_n(pad08, sizeof(pad08), 0x00);
    }

    ObjectEntryIndex SurfaceElement::getSurfaceObjectIndex() const
    {
        return surfaceStyle;
    }

    ObjectEntryIndex SurfaceElement::getEdgeObjectIndex() const
    {
        return edgeObjectIndex;
    }

    void SurfaceElement::setSurfaceObjectIndex(ObjectEntryIndex newStyle)
    {
        surfaceStyle = static_cast<ObjectEntryIndex>(newStyle);
    }

    void SurfaceElement::setEdgeObjectIndex(ObjectEntryIndex newIndex)
    {
        edgeObjectIndex = static_cast<ObjectEntryIndex>(newIndex);
    }

    int32_t SurfaceElement::getWaterHeight() const
    {
        return waterHeight * kWaterHeightStep;
    }

    void SurfaceElement::setWaterHeight(int32_t newWaterHeight)
    {
        waterHeight = newWaterHeight / kWaterHeightStep;
    }

    uint8_t SurfaceElement::getGrassLength() const
    {
        return grassLength;
    }

    void SurfaceElement::setGrassLength(uint8_t newLength)
    {
        grassLength = newLength;
    }

    OwnershipFlags SurfaceElement::getOwnership() const
    {
        OwnershipFlags ret;
        ret.holder = (ownership & kTileElementSurfaceOwnershipMask) >> 4;
        return ret;
    }

    void SurfaceElement::setOwnership(OwnershipFlags newOwnership)
    {
        ownership &= ~kTileElementSurfaceOwnershipMask;
        ownership |= (((newOwnership.holder) << 4) & kTileElementSurfaceOwnershipMask);
    }

    uint8_t SurfaceElement::getParkFences() const
    {
        return (ownership & kTileElementSurfaceParkFenceMask);
    }

    void SurfaceElement::setParkFences(uint8_t newParkFences)
    {
        ownership &= ~kTileElementSurfaceParkFenceMask;
        ownership |= (newParkFences & kTileElementSurfaceParkFenceMask);
    }

    uint8_t SurfaceElement::getSlope() const
    {
        return slope;
    }

    void SurfaceElement::setSlope(uint8_t newSlope)
    {
        slope = newSlope;
    }

    bool SurfaceElement::hasTrackThatNeedsWater() const
    {
        return (type & SURFACE_ELEMENT_HAS_TRACK_THAT_NEEDS_WATER) != 0;
    }

    void SurfaceElement::setHasTrackThatNeedsWater(bool on)
    {
        type &= ~SURFACE_ELEMENT_HAS_TRACK_THAT_NEEDS_WATER;
        if (on)
            type |= SURFACE_ELEMENT_HAS_TRACK_THAT_NEEDS_WATER;
    }

    Drawing::Colour SurfaceElement::getPrimarySurfaceColour() const
    {
        return surfaceColour1;
    }

    void SurfaceElement::setPrimarySurfaceColour(Drawing::Colour newColour)
    {
        newColour = (newColour == Drawing::Colour::invisible) ? Drawing::Colour::black : clampColour(newColour);
        surfaceColour1 = newColour;
    }

    Drawing::Colour SurfaceElement::getPrimaryEdgeColour() const
    {
        return edgeColour1;
    }

    void SurfaceElement::setPrimaryEdgeColour(Drawing::Colour newColour)
    {
        newColour = (newColour == Drawing::Colour::invisible) ? Drawing::Colour::black : clampColour(newColour);
        edgeColour1 = newColour;
    }

    bool PathElement::isSloped() const
    {
        return flags2.has(FootpathElementFlag::isSloped);
    }

    void PathElement::setSloped(bool isSloped)
    {
        flags2.set(FootpathElementFlag::isSloped, isSloped);
    }

    Direction PathElement::getSlopeDirection() const
    {
        return slopeDirection;
    }

    void PathElement::setSlopeDirection(Direction newSlope)
    {
        slopeDirection = newSlope;
    }

    bool PathElement::isQueue() const
    {
        return (type & FOOTPATH_ELEMENT_TYPE_FLAG_IS_QUEUE) != 0;
    }

    void PathElement::setIsQueue(bool isQueue)
    {
        type &= ~FOOTPATH_ELEMENT_TYPE_FLAG_IS_QUEUE;
        if (isQueue)
            type |= FOOTPATH_ELEMENT_TYPE_FLAG_IS_QUEUE;
    }

    bool PathElement::hasQueueBanner() const
    {
        return flags2.has(FootpathElementFlag::hasQueueBanner);
    }

    void PathElement::setHasQueueBanner(bool hasQueueBanner)
    {
        flags2.set(FootpathElementFlag::hasQueueBanner, hasQueueBanner);
    }

    bool PathElement::isBroken() const
    {
        return flags2.has(FootpathElementFlag::additionIsBroken);
    }

    void PathElement::setIsBroken(bool isBroken)
    {
        flags2.set(FootpathElementFlag::additionIsBroken, isBroken);
    }

    bool PathElement::isBlockedByVehicle() const
    {
        return flags2.has(FootpathElementFlag::blockedByVehicle);
    }

    void PathElement::setIsBlockedByVehicle(bool isBlocked)
    {
        flags2.set(FootpathElementFlag::blockedByVehicle, isBlocked);
    }

    ::StationIndex PathElement::getStationIndex() const
    {
        return stationIndex;
    }

    void PathElement::setStationIndex(::StationIndex newStationIndex)
    {
        stationIndex = newStationIndex;
    }

    bool PathElement::isWide() const
    {
        return (type & FOOTPATH_ELEMENT_TYPE_FLAG_IS_WIDE) != 0;
    }

    void PathElement::setWide(bool isWide)
    {
        type &= ~FOOTPATH_ELEMENT_TYPE_FLAG_IS_WIDE;
        if (isWide)
            type |= FOOTPATH_ELEMENT_TYPE_FLAG_IS_WIDE;
    }

    uint8_t PathElement::getAddition() const
    {
        return additions;
    }

    void PathElement::setAddition(uint8_t newAddition)
    {
        additions = newAddition;
    }

    bool PathElement::additionIsGhost() const
    {
        return flags2.has(FootpathElementFlag::additionIsGhost);
    }

    void PathElement::setAdditionIsGhost(bool isGhost)
    {
        flags2.set(FootpathElementFlag::additionIsGhost, isGhost);
    }

    ObjectEntryIndex PathElement::getLegacyPathEntryIndex() const
    {
        if (flags2.has(FootpathElementFlag::isLegacyPathEntry))
            return surfaceIndex;

        return kObjectEntryIndexNull;
    }

    void PathElement::setLegacyPathEntryIndex(ObjectEntryIndex newIndex)
    {
        surfaceIndex = newIndex;
        railingsIndex = kObjectEntryIndexNull;
        flags2.set(FootpathElementFlag::isLegacyPathEntry);
    }

    bool PathElement::hasLegacyPathEntry() const
    {
        return flags2.has(FootpathElementFlag::isLegacyPathEntry);
    }

    uint8_t PathElement::getQueueBannerDirection() const
    {
        return ((type & FOOTPATH_ELEMENT_TYPE_DIRECTION_MASK) >> 6);
    }

    void PathElement::setQueueBannerDirection(uint8_t direction)
    {
        type &= ~FOOTPATH_ELEMENT_TYPE_DIRECTION_MASK;
        type |= (direction << 6);
    }

    RideId PathElement::getRideIndex() const
    {
        return rideIndex;
    }

    void PathElement::setRideIndex(RideId newRideIndex)
    {
        rideIndex = newRideIndex;
    }

    uint8_t PathElement::getAdditionStatus() const
    {
        return static_cast<uint8_t>(rideIndex.ToUnderlying());
    }

    void PathElement::setAdditionStatus(uint8_t newStatus)
    {
        // The status aliases the low byte of RideId in the legacy layout.
        // Addressing the first native byte would corrupt the ID on big endian.
        rideIndex = RideId::FromUnderlying((rideIndex.ToUnderlying() & 0xff00) | newStatus);
    }

    uint8_t PathElement::getEdges() const
    {
        return edgesAndCorners & FOOTPATH_PROPERTIES_EDGES_EDGES_MASK;
    }

    void PathElement::setEdges(uint8_t newEdges)
    {
        edgesAndCorners &= ~FOOTPATH_PROPERTIES_EDGES_EDGES_MASK;
        edgesAndCorners |= (newEdges & FOOTPATH_PROPERTIES_EDGES_EDGES_MASK);
    }

    uint8_t PathElement::getCorners() const
    {
        return edgesAndCorners >> 4;
    }

    void PathElement::setCorners(uint8_t newCorners)
    {
        edgesAndCorners &= ~FOOTPATH_PROPERTIES_EDGES_CORNERS_MASK;
        edgesAndCorners |= (newCorners << 4);
    }
    ObjectEntryIndex PathElement::getSurfaceEntryIndex() const
    {
        if (flags2.has(FootpathElementFlag::isLegacyPathEntry))
            return kObjectEntryIndexNull;

        return surfaceIndex;
    }

    void PathElement::setSurfaceEntryIndex(ObjectEntryIndex newIndex)
    {
        surfaceIndex = newIndex;
        flags2.unset(FootpathElementFlag::isLegacyPathEntry);
    }

    ObjectEntryIndex PathElement::getRailingsEntryIndex() const
    {
        if (flags2.has(FootpathElementFlag::isLegacyPathEntry))
            return kObjectEntryIndexNull;

        return railingsIndex;
    }

    void PathElement::setRailingsEntryIndex(ObjectEntryIndex newIndex)
    {
        railingsIndex = newIndex;
        flags2.unset(FootpathElementFlag::isLegacyPathEntry);
    }
}
