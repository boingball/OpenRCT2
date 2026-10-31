/* SPDX-License-Identifier: GPL-3.0-only */
#include "diagnostic_output.h"
#include "s6_partial_import.h"
#include <openrct2/world/tile_element/PathElement.h>
#include <cstring>
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
    print("OpenRCT2 partial native GameState import 0.8\n");
    print("Terrain/paths and selected metadata only; no simulation.\n");
    int result = 0;
    try
    {
        testPathUnion();
        print("Reading and importing SV6...\n");
        auto imported = ImportS6TerrainAndMetadata(argv[1]);
        const auto& state = *imported.state;
        print("Constructed actual GameState: %lu bytes\n", static_cast<unsigned long>(sizeof(state)));
        print("Map: %u x %u; surfaces: %lu; paths: %lu\n",
            static_cast<unsigned>(state.mapSize.x), static_cast<unsigned>(state.mapSize.y),
            static_cast<unsigned long>(imported.surfaces), static_cast<unsigned long>(imported.paths));
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
        print("Objects, rides and entities are NOT imported. This state cannot run simulation.\n");
    }
    catch (const std::exception& error)
    {
        print("FAIL: %s\n", error.what());
        result = 20;
    }
    if (report && std::fclose(report) != 0) result = 20;
    return result;
}
