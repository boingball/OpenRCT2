/* SPDX-License-Identifier: GPL-3.0-only
 * Data-only Ride station accessors, OpenRCT2 developers 2014-2026. */
#include "Ride.h"
namespace OpenRCT2
{
    RideStation& Ride::getStation(StationIndex stationIndex)
    {
        return stations[stationIndex.ToUnderlying()];
    }

    const RideStation& Ride::getStation(StationIndex stationIndex) const
    {
        return stations[stationIndex.ToUnderlying()];
    }

    std::span<RideStation> Ride::getStations()
    {
        return stations;
    }

    std::span<const RideStation> Ride::getStations() const
    {
        return stations;
    }
}
