/* SPDX-License-Identifier: GPL-3.0-only */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <openrct2/MemoryProfile.h>
#include <openrct2/ParkImporter.h>
#include <openrct2/PlatformEnvironment.h>
#include <openrct2/core/MemoryStream.h>
#include <openrct2/object/ObjectRepository.h>
#include <openrct2/rct2/RCT2.h>
#include <openrct2/sawyer_coding/SawyerChunkWriter.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

using namespace OpenRCT2;
using namespace OpenRCT2::SawyerCoding;

// Build genuine wire-order Sawyer streams without depending on game assets.
// Exercise the real importer, including its SC6 split tail and SV6 whole tail.
// GCC supports offsets in this packed legacy file-layout struct.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
static MemoryStream makeStream(bool scenario, uint16_t mapSize)
{
    MemoryStream stream;
    SawyerChunkWriter writer(&stream);
    const auto chunk = [&](size_t length, uint8_t fill = 0, size_t mapOffset = SIZE_MAX) {
        std::vector<uint8_t> bytes(length, fill);
        if (mapOffset != SIZE_MAX)
        {
            assert(mapOffset + 2 <= length);
            bytes[mapOffset] = static_cast<uint8_t>(mapSize);
            bytes[mapOffset + 1] = static_cast<uint8_t>(mapSize >> 8);
        }
        writer.WriteChunk(bytes.data(), bytes.size(), ChunkEncoding::rle);
    };
    uint8_t header[sizeof(RCT2::S6Header)]{};
    header[0] = scenario ? S6_TYPE_SCENARIO : S6_TYPE_SAVEDGAME;
    writer.WriteChunk(header, sizeof(header), ChunkEncoding::rle);
    if (scenario)
        chunk(sizeof(RCT2::S6Info));
    chunk(sizeof(RCT2::S6Data::Objects), 0xFF);
    chunk(16);
    chunk(sizeof(RCT2::S6Data::TileElements));
    constexpr size_t entities = RCT2::Limits::kMaxEntities * sizeof(RCT2::Entity);
    if (scenario)
    {
        chunk(76 + entities);
        chunk(4);
        chunk(8);
        chunk(2);
        chunk(1082);
        chunk(16);
        chunk(4);
        chunk(483816, 0, offsetof(RCT2::S6Data, MapSize) - offsetof(RCT2::S6Data, CompletedCompanyValue));
    }
    else
    {
        constexpr size_t unusedEntities =
            (RCT2::Limits::kMaxEntitiesRCTCExtended - RCT2::Limits::kMaxEntities) * sizeof(RCT2::Entity);
        chunk(488816 + entities, 0,
            offsetof(RCT2::S6Data, MapSize) - offsetof(RCT2::S6Data, NextFreeTileElementPointerIndex) - unusedEntities);
    }
    stream.SetPosition(0);
    return stream;
}

#pragma GCC diagnostic pop

int main(int argc, char** argv)
{
    static_assert(MemoryProfile::kSmall);
    auto env = CreatePlatformEnvironment(DirBaseValues{});
    auto repository = CreateObjectRepository(*env);
    if (argc == 2)
    {
        auto importer = ParkImporter::CreateS6(*repository);
        try
        {
            importer->Load(argv[1], false);
            std::puts("Native S6 decoding/map-profile/object-list: PASS (no import or simulation)");
            return 0;
        }
        catch (const std::exception& error)
        {
            std::fprintf(stderr, "S6 load failed: %s\n", error.what());
            return 10;
        }
    }
    if (argc != 1)
        return 2;
    for (bool scenario : { false, true })
    {
        for (uint16_t size : { 0, 4, 5, 150, 256, 257 })
        {
            std::printf("Testing %s %u tiles\n", scenario ? "SC6" : "SV6", size);
            auto stream = makeStream(scenario, size);
            auto importer = ParkImporter::CreateS6(*repository);
            bool accepted = false;
            try
            {
                importer->LoadFromStream(&stream, scenario, false);
                accepted = true;
            }
            catch (const std::runtime_error& error)
            {
                if (std::strcmp(error.what(), "Saved park exceeds the small-memory build's map limit.") != 0)
                    throw;
            }
            assert(accepted == MemoryProfile::mapFits(size, size));
            std::printf("%s %u tiles: %s PASS\n", scenario ? "SC6" : "SV6", size, accepted ? "accepted" : "rejected");
        }
    }
}
