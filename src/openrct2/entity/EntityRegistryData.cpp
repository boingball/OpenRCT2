/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "EntityRegistry.h"
#include "../GameState.h"
#include "../core/Algorithm.hpp"
#include "../core/Guard.hpp"
#include "../world/Location.hpp"
#include <algorithm>
#include <cstdlib>
#include <iterator>

namespace OpenRCT2
{
    using namespace OpenRCT2::Core;


    uint16_t EntityRegistry::getEntityListCount(EntityType type)
    {
        return static_cast<uint16_t>(gEntityLists[EnumValue(type)].size());
    }

    uint16_t EntityRegistry::getNumFreeEntities()
    {
        return static_cast<uint16_t>(_freeIdList.size());
    }

    EntityBase* EntityRegistry::tryGetEntity(EntityId entityIndex)
    {
        const auto idx = entityIndex.ToUnderlying();
        if (idx >= kMaxEntities)
        {
            return nullptr;
        }
#ifdef __amigaos__
        Entity_t* chunk = _entityChunks[idx >> kEntityChunkShift];
        return chunk == nullptr ? nullptr : &chunk[idx & kEntityChunkMask].base;
#else
        return &entities[idx].base;
#endif
    }

    EntityBase* EntityRegistry::getEntity(EntityId entityIndex)
    {
        if (entityIndex.IsNull())
        {
            return nullptr;
        }
        Guard::Assert(entityIndex.ToUnderlying() < kMaxEntities, "Tried getting entity %u", entityIndex.ToUnderlying());
        return tryGetEntity(entityIndex);
    }

    const std::vector<EntityId>& EntityRegistry::getEntityTileList(const CoordsXY& spritePos)
    {
        return gEntitySpatialIndex[computeSpatialIndex(spritePos)];
    }

    void EntityRegistry::resetEntityLists()
    {
        for (auto& list : gEntityLists)
        {
            list.clear();
        }
    }

    void EntityRegistry::resetFreeIds()
    {
        _freeIdList.clear();
        _freeIdList.resize(kMaxEntities);

        // List needs to be back to front to simplify removing
        auto nextId = 0;
        std::for_each(std::rbegin(_freeIdList), std::rend(_freeIdList), [&](auto& elem) {
            elem = EntityId::FromUnderlying(nextId);
            nextId++;
        });
    }

    const std::list<EntityId>& EntityRegistry::getEntityList(const EntityType id)
    {
        return gEntityLists[EnumValue(id)];
    }

    void EntityRegistry::resetEntitySpatialIndices()
    {
        // The only place the stride changes. Every entity is re-inserted below, so no stored index
        // survives it. A map is square here; the larger side decides, and one spare row covers the
        // null bucket.
        const auto& mapSize = getGameState().mapSize;
        const auto wanted = static_cast<uint32_t>(
            std::clamp<int32_t>(std::max(mapSize.x, mapSize.y), 1, kMaximumMapSizeTechnical));
        if (wanted != _spatialStride || gEntitySpatialIndex.size() != static_cast<size_t>(wanted) * wanted + 1)
        {
            // swap rather than assign: assign would keep the old buffer's capacity, which is the memory
            // this is trying to give back.
            std::vector<std::vector<EntityId>> fresh(static_cast<size_t>(wanted) * wanted + 1);
            gEntitySpatialIndex.swap(fresh);
            _spatialStride = wanted;
        }
        for (auto& vec : gEntitySpatialIndex)
        {
            vec.clear();
        }
        for (EntityId::UnderlyingType i = 0; i < kMaxEntities; i++)
        {
            auto* entity = getEntity(EntityId::FromUnderlying(i));
            if (entity != nullptr && entity->type != EntityType::null)
            {
                entitySpatialInsert(*entity, { entity->x, entity->y });
            }
        }
    }

