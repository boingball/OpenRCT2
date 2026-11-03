/* SPDX-License-Identifier: GPL-3.0-only */
#include "diagnostic_output.h"
#include "s6_partial_import.h"
#include <openrct2/world/tile_element/PathElement.h>
#include <cstring>
#include <algorithm>
#include <openrct2/entity/Guest.h>
#include <openrct2/ride/Ride.h>
#include <openrct2/world/tile_element/TrackElement.h>
#include <openrct2/rct2/RCT2.h>
#include <openrct2/rct2/S6ElementImport.h>
#include <openrct2/ride/ted/TrackElemType.h>
#include <stdexcept>

#ifdef __amigaos__
extern "C" {
    unsigned long __stack = 262144;
    void __stkinit(void);
    void (*keep_stack_init)(void) = __stkinit;
}
#endif

static std::string integer(int64_t value)
{
    const bool negative = value < 0;
    uint64_t magnitude = negative ? 0ULL - static_cast<uint64_t>(value) : static_cast<uint64_t>(value);
    char digits[32];
    unsigned count = 0;
    do { digits[count++] = '0' + magnitude % 10; magnitude /= 10; } while (magnitude);
    std::string result = negative ? "-" : "";
    while (count) result.push_back(digits[--count]);
    return result;
}

static void testPathUnion()
{
    // AdditionStatus and RideId alias in the engine's packed PathElement.
    // A byte store to the first native byte of the ID is wrong on m68k.
    OpenRCT2::TileElement element;
    element.clearAs(OpenRCT2::TileElementType::path);
    auto* path = element.asPath();
    for (uint16_t ride : { uint16_t(0x0001), uint16_t(0x00fe), uint16_t(0xffff), uint16_t(0x1234) })
    {
        path->setRideIndex(RideId::FromUnderlying(ride));
        for (uint8_t status : { uint8_t(0), uint8_t(17), uint8_t(255) })
        {
            path->setAdditionStatus(status);
            if (path->getAdditionStatus() != status
                || path->getRideIndex().ToUnderlying() != ((ride & 0xff00) | status))
                throw std::runtime_error("Path status/RideId byte-order test failed");
        }
    }
    print("Native path status/RideId union: PASS\n");
}

static void testNativeEntityPool()
{
    using namespace OpenRCT2;
    auto registry=std::make_unique<EntityRegistry>();
    registry->initialiseEmptyStorage();
    if (registry->getNumFreeEntities()!=kMaxEntities || registry->tryGetEntity(EntityId::FromUnderlying(kMaxEntities)))
        throw std::runtime_error("Entity pool initial bounds failed");
    for (uint16_t i=0;i<kMaxEntities;i++)
    {
        auto* entity=registry->createEntityAt(EntityId::FromUnderlying(i),EntityType::guest);
        if (!entity || entity->id.ToUnderlying()!=i || entity->type!=EntityType::guest)
            throw std::runtime_error("Entity pool allocation failed");
    }
    if (registry->getNumFreeEntities()!=0 || registry->getEntityListCount(EntityType::guest)!=kMaxEntities
        || registry->createEntityAt(EntityId::FromUnderlying(kMaxEntities-1),EntityType::guest)
        || registry->createEntityAt(EntityId::FromUnderlying(kMaxEntities),EntityType::guest))
        throw std::runtime_error("Entity pool duplicate/capacity checks failed");
    bool rejected=false;
    try { registry->initialiseEmptyStorage(); } catch (const std::exception&) { rejected=true; }
    if (!rejected || registry->getEntityTileList({kLocationNull,0}).size()!=kMaxEntities)
        throw std::runtime_error("Entity pool reinitialisation/null bucket failed");
    registry->resetEntitySpatialIndices();
    if (registry->getEntityTileList({kLocationNull,0}).size()!=kMaxEntities)
        throw std::runtime_error("Entity null bucket rebuild failed");
    print("Actual entity pool: all slots / duplicate / bounds / reinitialisation / null bucket: PASS\n");
}

