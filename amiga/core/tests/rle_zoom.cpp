/* SPDX-License-Identifier: GPL-3.0-only */
#include <openrct2/drawing/Drawing.Sprite.h>
#include <openrct2/drawing/RenderTarget.h>
#include <algorithm>
#include <cstdio>
#include <vector>

#ifdef __amigaos__
extern "C" int amiga_env_flag(const char* name);
extern "C" int amiga_env_flag(const char*) { return 0; }
#endif

int main()
{
    using namespace OpenRCT2;
    using namespace OpenRCT2::Drawing;
    // Keep wrong-endian offsets within the fixture too: the old renderer
    // then produces wrong pixels instead of reading beyond the allocation.
    std::vector<uint8_t> storage(65540, 0x80);
    uint8_t* sprite = storage.data() + 1; // also test unaligned sprite data
    const unsigned offsets[] = { 0x123, 0x141, 0x163 };
    for (unsigned y = 0; y < 3; ++y)
    {
        sprite[y * 2] = static_cast<uint8_t>(offsets[y]);
        sprite[y * 2 + 1] = static_cast<uint8_t>(offsets[y] >> 8);
    }
    const uint8_t row0[] = { 2, 1, 11, 12, 0x82, 4, 14, 15 };
    const uint8_t row1[] = { 0x86, 0, 21, 22, 23, 24, 25, 26 };
    const uint8_t row2[] = { 0x83, 2, 31, 32, 33 };
    std::copy(std::begin(row0), std::end(row0), sprite + offsets[0]);
    std::copy(std::begin(row1), std::end(row1), sprite + offsets[1]);
    std::copy(std::begin(row2), std::end(row2), sprite + offsets[2]);
    const uint8_t pixels[3][6] = { { 0, 11, 12, 0, 14, 15 }, { 21, 22, 23, 24, 25, 26 }, { 0, 0, 31, 32, 33, 0 } };
    G1Element image{};
    image.offset = sprite;
    image.width = 6;
    image.height = 3;
    std::vector<PaletteIndex> map(65536);
    for (size_t i = 0; i < map.size(); ++i)
        map[i] = static_cast<PaletteIndex>((i * 3 + 7) % 255 + 1);
    map[12] = PaletteIndex::transparent;
    PaletteMap palette(map.data(), 256, 256);
    unsigned cases = 0;
    for (int8_t level : { -2, -1, 0 })
    {
        const int scale = 1 << -level;
        for (int clipped : { 0, 1 })
        {
            const int sx = clipped, sy = clipped;
            const int width = 6 * scale - sx - clipped;
            const int height = 3 * scale - sy - clipped;
            for (int mode = 0; mode < 4; ++mode)
            {
                constexpr int stride = 47, rows = 20, origin = 16 + 2 * stride + 3;
                std::vector<PaletteIndex> actual(32 + stride * rows, static_cast<PaletteIndex>(119));
                auto expected = actual;
                ImageId id(0);
                if (mode == 1 || mode == 3) id = id.WithPrimary(static_cast<Colour>(0));
                if (mode == 2 || mode == 3) id = id.WithBlended(true);
                RenderTarget rt{ .bits = actual.data() + 16, .width = 40, .height = rows,
                    .pitch = stride - 40, .zoom_level = ZoomLevel(level) };
                DrawSpriteArgs args(id, palette, image, sx, sy, width, height, actual.data() + origin);
                for (int y = 0; y < height; ++y)
                    for (int x = 0; x < width; ++x)
                    {
                        const unsigned pixel = pixels[(sy + y) / scale][(sx + x) / scale];
                        if (pixel == 0) continue;
                        unsigned value = pixel;
                        if (mode == 1) value = static_cast<unsigned>(map[pixel]);
                        if (mode == 2) value = static_cast<unsigned>(map[119]);
                        if (mode == 3) value = static_cast<unsigned>(map[(pixel - 1) * 256 + 119]);
                        if (value != 0) expected[origin + y * stride + x] = static_cast<PaletteIndex>(value);
                    }
                GfxRleSpriteToBuffer(rt, args);
                if (actual != expected)
                {
                    std::printf("RLE zoom %d clip %d mode %d: FAIL\n", level, clipped, mode);
                    return 10;
                }
                ++cases;
            }
        }
    }
    std::printf("RLE pixel reference: %u cases at 1x/2x/4x, clipping, remap, blend and canaries PASS\n", cases);
}