    void EntityRegistry::entityReset(EntityBase& entity)
    {
        // Need to retain how the sprite is linked in lists
        auto entityIndex = entity.id;
        _entityFlashingList[entityIndex.ToUnderlying()] = false;

        Entity_t* tempEntity = reinterpret_cast<Entity_t*>(&entity);
        *tempEntity = Entity_t();

        entity.id = entityIndex;
        entity.type = EntityType::null;
    }

    void EntityRegistry::addToEntityList(EntityBase& entity)
    {
        auto& list = gEntityLists[EnumValue(entity.type)];

        // Entity list is sorted by id to prevent desyncs.
        Algorithm::sortedInsert(list, entity.id);
    }

    void EntityRegistry::prepareNewEntity(EntityBase& base, const EntityType type)
    {
        // Need to reset all sprite data, as the uninitialised values
        // may contain garbage and cause a desync later on.
        entityReset(base);

        base.type = type;
        addToEntityList(base);

        base.x = kLocationNull;
        base.y = kLocationNull;
        base.z = 0;
        base.spriteData.width = 0x10;
        base.spriteData.heightMin = 0x14;
        base.spriteData.heightMax = 0x8;
        base.spriteData.spriteRect = {};
        base.spatialIndex = kInvalidSpatialIndex;

        entitySpatialInsert(base, { kLocationNull, 0 });
    }

    EntityBase* EntityRegistry::createEntityAt(const EntityId index, const EntityType type)
    {
        auto id = Algorithm::binaryFind(std::rbegin(_freeIdList), std::rend(_freeIdList), index);
        if (id == std::rend(_freeIdList))
        {
            return nullptr;
        }

#ifdef __amigaos__
        auto* slot = ensureChunkSlot(index.ToUnderlying());
        auto* entity = slot == nullptr ? nullptr : &slot->base;
#else
        auto* entity = getEntity(index);
#endif
        if (entity == nullptr)
        {
            return nullptr;
        }

        _freeIdList.erase(std::next(id).base());

        prepareNewEntity(*entity, type);
        return entity;
    }

    void EntityRegistry::entitySpatialInsert(EntityBase& entity, const CoordsXY& newLoc)
    {
        const auto newIndex = computeSpatialIndex(newLoc);

        auto& spatialVector = gEntitySpatialIndex[newIndex];

        Algorithm::sortedInsert(spatialVector, entity.id);

        entity.spatialIndex = newIndex;
    }

    uint32_t EntityRegistry::computeSpatialIndex(const CoordsXY& loc) const
    {
        if (loc.isNull())
            return spatialNullBucket();

        // NOTE: The input coordinate is rotated and can have negative components.
        const auto tileX = std::abs(loc.x) / kCoordsXYStep;
        const auto tileY = std::abs(loc.y) / kCoordsXYStep;

        const auto stride = static_cast<int32_t>(_spatialStride);
        if (tileX >= stride || tileY >= stride)
            return spatialNullBucket();

        return static_cast<uint32_t>(tileX) * _spatialStride + static_cast<uint32_t>(tileY);
    }

    // For a freshly constructed, empty registry only. Existing populated
    // registries must use resetAllEntities to release attached peep names etc.
    void EntityRegistry::initialiseEmptyStorage()
    {
        Guard::Assert(_freeIdList.empty(), "Entity storage has already been initialised");
        for (const auto& list : gEntityLists)
            Guard::Assert(list.empty(), "Cannot initialise populated entity storage");
        for (const auto& bucket : gEntitySpatialIndex)
            Guard::Assert(bucket.empty(), "Cannot initialise populated spatial storage");
#ifndef __amigaos__
        std::fill(std::begin(entities), std::end(entities), Entity_t());
        for (uint16_t i = 0; i < kMaxEntities; ++i)
        {
            entities[i].base.type = EntityType::null;
            entities[i].base.id = EntityId::FromUnderlying(i);
            _entityFlashingList[i] = false;
        }
#else
        std::fill(std::begin(_entityFlashingList), std::end(_entityFlashingList), false);
#endif
        resetFreeIds();
    }
} // namespace OpenRCT2
