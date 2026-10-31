/* SPDX-License-Identifier: GPL-3.0-only */
#include "s6_partial_import.h"
#include <openrct2/core/Endianness.h>
#include <openrct2/core/MemoryStream.h>
#include <openrct2/core/Numerics.hpp>
#include <openrct2/rct2/RCT2.h>
#include <openrct2/rct2/S6ElementImport.h>
#include <openrct2/sawyer_coding/SawyerChunk.h>
#include <openrct2/sawyer_coding/SawyerChunkReader.h>
#include <openrct2/sawyer_coding/SawyerCoding.h>
#include <openrct2/world/TilePointerIndex.hpp>
#include <openrct2/world/tile_element/SurfaceElement.h>
#include <openrct2/world/tile_element/PathElement.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>

using namespace OpenRCT2;
using namespace OpenRCT2::SawyerCoding;

namespace
{
    void check(bool ok, const char* message)
    {
        if (!ok) throw std::runtime_error(message);
    }
    struct CloseFile { void operator()(FILE* p) const { std::fclose(p); } };
    using File = std::unique_ptr<FILE, CloseFile>;
    std::shared_ptr<SawyerChunk> readChunk(MemoryStream& stream)
    {
        const auto position = stream.GetPosition();
        const auto remaining = stream.GetLength() - position;
        check(remaining >= 5, "Truncated S6 chunk header");
        const auto* bytes = static_cast<const uint8_t*>(stream.GetData()) + position;
        const auto length = ReadLittleEndian<uint32_t>(bytes + 1);
        check(length > 0 && length < 16 * 1024 * 1024 && length <= remaining - 5,
            "Invalid S6 chunk length");
        return SawyerChunkReader(&stream).ReadChunk();
    }