static uint32_t checkNativeGuests(PartialS6State& imported)
{
    using namespace OpenRCT2;
    auto& registry=imported.state->entities;
    const auto count=imported.savedGuests.size();
    if (registry.getEntityListCount(EntityType::guest)!=count || registry.getNumFreeEntities()+count!=kMaxEntities)
        throw std::runtime_error("Imported guest entity/free counts differ");
    uint32_t hash=2166136261u;
    auto append=[&](uint64_t value,unsigned width){while(width--){hash=(hash^(value&255))*16777619u;value>>=8;}};
    uint16_t previous=0;bool first=true;
    for (auto id:registry.getEntityList(EntityType::guest))
    {
        auto* base=registry.getEntity(id);
        auto* guest=reinterpret_cast<Guest*>(base);
        if (!base || base->type!=EntityType::guest || (!first && id.ToUnderlying()<=previous))
            throw std::runtime_error("Guest type/ID/list order failed");
        previous=id.ToUnderlying();first=false;
        const auto& bucket=registry.getEntityTileList({guest->x,guest->y});
        const uint32_t spatial=(guest->x/32)*kMaximumMapSizeTechnical+guest->y/32;
        if (guest->spatialIndex!=spatial || std::count(bucket.begin(),bucket.end(),id)!=1)
            throw std::runtime_error("Guest spatial index membership failed");
        append(id.ToUnderlying(),2);append(guest->x,4);append(guest->y,4);append(guest->z,4);
        append(guest->orientation,1);append(static_cast<uint8_t>(guest->state),1);append(guest->outsideOfPark,1);
        append(static_cast<uint8_t>(guest->tShirtColour),1);append(static_cast<uint8_t>(guest->trousersColour),1);
        append(static_cast<uint8_t>(guest->animationGroup),1);append(guest->peepId,4);
        append(guest->energy,1);append(guest->happiness,1);append(guest->hunger,1);append(guest->thirst,1);append(guest->toilet,1);
        append(guest->cashInPocket,8);append(guest->cashSpent,8);append(guest->spatialIndex,4);
        if (registry.createEntityAt(id,EntityType::guest))throw std::runtime_error("Duplicate imported ID accepted");
    }
    if (!registry.getEntityTileList({kLocationNull,0}).empty())
        throw std::runtime_error("Placed guest left in null spatial bucket");
    print("Actual Guest structure bytes: %lu\n",static_cast<unsigned long>(sizeof(Guest)));
    print("Actual native guests: %lu; free slots: %u; sorted list / spatial membership: PASS\n",
        static_cast<unsigned long>(count),unsigned(registry.getNumFreeEntities()));
    return hash;
}

static void testNativeTrackFields()
{
    using namespace OpenRCT2;
    uint8_t raw[9]={0};raw[5]=101;raw[6]=0x34;raw[7]=0x12;
    const auto* legacy=reinterpret_cast<const RCT12TrackElement*>(raw+1);
    if(legacy->GetMazeEntry()!=0x1234)throw std::runtime_error("Unaligned legacy maze byte order failed");
    TileElement tile;tile.clearAs(TileElementType::track);
    RCT2::ImportS6TrackData(*tile.asTrack(),*legacy,TrackElemType::maze,RIDE_TYPE_MAZE,true);
    if(tile.asTrack()->getMazeEntry()!=0x1234)throw std::runtime_error("Native maze union transfer failed");
    tile.clearAs(TileElementType::track);
    raw[1]=0x8a;raw[2]=0x60;raw[3]=6;raw[4]=8;raw[5]=2;raw[6]=0xa3;raw[7]=0x0e;raw[8]=2;
    RCT2::ImportS6TileBase(tile,*reinterpret_cast<const RCT12TileElement*>(raw+1),false);
    RCT2::ImportS6TrackData(*tile.asTrack(),*legacy,TrackElemType::beginStation,4,false);
    const auto* track=tile.asTrack();
    if(track->getRideIndex().ToUnderlying()!=2 || track->getSequenceIndex()!=3
        ||track->getStationIndex().ToUnderlying()!=2 || !track->hasChain() || !track->hasCableLift()
        ||!track->isInverted() ||!track->isBrakeClosed() ||!track->isIndestructible() ||!track->hasGreenLight())
        throw std::runtime_error("Native station/flags transfer failed");
    print("Track field conversion / station flags / unaligned maze union: PASS\n");
}

