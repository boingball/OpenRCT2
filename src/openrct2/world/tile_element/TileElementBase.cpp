/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "TileElementBase.h"

#include "../../core/EnumUtils.hpp"
#include "../Map.h"
#include "BannerElement.h"
#include "EntranceElement.h"
#include "LargeSceneryElement.h"
#include "PathElement.h"
#include "SmallSceneryElement.h"
#include "SurfaceElement.h"
#include "TileElement.h"
#include "TrackElement.h"
#include "WallElement.h"

namespace OpenRCT2
{

    Direction TileElementBase::getDirectionWithOffset(uint8_t offset) const
    {
        return DirectionRotate(getDirection(), offset);
    }

    void TileElementBase::remove()
    {
        TileElementRemove(static_cast<TileElement*>(this));
    }

    uint8_t TileElementBase::getOwner() const
    {
        return owner & kTileElementOwnerMask;
    }

    void TileElementBase::setOwner(uint8_t newOwner)
    {
        owner &= ~kTileElementOwnerMask;
        owner |= (newOwner & kTileElementOwnerMask);
    }

    const SmallSceneryElement* TileElementBase::asSmallScenery() const
    {
        return as<SmallSceneryElement>();
    }
    SmallSceneryElement* TileElementBase::asSmallScenery()
    {
        return as<SmallSceneryElement>();
    }
    const LargeSceneryElement* TileElementBase::asLargeScenery() const
    {
        return as<LargeSceneryElement>();
    }
    LargeSceneryElement* TileElementBase::asLargeScenery()
    {
        return as<LargeSceneryElement>();
    }
    const WallElement* TileElementBase::asWall() const
    {
        return as<WallElement>();
    }
    WallElement* TileElementBase::asWall()
    {
        return as<WallElement>();
    }

    const EntranceElement* TileElementBase::asEntrance() const
    {
        return as<EntranceElement>();
    }

    EntranceElement* TileElementBase::asEntrance()
    {
        return as<EntranceElement>();
    }

    const BannerElement* TileElementBase::asBanner() const
    {
        return as<BannerElement>();
    }
    BannerElement* TileElementBase::asBanner()
    {
        return as<BannerElement>();
    }
} // namespace OpenRCT2