    // Use the actual packed legacy definition for offsets, without allocating
    // its extended 15,000-entity array. Chunk 6 stores only 10,000 in these saves.
    template<typename T>
    T scalar(const SawyerChunk& chunk, size_t relative)
    {
        constexpr size_t postEntities = 4 + RCT2::Limits::kMaxEntities * sizeof(RCT2::Entity);
        const size_t offset = postEntities + relative;
        check(offset <= chunk.GetLength() && sizeof(T) <= chunk.GetLength() - offset, "Truncated S6 state field");
        return ReadLittleEndian<T>(static_cast<const uint8_t*>(chunk.GetData()) + offset);
    }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
PartialS6State ImportS6TerrainAndMetadata(const char* path)
{
    static_assert(MemoryProfile::kSmall, "Native import diagnostic must use the small-memory profile");
    File input(std::fopen(path, "rb"));
    check(input != nullptr, "Cannot open SV6 file");
    check(std::fseek(input.get(), 0, SEEK_END) == 0, "Cannot seek SV6 file");
    const long size = std::ftell(input.get());
    check(size >= 9 && size <= 16 * 1024 * 1024, "SV6 file size outside diagnostic limit");
    check(std::fseek(input.get(), 0, SEEK_SET) == 0, "Cannot rewind SV6 file");
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    check(std::fread(bytes.data(), 1, bytes.size(), input.get()) == bytes.size(), "Cannot read SV6 file");
    check(CalculateChecksum(bytes.data(), bytes.size() - 4)
        == ReadLittleEndian<uint32_t>(bytes.data() + bytes.size() - 4), "SV6 checksum mismatch");
    MemoryStream stream(bytes.data(), bytes.size() - 4, MemoryAccess::read);
    auto headerChunk = readChunk(stream);
    check(headerChunk->GetLength() == sizeof(RCT2::S6Header), "Wrong S6 header size");
    RCT2::S6Header header;
    std::memcpy(&header, headerChunk->GetData(), sizeof header);
    check(header.Type == 0, "Partial importer currently supports SV6 saved games only");
    check(header.ClassicFlag != 0x0f, "Extended Classic saves are unsupported");
    for (unsigned i = 0; i < ToLittleEndian(header.NumPackedObjects); ++i)
    {
        check(stream.GetLength() - stream.GetPosition() >= sizeof(RCTObjectEntry), "Truncated packed object entry");
        stream.Seek(sizeof(RCTObjectEntry), STREAM_SEEK_CURRENT);
        (void)readChunk(stream);
    }
    auto objects = readChunk(stream);
    check(objects->GetLength() == sizeof(RCT2::S6Data::Objects), "Wrong legacy object table size");
    auto date = readChunk(stream);
    check(date->GetLength() == 16, "Wrong S6 date chunk size");
    auto tiles = readChunk(stream);
    check(tiles->GetLength() == RCT2::Limits::kMaxTileElements * sizeof(RCT12TileElement), "Wrong S6 tile chunk size");
    auto savedState = readChunk(stream);
    check(savedState->GetLength() == 488816 + RCT2::Limits::kMaxEntities * sizeof(RCT2::Entity),
        "Wrong classic S6 state chunk size");
    check(stream.GetPosition() == stream.GetLength(), "Unexpected chunks after classic SV6 state");

    using Legacy = RCT2::S6Data;
    constexpr size_t start = offsetof(Legacy, EntityListsHead);
#define FIELD(type, name) scalar<type>(*savedState, offsetof(Legacy, name) - start)
    const auto mapSize = FIELD(uint16_t, MapSize);
    check(MemoryProfile::mapFits(mapSize, mapSize), "SV6 map exceeds the small-memory map limit");
    PartialS6State result;
    result.state = std::make_unique<GameState_t>();
    auto& state = *result.state;
    state.mapSize = { mapSize, mapSize };
    const auto* dateBytes = static_cast<const uint8_t*>(date->GetData());
    state.date = { ReadLittleEndian<uint16_t>(dateBytes), ReadLittleEndian<uint16_t>(dateBytes + 2) };
    Random::FixedSeedSequence<2> seeds(ReadLittleEndian<uint32_t>(dateBytes + 8), ReadLittleEndian<uint32_t>(dateBytes + 12));
    state.scenarioRand.seed(seeds);
    state.currentTicks = FIELD(uint32_t, GameTicks1);
    state.scenarioOptions.initialCash = ToMoney64(FIELD(money32, InitialCash));
    state.park.bankLoan = ToMoney64(FIELD(money32, CurrentLoan));
    state.park.flags.holder = FIELD(uint32_t, ParkFlags);
    state.park.entranceFee = FIELD(money16, ParkEntranceFee);
    state.park.numGuestsInPark = FIELD(uint16_t, GuestsInPark);
    state.park.rating = FIELD(uint16_t, ParkRating);
    state.park.cash = ToMoney64(static_cast<money32>(Numerics::rol32(FIELD(uint32_t, Cash) ^ 0xf4ec9621U, 13)));
    const size_t labelOffset = 4 + RCT2::Limits::kMaxEntities * sizeof(RCT2::Entity)
        + offsetof(Legacy, ScenarioName) - start;
    const auto* label = static_cast<const char*>(savedState->GetData()) + labelOffset;
    const auto labelLength = std::find(label, label + 64, '\0') - label;
    result.scenarioLabel.assign(label, static_cast<size_t>(labelLength));
#undef FIELD

    std::array<bool, RCT2::Limits::kMaxRidesInPark> ridePresent{};
    for (unsigned slot = 0; slot < RCT2::Limits::kMaxRidesInPark; ++slot)
    {
        const size_t relative = offsetof(Legacy, Rides) - start + slot * sizeof(RCT2::Ride);
        auto field = [&](size_t offset) { return scalar<uint8_t>(*savedState, relative + offset); };
        const auto type = field(offsetof(RCT2::Ride, type));
        if (type == 255) continue;
        const auto status = field(offsetof(RCT2::Ride, status));
        const auto stations = field(offsetof(RCT2::Ride, numStations));
        if (status > 3 || stations > 4) { ++result.skippedRideSnapshots; continue; }
        result.savedRides.push_back({ static_cast<uint8_t>(slot), type,
            field(offsetof(RCT2::Ride, subtype)), status, field(offsetof(RCT2::Ride, mode)),
            field(offsetof(RCT2::Ride, overallView)), field(offsetof(RCT2::Ride, overallView) + 1), stations });
        ridePresent[slot] = true;
    }
    std::array<ObjectEntryIndex, 32> terrain{};
    for (unsigned i = 0; i < terrain.size(); ++i) terrain[i] = i;
    std::array<ObjectEntryIndex, 16> legacyPaths;
    legacyPaths.fill(kObjectEntryIndexNull);
    const auto* tileBytes = static_cast<const uint8_t*>(tiles->GetData());
    size_t index = 0;
    state.tileElements.reserve(RCT2::Limits::kMaxTileElements);
    for (unsigned y = 0; y < RCT2::Limits::kMaxMapSize; ++y)
    {
        for (unsigned x = 0; x < RCT2::Limits::kMaxMapSize; ++x)
        {
            const size_t first = state.tileElements.size();
            bool surfaceAdded = false, nextInvisible = false, restInvisible = false;
            bool last;
            do
            {
                check(index < RCT2::Limits::kMaxTileElements, "Tile chain has no end marker");
                RCT12TileElement src;
                std::memcpy(&src, tileBytes + index++ * sizeof(src), sizeof(src));
                last = src.isLastForTile();
                if (x >= mapSize || y >= mapSize || src.baseHeight == RCT2::Limits::kMaxElementHeight) continue;
                const auto type = src.getType();
                ++result.sourceTypes[static_cast<unsigned>(type)];
                if (type == RCT12TileElementType::corrupt) { nextInvisible = !nextInvisible; continue; }
                if (type == RCT12TileElementType::eightCarsCorrupt14 || type == RCT12TileElementType::eightCarsCorrupt15)
                { restInvisible = true; continue; }
                if (type != RCT12TileElementType::surface && type != RCT12TileElementType::path)
                {
                    if (type == RCT12TileElementType::track)
                    {
                        const auto* raw = reinterpret_cast<const uint8_t*>(&src);
                        if (raw[7] != 255 && !ridePresent[raw[7]]) ++result.skippedTrackSnapshots;
                        else
                        {
                            TileElement base;
                            RCT2::ImportS6TileBase(base, src, nextInvisible || restInvisible);
                            result.savedTracks.push_back({ static_cast<uint16_t>(x), static_cast<uint16_t>(y),
                                { 2, src.baseHeight, static_cast<uint8_t>(base.flags & 0x30),
                                    src.getDirection(), raw[4], raw[5], raw[6], raw[7] } });
                        }
                    }
                    ++result.omittedElements; nextInvisible = false; continue;
                }
                check(state.tileElements.size() < MemoryProfile::kTileElements, "Imported tile capacity exceeded");
                auto& dst = state.tileElements.emplace_back();
                RCT2::ImportS6TileBase(dst, src, nextInvisible || restInvisible);
                nextInvisible = false;
                dst.setLastForTile(false);
                if (type == RCT12TileElementType::surface)
                {
                    check(!surfaceAdded, "Multiple surface elements on a tile");
                    RCT2::ImportS6SurfaceData(*dst.asSurface(), *src.asSurface(), terrain);
                    ++result.surfaces;
                    surfaceAdded = true;
                }
                else
                {
                    RCT2::ImportS6PathData(*dst.asPath(), *src.asPath(), legacyPaths, legacyPaths, legacyPaths);
                    ++result.paths;
                }
            } while (!last);
            if (x < mapSize && y < mapSize) check(surfaceAdded, "Active tile has no imported surface");
            if (first == state.tileElements.size())
            {
                auto& dst = state.tileElements.emplace_back();
                dst.clearAs(TileElementType::surface);
            }
            state.tileElements.back().setLastForTile(true);
        }
    }
    // Read packed fields explicitly. These snapshots are not engine Guest
    // instances and must never be ticked or installed into EntityRegistry.
    const auto* entityBytes = static_cast<const uint8_t*>(savedState->GetData()) + 4;
    for (unsigned slot = 0; slot < RCT2::Limits::kMaxEntities; ++slot)
    {
        const auto* p = entityBytes + slot * sizeof(RCT2::Entity);
        if (p[offsetof(RCT2::Peep, EntityIdentifier)] != static_cast<uint8_t>(RCT12EntityIdentifier::peep)
            || p[offsetof(RCT2::Peep, PeepType)] != static_cast<uint8_t>(RCT12PeepType::guest)) continue;
        const auto x = ReadLittleEndian<int16_t>(p + offsetof(RCT2::Peep, x));
        const auto y = ReadLittleEndian<int16_t>(p + offsetof(RCT2::Peep, y));
        const auto z = ReadLittleEndian<int16_t>(p + offsetof(RCT2::Peep, z));
        const auto direction = p[offsetof(RCT2::Peep, EntityDirection)];
        const auto peepState = p[offsetof(RCT2::Peep, State)];
        const auto outside = p[offsetof(RCT2::Peep, OutsideOfPark)];
        const auto shirt = p[offsetof(RCT2::Peep, TshirtColour)];
        const auto trousers = p[offsetof(RCT2::Peep, TrousersColour)];
        if (x < 0 || y < 0 || x >= mapSize * 32 || y >= mapSize * 32 || z < 0 || z > 2040
            || direction > 31 || peepState > 23 || outside > 1 || shirt > 31 || trousers > 31)
        {
            ++result.skippedGuestSnapshots;
            continue;
        }
        result.savedGuests.push_back({ static_cast<uint16_t>(slot), static_cast<uint16_t>(x),
            static_cast<uint16_t>(y), z, direction, peepState, outside, shirt, trousers,
            p[offsetof(RCT2::Peep, AnimationGroup)] });
    }
    std::sort(result.savedGuests.begin(), result.savedGuests.end(), [](const auto& a, const auto& b) {
        const unsigned ka = (a.x / 32 + a.y / 32) * 256 + a.x / 32;
        const unsigned kb = (b.x / 32 + b.y / 32) * 256 + b.x / 32;
        if (ka != kb) return ka < kb;
        if (a.x + a.y != b.x + b.y) return a.x + a.y < b.x + b.y;
        return a.slot < b.slot;
    });
    result.tileIndex = std::make_unique<TilePointerIndex<TileElement>>(
        RCT2::Limits::kMaxMapSize, state.tileElements.data(), state.tileElements.size());
    return result;
}

#pragma GCC diagnostic pop

uint32_t PartialS6Fingerprint(const PartialS6State& imported)
{
    uint32_t hash = 2166136261U;
    auto append = [&](uint64_t value, unsigned width) {
        for (unsigned i = 0; i < width; ++i) { hash = (hash ^ (value & 255)) * 16777619U; value >>= 8; }
    };
    const auto& s = *imported.state;
    append(s.mapSize.x, 2); append(s.mapSize.y, 2);
    append(s.date.monthsElapsed, 4); append(s.date.monthTicks, 2); append(s.currentTicks, 4);
    append(s.scenarioRand.state().s0, 4); append(s.scenarioRand.state().s1, 4);
    append(s.scenarioOptions.initialCash, 8); append(s.park.flags.holder, 8); append(s.park.entranceFee, 2);
    append(s.park.cash, 8); append(s.park.bankLoan, 8); append(s.park.rating, 2); append(s.park.numGuestsInPark, 4);
    for (const auto c : imported.scenarioLabel) append(static_cast<uint8_t>(c), 1);
    append(0, 1);
    append(s.tileElements.size(), 4);
    for (const auto& tile : s.tileElements)
    {
        append(tile.type, 1); append(tile.flags, 1); append(tile.baseHeight, 1); append(tile.clearanceHeight, 1); append(tile.owner, 1);
        if (const auto* p = tile.asSurface())
        {
            append(p->getSurfaceObjectIndex(), 2); append(p->getEdgeObjectIndex(), 2); append(p->getSlope(), 1);
            append(p->getWaterHeight(), 4); append(p->getGrassLength(), 1); append(p->getOwnership().holder, 1);
            append(p->getParkFences(), 1); append(static_cast<uint8_t>(p->getPrimarySurfaceColour()), 1);
            append(static_cast<uint8_t>(p->getPrimaryEdgeColour()), 1);
        }
        else if (const auto* p = tile.asPath())
        {
            append(p->getLegacyPathEntryIndex(), 2); append(p->isQueue(), 1); append(p->isSloped(), 1);
            append(p->getSlopeDirection(), 1); append(p->getRideIndex().ToUnderlying(), 2);
            append(p->getStationIndex().ToUnderlying(), 1); append(p->getEdges(), 1); append(p->getCorners(), 1);
            append(p->getAddition(), 1); append(p->getAdditionStatus(), 1); append(p->isWide(), 1);
            append(p->hasQueueBanner(), 1); append(p->getQueueBannerDirection(), 1); append(p->additionIsGhost(), 1);
            append(p->isBroken(), 1); append(p->isBlockedByVehicle(), 1);
        }
    }
    return hash;
}

void WritePartialS6Overview(const PartialS6State& imported, const char* path)
{
    // A plain PPM makes the imported terrain/path footprint inspectable without
    // requiring graphics libraries or an Amiga display driver in this tool.
    const auto& state = *imported.state;
    const auto size = state.mapSize.x;
    std::vector<uint8_t> pixels(size * size * 3);
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            std::array<uint8_t, 3> colour{ 24, 64, 24 };
            const auto* tile = imported.tileIndex->GetFirstElementAt({ x, y });
            bool last;
            do
            {
                const auto& t = *tile++;
                last = t.isLastForTile();
                if (t.isGhost() || t.isInvisible()) continue;
                if (const auto* surface = t.asSurface())
                {
                    colour = { static_cast<uint8_t>(30 + t.baseHeight), static_cast<uint8_t>(90 + t.baseHeight / 2), 35 };
                    if (surface->getWaterHeight() > t.getBaseZ()) colour = { 32, 96, 180 };
                }
                else if (const auto* p = t.asPath()) colour = p->isQueue()
                    ? std::array<uint8_t, 3>{ 72, 144, 210 } : std::array<uint8_t, 3>{ 210, 194, 148 };
            } while (!last);
            std::copy(colour.begin(), colour.end(), pixels.begin() + (x + y * size) * 3);
        }
    }
    File out(std::fopen(path, "wb"));
    check(out != nullptr, "Cannot create terrain overview");
    const std::string header = "P6\n" + std::to_string(size) + " " + std::to_string(size) + "\n255\n";
    check(std::fwrite(header.data(), 1, header.size(), out.get()) == header.size()
        && std::fwrite(pixels.data(), 1, pixels.size(), out.get()) == pixels.size(), "Cannot write terrain overview");
    check(std::fflush(out.get()) == 0, "Cannot flush terrain overview");
}
