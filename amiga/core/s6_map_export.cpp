/* SPDX-License-Identifier: GPL-3.0-only */
#include "s6_partial_import.h"
#include "../prototype/src/park_map.h"
#include <openrct2/core/Endianness.h>
#include <openrct2/world/tile_element/SurfaceElement.h>
#include <openrct2/world/tile_element/PathElement.h>
#include <openrct2/world/tile_element/Slope.h>
#include <cstdio>
#include <stdexcept>
#include <vector>

void WritePartialS6Map(const PartialS6State& imported, const char* path)
{
    const auto side = static_cast<unsigned>(imported.state->mapSize.x);
    const unsigned cells = side * side;
    std::vector<uint8_t> bytes(PARK_MAP_HEADER + (cells + 1) * 4);
    unsigned elements = 0;
    size_t trackCursor = 0;
    for (unsigned y = 0; y < side; ++y)
    {
        for (unsigned x = 0; x < side; ++x)
        {
            WriteLittleEndian<uint32_t>(bytes.data() + PARK_MAP_HEADER + (x + y * side) * 4, elements);
            const auto* tile = imported.tileIndex->GetFirstElementAt({ static_cast<int>(x), static_cast<int>(y) });
            bool last;
            do
            {
                uint8_t record[PARK_MAP_RECORD]{};
                const auto& t = *tile++;
                last = t.isLastForTile();
                record[0] = t.asPath() ? 1 : 0;
                record[1] = t.baseHeight;
                record[2] = t.flags & 0x30;
                if (const auto* p = t.asSurface())
                {
                    const auto corners = OpenRCT2::GetSlopeRelativeCornerHeights(p->getSlope());
                    record[3] = corners.top | (corners.right << 2) | (corners.bottom << 4) | (corners.left << 6);
                    record[4] = static_cast<uint8_t>(p->getSurfaceObjectIndex());
                    record[5] = static_cast<uint8_t>(p->getWaterHeight() / 8);
                    record[6] = p->getGrassLength();
                    record[7] = p->getOwnership().holder;
                }
                else if (const auto* p = t.asPath())
                {
                    record[3] = p->getEdges() | (p->getCorners() << 4);
                    record[4] = p->getSlopeDirection() | (p->isQueue() ? 4 : 0)
                        | (p->isSloped() ? 8 : 0) | (p->isWide() ? 16 : 0);
                    record[5] = static_cast<uint8_t>(p->getLegacyPathEntryIndex());
                }
                else throw std::runtime_error("Unsupported element in partial map export");
                bytes.insert(bytes.end(), std::begin(record), std::end(record));
                ++elements;
            } while (!last);
            while (trackCursor < imported.savedTracks.size()
                && imported.savedTracks[trackCursor].x == x && imported.savedTracks[trackCursor].y == y)
            {
                const auto& record = imported.savedTracks[trackCursor++].record;
                if (elements >= PARK_MAP_MAX_ELEMENTS) throw std::runtime_error("Map snapshot element capacity exceeded");
                bytes.insert(bytes.end(), record.begin(), record.end());
                ++elements;
            }
        }
    }
    if (elements > PARK_MAP_MAX_ELEMENTS || trackCursor != imported.savedTracks.size())
        throw std::runtime_error("Map snapshot element capacity/order invalid");
    WriteLittleEndian<uint32_t>(bytes.data() + PARK_MAP_HEADER + cells * 4, elements);
    for (const auto& guest : imported.savedGuests)
    {
        uint8_t record[PARK_MAP_GUEST_RECORD]{};
        WriteLittleEndian<uint16_t>(record, guest.slot);
        WriteLittleEndian<uint16_t>(record+2, guest.x);
        WriteLittleEndian<uint16_t>(record+4, guest.y);
        WriteLittleEndian<int16_t>(record+6, guest.z);
        record[8]=guest.direction; record[9]=guest.state; record[10]=guest.outside;
        record[11]=guest.shirt; record[12]=guest.trousers; record[13]=guest.animationGroup;
        bytes.insert(bytes.end(), std::begin(record), std::end(record));
    }
    for (const auto& ride : imported.savedRides) bytes.insert(bytes.end(), ride.begin(), ride.end());
    WriteLittleEndian<uint32_t>(bytes.data()+24, imported.savedGuests.size());
    WriteLittleEndian<uint32_t>(bytes.data()+28, imported.savedRides.size());
    bytes[0]='A'; bytes[1]='M'; bytes[2]='A'; bytes[3]='P';
    WriteLittleEndian<uint16_t>(bytes.data()+4, 3);
    WriteLittleEndian<uint16_t>(bytes.data()+6, side);
    WriteLittleEndian<uint32_t>(bytes.data()+8, cells);
    WriteLittleEndian<uint32_t>(bytes.data()+12, elements);
    WriteLittleEndian<uint32_t>(bytes.data()+16, PartialS6Fingerprint(imported));
    uint32_t hash = 2166136261U;
    for (size_t i=PARK_MAP_HEADER;i<bytes.size();++i) hash=(hash^bytes[i])*16777619U;
    WriteLittleEndian<uint32_t>(bytes.data()+20, hash);
    FILE* output=std::fopen(path,"wb");
    if (!output) throw std::runtime_error("Cannot create imported map file");
    const bool written=std::fwrite(bytes.data(),1,bytes.size(),output)==bytes.size();
    const bool closed=std::fclose(output)==0;
    if (!written || !closed) throw std::runtime_error("Cannot write imported map file");
}
