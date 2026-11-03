/* SPDX-License-Identifier: GPL-3.0-only */
#undef NDEBUG
#include <openrct2/object/ObjectManager.h>
#include <openrct2/object/ObjectRepository.h>
#include <openrct2/object/ObjectList.h>
#include <cassert>
#include <cstdio>

using namespace OpenRCT2;
static int live = 0, activated = 0;
static bool fail = true;
struct TestObject final : Object
{
    TestObject(const ObjectRepositoryItem& item) { SetDescriptor(ObjectEntryDescriptor(item)); ++live; }
    ~TestObject() override { --live; }
    void Load() override
    {
        ++activated;
        if (fail && activated == 2) throw std::bad_alloc();
    }
    void Unload() override { --activated; }
};
struct Repository final : IObjectRepository
{
    ObjectRepositoryItem items[3]{};
    Repository()
    {
        for (int i = 0; i < 3; ++i)
        {
            items[i].Id = i;
            items[i].Type = i == 2 ? ObjectType::audio : ObjectType::music;
            items[i].Generation = ObjectGeneration::json;
            items[i].Identifier = i == 2 ? "test.audio.keep" : i ? "test.music.second" : "test.music.first";
        }
    }
    void LoadOrConstruct(int32_t) override {}
    void Construct(int32_t) override {}
    size_t GetNumObjects() const override { return 3; }
    const ObjectRepositoryItem* GetObjects() const override { return items; }
    const ObjectRepositoryItem* FindObjectLegacy(std::string_view id) const override { return FindObject(id); }
    const ObjectRepositoryItem* FindObject(std::string_view id) const override
    {
        for (const auto& item : items) if (item.Identifier == id) return &item;
        return nullptr;
    }
    const ObjectRepositoryItem* FindObject(const RCTObjectEntry*) const override { return nullptr; }
    const ObjectRepositoryItem* FindObject(const ObjectEntryDescriptor& descriptor) const override
    { return FindObject(descriptor.Identifier); }
    std::unique_ptr<Object> LoadObject(const ObjectRepositoryItem* item) override { return std::make_unique<TestObject>(*item); }
    std::unique_ptr<Object> LoadObject(const ObjectRepositoryItem* item, bool) override { return LoadObject(item); }
    void RegisterLoadedObject(const ObjectRepositoryItem* item, std::unique_ptr<Object>&& object) override
    { items[item->Id].LoadedObject = std::move(object); }
    void UnregisterLoadedObject(const ObjectRepositoryItem* item, Object* object) override
    {
        assert(items[item->Id].LoadedObject.get() == object);
        items[item->Id].LoadedObject.reset();
    }
    void AddObject(const RCTObjectEntry*, const void*, size_t) override {}
    void AddObjectFromFile(ObjectGeneration, std::string_view, const void*, size_t) override {}
    void ExportPackedObject(IStream*) override {}
};
int main()
{
    Repository repository;
    auto manager = CreateObjectManager(repository);
    ObjectList list;
    for (int i = 0; i < 2; ++i) list.Add(ObjectEntryDescriptor(repository.items[i]));
    try { manager->LoadObjects(list); assert(false); } catch (const std::bad_alloc&) {}
    assert(live == 0 && activated == 0);
    for (const auto& item : repository.items) assert(!item.LoadedObject);
    fail = false;
    manager->LoadObjects(list);
    assert(live == 2 && activated == 2);
    // A static menu must release the former park while keeping the shared
    // audio object. Exercise unload/reload cycles through the actual manager.
    auto* audio = manager->LoadObject("test.audio.keep");
    assert(audio != nullptr && live == 3 && activated == 3);
    for (int i = 0; i < 3; ++i)
    {
        manager->UnloadAllTransient();
        assert(live == 1 && activated == 1);
        assert(!repository.items[0].LoadedObject && !repository.items[1].LoadedObject);
        assert(repository.items[2].LoadedObject.get() == audio);
        manager->LoadObjects(list);
        assert(live == 3 && activated == 3);
        assert(repository.items[2].LoadedObject.get() == audio);
    }
    manager->UnloadAll();
    assert(live == 0 && activated == 0);
    puts("Object load: injected bad_alloc rolls back repository/activation; retry, transient park release/audio retention, repeated reload and shutdown PASS");
}
