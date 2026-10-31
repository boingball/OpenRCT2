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

#include "../Diagnostic.h"
#include "../GameState.h"
#include "../core/Algorithm.hpp"
#include "../core/ChecksumStream.h"
#include "../core/DataSerialiser.h"
#include "../core/Guard.hpp"
#include "../core/String.hpp"
#include "../entity/EntityList.h"
#include "../entity/Staff.h"
#include "../interface/Viewport.h"
#include "../peep/RideUseSystem.h"
#include "../profiling/Profiling.h"
#include "../ride/Vehicle.h"
#include "../world/Map.h"
#include "Balloon.h"
#include "Duck.h"
#include "EntityTweener.h"
#include "JumpingFountain.h"
#include "MoneyEffect.h"
#include "Particle.h"

#include <cassert>
#include <iterator>
#include <vector>

namespace OpenRCT2
{
    using namespace OpenRCT2::Core;

    static constexpr uint32_t GetSpatialIndex(EntityBase& entity)
    {
        return entity.spatialIndex & ~kSpatialIndexDirtyMask;
    }

    static constexpr bool EntityTypeIsMiscEntity(const EntityType type)
    {
        switch (type)
        {
            case EntityType::steamParticle:
            case EntityType::moneyEffect:
            case EntityType::crashedVehicleParticle:
            case EntityType::explosionCloud:
            case EntityType::crashSplash:
            case EntityType::explosionFlare:
            case EntityType::jumpingFountain:
            case EntityType::balloon:
            case EntityType::duck:
                return true;
            default:
                return false;
        }
    }

    std::string EntitiesChecksum::toString() const
    {
        return String::StringFromHex(raw);
    }

    /**
     *
     *  rct2: 0x0069EB13
     */
    void EntityRegistry::resetAllEntities()
    {
        // Free all associated Entity pointers prior to zeroing memory
        for (int32_t i = 0; i < kMaxEntities; ++i)
        {
            auto* spr = getEntity(EntityId::FromUnderlying(i));
            if (spr == nullptr)
            {
                continue;
            }
            freeEntity(*spr);
        }

        std::fill(std::begin(entities), std::end(entities), Entity_t());
        RideUse::GetHistory().Clear();
        RideUse::GetTypeHistory().Clear();
        for (int32_t i = 0; i < kMaxEntities; ++i)
        {
            auto* spr = getEntity(EntityId::FromUnderlying(i));
            if (spr == nullptr)
            {
                continue;
            }
            spr->type = EntityType::null;
            spr->id = EntityId::FromUnderlying(i);

            _entityFlashingList[i] = false;
        }
        resetEntityLists();
        resetFreeIds();
        resetEntitySpatialIndices();
    }

#ifndef DISABLE_NETWORK
    EntitiesChecksum EntityRegistry::getAllEntitiesChecksum()
    {
        EntitiesChecksum checksum{};

        ChecksumStream ms(checksum.raw);
        DataSerialiser ds(true, ms);
        networkSerialiseEntityTypes<Guest, Staff, Vehicle, Litter>(ds);

        return checksum;
    }
#else
    EntitiesChecksum EntityRegistry::getAllEntitiesChecksum()
    {
        return EntitiesChecksum{};
    }
#endif // DISABLE_NETWORK

    void EntityRegistry::addToFreeList(EntityId index)
    {
        // Free list must be in reverse sprite_index order to prevent desync issues
        _freeIdList.insert(std::upper_bound(std::rbegin(_freeIdList), std::rend(_freeIdList), index).base(), index);
    }

    void EntityRegistry::removeFromEntityList(EntityBase& entity)
    {
        auto& list = _entityLists[EnumValue(entity.type)];
        auto ptr = Algorithm::binaryFind(std::begin(list), std::end(list), entity.id);
        if (ptr != std::end(list))
        {
            list.erase(ptr);
        }
    }

    uint16_t EntityRegistry::getMiscEntityCount()
    {
        uint16_t count = 0;
        for (auto id : { EntityType::steamParticle, EntityType::moneyEffect, EntityType::crashedVehicleParticle,
                         EntityType::explosionCloud, EntityType::crashSplash, EntityType::explosionFlare,
                         EntityType::jumpingFountain, EntityType::balloon, EntityType::duck })
        {
            count += getEntityListCount(id);
        }
        return count;
    }

    EntityBase* EntityRegistry::createEntity(EntityType type)
    {
        if (_freeIdList.empty())
        {
            // No free sprites.
            return nullptr;
        }

        if (EntityTypeIsMiscEntity(type))
        {
            // Misc sprites are commonly used for effects, give other entity types higher priority.
            if (getMiscEntityCount() >= kMaxMiscEntities)
            {
                return nullptr;
            }

            // If there are less than kMaxMiscEntities free slots, ensure other entities can be created.
            if (_freeIdList.size() < kMaxMiscEntities)
            {
                return nullptr;
            }
        }

        auto* entity = getEntity(_freeIdList.back());
        if (entity == nullptr)
        {
            return nullptr;
        }
        _freeIdList.pop_back();

        prepareNewEntity(*entity, type);

        return entity;
    }

    /**
     *
     *  rct2: 0x00672AA4
     */
    void EntityRegistry::updateAllMiscEntities()
    {
        PROFILED_FUNCTION();

        miscUpdateAllTypes<
            SteamParticle, MoneyEffect, VehicleCrashParticle, ExplosionCloud, CrashSplashParticle, ExplosionFlare,
            JumpingFountain, Balloon, Duck>();
    }

    void EntityRegistry::updateMoneyEffect()
    {
        miscUpdateAllTypes<MoneyEffect>();
    }