static uint32_t checkNativeRidesAndTracks(PartialS6State& imported)
{
    using namespace OpenRCT2;auto& state=*imported.state;
    uint32_t hash=2166136261u,rides=0,tracks=0;
    auto append=[&](uint64_t value,unsigned width){while(width--){hash=(hash^(value&255))*16777619u;value>>=8;}};
    auto coord=[&](const auto& position,bool world){
        append(position.isNull()?65535:position.x/(world?32:1),2);
        append(position.isNull()?65535:position.y/(world?32:1),2);
    };
    for(auto& ride:state.rides)if(!ride.id.IsNull())
    {
        ++rides;append(ride.id.ToUnderlying(),2);append(ride.type,2);
        append(static_cast<uint8_t>(ride.status),1);append(static_cast<uint8_t>(ride.mode),1);append(ride.numStations,1);
        coord(ride.overallView,true);
        if(ride.subtype!=kObjectEntryIndexNull)throw std::runtime_error("Unloaded ride object slot was treated as native object index");
        for(auto vehicle:ride.vehicles)if(!vehicle.IsNull())throw std::runtime_error("Unloaded vehicle reference survived");
        for(unsigned i=0;i<4;i++)
        {
            const auto& station=ride.getStation(StationIndex::FromUnderlying(i));
            coord(station.start,true);append(station.height,1);append(station.length,1);append(station.depart,1);
            append(station.queueTime,1);append(station.queueLength,2);append(station.segmentLength,4);append(station.segmentTime,2);
            coord(station.entrance,false);coord(station.exit,false);append(station.lastPeepInQueue.ToUnderlying(),2);
            if(!station.lastPeepInQueue.IsNull())
            {
                const auto* guest=state.entities.tryGetEntity(station.lastPeepInQueue);
                if(!guest || guest->type!=EntityType::guest)throw std::runtime_error("Station guest reference is unresolved");
            }
        }
    }
    for(const auto& tile:state.tileElements)if(const auto* track=tile.asTrack())
    {
        ++tracks;const auto id=track->getRideIndex();
        if(id.IsNull() || id.ToUnderlying()>=state.rides.size() || state.rides[id.ToUnderlying()].id!=id)
            throw std::runtime_error("Native track ride reference invalid");
        if(trackTypeIsStation(track->getTrackType()) && track->getStationIndex().ToUnderlying()>=state.rides[id.ToUnderlying()].numStations)
            throw std::runtime_error("Native track station reference invalid");
    }
    if(rides!=imported.nativeRides || tracks!=imported.nativeTracks)throw std::runtime_error("Native ride/track counts differ");
    for(auto id:state.entities.getEntityList(EntityType::guest))
    {
        const auto* guest=reinterpret_cast<const Guest*>(state.entities.getEntity(id));
        if(!guest->currentRide.IsNull() && (state.rides[guest->currentRide.ToUnderlying()].id!=guest->currentRide
            ||guest->currentRideStation.ToUnderlying()>=state.rides[guest->currentRide.ToUnderlying()].numStations))
            throw std::runtime_error("Native guest ride reference invalid");
        append(id.ToUnderlying(),2);append(guest->currentRide.ToUnderlying(),2);append(guest->currentRideStation.ToUnderlying(),1);
    }
    print("Actual native rides: %lu; tracks: %lu; ID/station/reference checks: PASS\n",static_cast<unsigned long>(rides),static_cast<unsigned long>(tracks));
    print("Native tracks skipped: aliases %lu; missing rides %lu; invalid stations %lu\n",
        static_cast<unsigned long>(imported.ambiguousTracks),static_cast<unsigned long>(imported.unresolvedTracks),static_cast<unsigned long>(imported.invalidTrackStations));
    print("Guest ride links: %lu; unresolved %lu; station guest links: %lu; unresolved %lu\n",
        static_cast<unsigned long>(imported.linkedGuestRides),static_cast<unsigned long>(imported.unresolvedGuestRides),
        static_cast<unsigned long>(imported.linkedQueueGuests),static_cast<unsigned long>(imported.unresolvedQueueGuests));
    print("Queue path ride/station links: %lu; unresolved cleared: %lu\n",static_cast<unsigned long>(imported.linkedQueuePaths),static_cast<unsigned long>(imported.unresolvedQueuePaths));
    print("Invalid station coordinates cleared: %lu\n",static_cast<unsigned long>(imported.invalidStationCoords));
    return hash;
}

