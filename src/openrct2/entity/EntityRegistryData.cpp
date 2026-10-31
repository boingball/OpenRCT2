/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "EntityRegistry.h"
#include "EntitySpatialIndex.hpp"
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
        return static_cast<uint16_t>(_entityLists[EnumValue(type)].size());
    }

    uint16_t EntityRegistry::getNumFreeEntities()
    {
        return static_cast<uint16_t>(_freeIdList.size());
    }

    EntityBase* EntityRegistry::tryGetEntity(EntityId entityIndex)
    {
        const auto idx = entityIndex.ToUnderlying();
        return idx >= kMaxEntities ? nullptr : &entities[idx].base;
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
        return _entitySpatialIndex[ComputeSpatialIndex(spritePos)];
    }

    void EntityRegistry::resetEntityLists()
    {
        for (auto& list : _entityLists)
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
        return _entityLists[EnumValue(id)];
    }

    void EntityRegistry::resetEntitySpatialIndices()
    {
        for (auto& vec : _entitySpatialIndex)
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
        auto& list = _entityLists[EnumValue(entity.type)];

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

        auto* entity = getEntity(index);
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
        const auto newIndex = ComputeSpatialIndex(newLoc);

        auto& spatialVector = _entitySpatialIndex[newIndex];

        Algorithm::sortedInsert(spatialVector, entity.id);

        entity.spatialIndex = newIndex;
    }

    // For a freshly constructed, empty registry only. Existing populated
    // registries must use resetAllEntities to release attached peep names etc.
    void EntityRegistry::initialiseEmptyStorage()
    {
        Guard::Assert(_freeIdList.empty(), "Entity storage has already been initialised");
        for (const auto& list : _entityLists)
            Guard::Assert(list.empty(), "Cannot initialise populated entity storage");
        for (const auto& bucket : _entitySpatialIndex)
            Guard::Assert(bucket.empty(), "Cannot initialise populated spatial storage");
        std::fill(std::begin(entities), std::end(entities), Entity_t());
        for (uint16_t i = 0; i < kMaxEntities; ++i)
        {
            entities[i].base.type = EntityType::null;
            entities[i].base.id = EntityId::FromUnderlying(i);
            _entityFlashingList[i] = false;
        }
        resetFreeIds();
    }
} // namespace OpenRCT2