    void EntityRegistry::entitySpatialRemove(EntityBase& entity)
    {
        const auto currentIndex = GetSpatialIndex(entity);

        auto& spatialVector = _entitySpatialIndex[currentIndex];
        auto index = Algorithm::binaryFind(std::begin(spatialVector), std::end(spatialVector), entity.id);
        if (index != std::end(spatialVector))
        {
            spatialVector.erase(index, index + 1);
        }
        else
        {
            LOG_WARNING("Bad sprite spatial index. Rebuilding the spatial index...");
            resetEntitySpatialIndices();
        }

        entity.spatialIndex = kInvalidSpatialIndex;
    }

    void EntityRegistry::updateEntitySpatialIndex(EntityBase& entity)
    {
        if (entity.spatialIndex & kSpatialIndexDirtyMask)
        {
            if (entity.spatialIndex != kInvalidSpatialIndex)
            {
                entitySpatialRemove(entity);
            }
            entitySpatialInsert(entity, { entity.x, entity.y });
        }
    }

    void EntityRegistry::updateEntitiesSpatialIndex()
    {
        for (auto& entityList : _entityLists)
        {
            for (auto& entityId : entityList)
            {
                auto* entity = tryGetEntity(entityId);
                if (entity != nullptr && entity->type != EntityType::null)
                {
                    updateEntitySpatialIndex(*entity);
                }
            }
        }
    }

    /**
     * Frees any dynamically attached memory to the entity, such as peep name.
     */
    void EntityRegistry::freeEntity(EntityBase& entity)
    {
        auto* guest = entity.as<Guest>();
        auto* staff = entity.as<Staff>();
        if (staff != nullptr)
        {
            staff->setName({});
            staff->clearPatrolArea();
        }
        else if (guest != nullptr)
        {
            guest->setName({});
            guest->guestNextInQueue = EntityId::GetNull();

            RideUse::GetHistory().RemoveHandle(guest->id);
            RideUse::GetTypeHistory().RemoveHandle(guest->id);
        }
    }

    /**
     *
     *  rct2: 0x0069EDB6
     */
    void EntityRegistry::entityRemove(EntityBase* entity)
    {
        freeEntity(*entity);

        EntityTweener::get().removeEntity(entity);
        removeFromEntityList(*entity); // remove from existing list
        addToFreeList(entity->id);

        entitySpatialRemove(*entity);
        entityReset(*entity);
    }

    /**
     * Loops through all floating entities and removes them.
     * Returns the amount of removed objects as feedback.
     */
    uint16_t EntityRegistry::removeFloatingEntities()
    {
        uint16_t removed = 0;
        for (auto* balloon : EntityList<Balloon>())
        {
            entityRemove(balloon);
            removed++;
        }
        for (auto* duck : EntityList<Duck>())
        {
            if (duck->isFlying())
            {
                entityRemove(duck);
                removed++;
            }
        }
        for (auto* money : EntityList<MoneyEffect>())
        {
            entityRemove(money);
            removed++;
        }
        return removed;
    }

    void EntityRegistry::entitySetFlashing(EntityBase* entity, bool flashing)
    {
        assert(entity->id.ToUnderlying() < kMaxEntities);
        _entityFlashingList[entity->id.ToUnderlying()] = flashing;
    }

    bool EntityRegistry::entityGetFlashing(EntityBase* entity)
    {
        assert(entity->id.ToUnderlying() < kMaxEntities);
        return _entityFlashingList[entity->id.ToUnderlying()];
    }
} // namespace OpenRCT2

using namespace OpenRCT2;

CoordsXYZ EntityBase::getLocation() const
{
    return { x, y, z };
}

void EntityBase::setLocation(const CoordsXYZ& newLocation)
{
    if (getLocation() == newLocation)
    {
        // No change, this can happen quite often when the entity is interpolated.
        return;
    }

    x = newLocation.x;
    y = newLocation.y;
    z = newLocation.z;

    if (spatialIndex & kSpatialIndexDirtyMask)
    {
        // Already marked as dirty.
        return;
    }

    const auto newSpatialIndex = ComputeSpatialIndex({ x, y });
    if (newSpatialIndex == GetSpatialIndex(*this))
    {
        // Avoid marking it dirty when we don't leave the current tile.
        return;
    }

    spatialIndex |= kSpatialIndexDirtyMask;
}

static void EntitySetCoordinates(const CoordsXYZ& entityPos, EntityBase* entity)
{
    auto screenCoords = Translate3DTo2DWithZ(GetCurrentRotation(), entityPos);

    entity->spriteData.spriteRect = ScreenRect(
        screenCoords - ScreenCoordsXY{ entity->spriteData.width, entity->spriteData.heightMin },
        screenCoords + ScreenCoordsXY{ entity->spriteData.width, entity->spriteData.heightMax });
    entity->setLocation(entityPos);
}

void EntityBase::moveTo(const CoordsXYZ& newLocation)
{
    if (x != kLocationNull)
    {
        // Invalidate old position.
        invalidate();
    }

    auto loc = newLocation;
    if (!MapIsLocationValid(loc))
    {
        loc.x = kLocationNull;
    }

    if (loc.x == kLocationNull)
    {
        setLocation(loc);
    }
    else
    {
        EntitySetCoordinates(loc, this);
        invalidate(); // Invalidate new position.
    }
}

void EntityBase::moveToAndUpdateSpatialIndex(const CoordsXYZ& newLocation)
{
    moveTo(newLocation);

    // TODO: pass as param instead of relying on global game state
    auto& gameState = getGameState();

    gameState.entities.updateEntitySpatialIndex(*this);
}