int main(int argc, char** argv)
{
    // Fixed output names never share the required .sv6 input extension.
    if (argc != 2)
    {
        print("Usage: OpenRCT2ImportProbe TestPark.sv6\n");
        return 20;
    }
    const size_t length = std::strlen(argv[1]);
    if (length < 4 || argv[1][length - 4] != '.'
        || (argv[1][length - 3] != 's' && argv[1][length - 3] != 'S')
        || (argv[1][length - 2] != 'v' && argv[1][length - 2] != 'V') || argv[1][length - 1] != '6')
    {
        print("Input must have a .sv6 extension. It is opened read-only.\n");
        return 20;
    }
    report = std::fopen("OpenRCT2ImportProbe.txt", "w");
    print("OpenRCT2 partial native GameState import 0.13\n");
    print("Native terrain/paths, partial rides/tracks/guests; no simulation.\n");
    int result = 0;
    try
    {
        testPathUnion();
        testNativeTrackFields();
        testNativeEntityPool();
        print("Reading and importing SV6...\n");
        auto imported = ImportS6TerrainAndMetadata(argv[1]);
        const auto rideFingerprint=checkNativeRidesAndTracks(imported);
        print("Native ride/station fields fingerprint: %08lx\n",static_cast<unsigned long>(rideFingerprint));
        const auto nativeGuestFingerprint=checkNativeGuests(imported);
        print("Native guest fields fingerprint: %08lx\n",static_cast<unsigned long>(nativeGuestFingerprint));
        const auto& state = *imported.state;
        print("Constructed actual GameState: %lu bytes\n", static_cast<unsigned long>(sizeof(state)));
        print("Map: %u x %u; surfaces: %lu; paths: %lu\n",
            static_cast<unsigned>(state.mapSize.x), static_cast<unsigned>(state.mapSize.y),
            static_cast<unsigned long>(imported.surfaces), static_cast<unsigned long>(imported.paths));
        print("Saved guest snapshots: %lu; skipped unplaced/invalid: %lu\n",
            static_cast<unsigned long>(imported.savedGuests.size()), static_cast<unsigned long>(imported.skippedGuestSnapshots));
        print("Track footprints: %lu; skipped unresolved rides: %lu\n",
            static_cast<unsigned long>(imported.savedTracks.size()), static_cast<unsigned long>(imported.skippedTrackSnapshots));
        print("Ride snapshots: %lu; skipped invalid: %lu\n",
            static_cast<unsigned long>(imported.savedRides.size()), static_cast<unsigned long>(imported.skippedRideSnapshots));
        for (const auto& ride : imported.savedRides)
            print("Ride %u: legacy type %u; object slot %u; status %u; mode %u; stations %u\n",
                unsigned(ride[0]), unsigned(ride[1]), unsigned(ride[2]), unsigned(ride[3]), unsigned(ride[4]), unsigned(ride[7]));
        print("Other active map elements omitted: %lu\n", static_cast<unsigned long>(imported.omittedElements));
        print("Ticks: %lu; elapsed months: %lu; month ticks: %u\n",
            static_cast<unsigned long>(state.currentTicks), static_cast<unsigned long>(state.date.monthsElapsed),
            static_cast<unsigned>(state.date.monthTicks));
        print("RNG seeds: %08lx %08lx\n", static_cast<unsigned long>(state.scenarioRand.state().s0),
            static_cast<unsigned long>(state.scenarioRand.state().s1));
        print("Initial cash (tenths): %s; fee: %s; flags: %08lx\n", integer(state.scenarioOptions.initialCash).c_str(),
            integer(state.park.entranceFee).c_str(), static_cast<unsigned long>(state.park.flags.holder));
        print("Saved guest count: %lu; park rating: %u\n", static_cast<unsigned long>(state.park.numGuestsInPark),
            static_cast<unsigned>(state.park.rating));
        print("Cash (tenths): %s; loan (tenths): %s\n", integer(state.park.cash).c_str(), integer(state.park.bankLoan).c_str());
        print("Scenario label (legacy bytes): %s\n", imported.scenarioLabel.c_str());
        print("Native tile pointer index: PASS\n");
        print("Partial native state fingerprint: %08lx\n", static_cast<unsigned long>(PartialS6Fingerprint(imported)));
        WritePartialS6Overview(imported, "ImportedMap.ppm");
        WritePartialS6Map(imported, "ImportedMap.amap");
        print("RTG viewer map: ImportedMap.amap\n");
        print("Terrain/path overview: ImportedMap.ppm\n");
        print("Partial GameState import: PASS\n");
        print("Ride/guest import is partial; objects, vehicles and staff are absent. No simulation.\n");
    }
    catch (const std::exception& error)
    {
        print("FAIL: %s\n", error.what());
        result = 20;
    }
    if (report && std::fclose(report) != 0) result = 20;
    return result;
}
