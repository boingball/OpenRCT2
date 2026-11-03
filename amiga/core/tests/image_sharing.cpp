/* SPDX-License-Identifier: GPL-3.0-only */
#undef NDEBUG
#include <openrct2/PlatformEnvironment.h>
#include <openrct2/SpriteIds.h>
#include <openrct2/core/Json.hpp>
#include <openrct2/drawing/Drawing.Sprite.h>
#include <openrct2/object/Object.h>
#include <openrct2/object/ObjectAsset.h>
#include <cassert>
#include <cstdio>

using namespace OpenRCT2;

struct Environment final : IPlatformEnvironment
{
    u8string path;
    u8string GetDirectoryPath(DirBase) const override { return {}; }
    u8string GetDirectoryPath(DirBase, DirId) const override { return {}; }
    u8string GetFilePath(PathId) const override { return path; }
    u8string FindFile(DirBase, DirId, u8string_view) const override { return path; }
    void SetBasePath(DirBase, u8string_view) override {}
    bool IsUsingClassic() const override { return false; }
};

struct ReadContext final : IReadObjectContext
{
    std::string_view GetObjectIdentifier() override { return "test.g1.sharing"; }
    bool ShouldLoadImages() override { return true; }
    std::vector<uint8_t> GetData(std::string_view) override { assert(false); return {}; }
    ObjectAsset GetAsset(std::string_view) override { assert(false); return {}; }
    void LogVerbose(ObjectError, const utf8*) override {}
    void LogWarning(ObjectError, const utf8*) override { assert(false); }
    void LogError(ObjectError, const utf8*) override { assert(false); }
};

static void little(FILE* f, uint32_t n, int bytes)
{
    while (bytes--) { assert(fputc(n & 255, f) != EOF); n >>= 8; }
}

int main(int argc, char** argv)
{
    Environment env;
    env.path = argc > 1 ? argv[1] : "RAM:OpenRCT2-G1Sharing.tmp";
    auto* f = fopen(env.path.c_str(), "wb");
    assert(f);
    little(f, SPR_G1_END, 4); little(f, 21, 4);
    for (uint32_t i = 0; i < SPR_G1_END; ++i)
    {
        const bool zoom = i == 1 || i == 2;
        little(f, i == 0 ? 20 : i == 1 ? 16 : 0, 4);
        little(f, i == 0 ? 1 : i == 1 ? 2 : 4, 2);
        little(f, i == 0 ? 1 : i == 1 ? 2 : 4, 2);
        little(f, 0, 2); little(f, 0, 2);
        little(f, zoom ? 16 : 0, 2); little(f, zoom ? 1 : 0, 2);
    }
    for (int i = 0; i < 21; ++i) assert(fputc(i + 1, f) != EOF);
    assert(fclose(f) == 0);
    assert(GfxLoadG1(env));
    ReadContext context;
    const auto* base = GfxGetG1Element(2);
    const auto originalOffset = base->xOffset;
    for (int round = 0; round < 16; ++round)
    {
        ImageTable table;
        auto json = json_t::parse(R"({"images":["$G1[2]",{"gx":"$G1[2]","x":11,"y":-4}]})");
        table.ReadJson(&context, json);
        assert(table.GetCount() == 6);
        const auto* entries = table.GetImages();
        assert(entries[0].offset == base->offset && entries[1].offset == base->offset);
        assert(entries[0].xOffset == originalOffset && entries[1].xOffset == 11 && entries[1].yOffset == -4);
        assert(base->xOffset == originalOffset);
        for (uint32_t index : { 0u, 1u })
        {
            const auto zoom1 = index - entries[index].zoomedOffset;
            const auto zoom2 = zoom1 - entries[zoom1].zoomedOffset;
            assert(zoom1 < table.GetCount() && zoom2 < table.GetCount());
            assert(entries[zoom1].offset == GfxGetG1Element(1)->offset);
            assert(entries[zoom2].offset == GfxGetG1Element(0)->offset);
            assert(entries[zoom2].offset[0] == 21);
        }
        // Mixed ownership: destruction must free this copy, never the G1 bank.
        table.AddImage(base);
        assert(table.GetImages()[6].offset != base->offset);
    }
    assert(base->offset[0] == 1);
    GfxUnloadG1();
    assert(remove(env.path.c_str()) == 0);
    puts("G1 shared pixels/private headers/two zoom levels/mixed ownership/repeated destruction: PASS");
}
