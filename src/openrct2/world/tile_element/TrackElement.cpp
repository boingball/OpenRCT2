/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "TrackElement.h"

#include "../../ride/RideData.h"
#include "../../ride/Track.h"

namespace OpenRCT2
{

    /**
     * Checks if a track element is recognised as the beginning of a block.
     * A beginning of a block can be the end of a station, the end of a lift hill,
     * or a block brake.
     */
    bool TrackElement::isBlockStart() const
    {
        switch (getTrackType())
        {
            case TrackElemType::endStation:
            case TrackElemType::cableLiftHill:
            case TrackElemType::blockBrakes:
            case TrackElemType::diagBlockBrakes:
                return true;
            case TrackElemType::up25ToFlat:
            case TrackElemType::up60ToFlat:
            case TrackElemType::diagUp25ToFlat:
            case TrackElemType::diagUp60ToFlat:
                return hasChain();
            default:
                return false;
        }
    }

    bool TrackElement::isStation() const
    {
        return trackTypeIsStation(getTrackType());
    }

    uint8_t TrackElement::getSeatRotation() const
    {
        const auto* ride = GetRide(getRideIndex());
        if (ride != nullptr && ride->getRideTypeDescriptor().flags.has(RtdFlag::hasLandscapeDoors))
            return kDefaultSeatRotation;

        return uRide.colourScheme >> 4;
    }

} // namespace OpenRCT2
