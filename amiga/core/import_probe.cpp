/* SPDX-License-Identifier: GPL-3.0-only */
#include "diagnostic_output.h"
#include "s6_partial_import.h"
#include <openrct2/world/tile_element/PathElement.h>
#include <cstring>
#include <algorithm>
#include <openrct2/entity/Guest.h>
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
    print("OpenRCT2 partial native GameState import 0.12\n");
    print("Terrain/paths, metadata, guest and ride/track snapshots; no simulation.\n");
    int result = 0;
    try
    {
        testPathUnion();
        testNativeEntityPool();
        print("Reading and importing SV6...\n");
        auto imported = ImportS6TerrainAndMetadata(argv[1]);
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
        print("Guest import is partial; objects, rides, vehicles and staff are absent. No simulation.\n");
    }
    catch (const std::exception& error)
    {
        print("FAIL: %s\n", error.what());
        result = 20;
    }
    if (report && std::fclose(report) != 0) result = 20;
    return result;
}
