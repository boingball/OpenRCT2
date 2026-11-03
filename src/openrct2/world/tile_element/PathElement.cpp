/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "PathElement.h"

#include "../../Context.h"
#include "../../Diagnostic.h"
#include "../../object/FootpathObject.h"
#include "../../object/FootpathRailingsObject.h"
#include "../../object/FootpathSurfaceObject.h"
#include "../../object/ObjectEntryManager.h"
#include "../../object/ObjectManager.h"
#include "../../object/PathAdditionEntry.h"
#include "../Footpath.h"

namespace OpenRCT2
{

    bool PathElement::hasJunctionRailings() const
    {
        return flags2.has(FootpathElementFlag::hasJunctionRailings);
    }

    void PathElement::setJunctionRailings(bool hasJunctionRailings)
    {
        flags2.set(FootpathElementFlag::hasJunctionRailings, hasJunctionRailings);
    }

    bool PathElement::hasAddition() const
    {
        return additions != 0;
    }

    ObjectEntryIndex PathElement::getAdditionEntryIndex() const
    {
        // `Additions` is set to 0 when there is no addition, so the value 1 corresponds with path addition slot 0, etc.
        return getAddition() - 1;
    }

    const PathAdditionEntry* PathElement::getAdditionEntry() const
    {
        if (!hasAddition())
            return nullptr;
        return ObjectEntryManager::GetObjectEntry<PathAdditionEntry>(getAdditionEntryIndex());
    }

    void PathElement::setAdditionEntryIndex(ObjectEntryIndex entryIndex)
    {
        // `Additions` is set to 0 when there is no addition, so the value 1 corresponds with path addition slot 0, etc.
        additions = entryIndex + 1;
    }

    const FootpathObject* PathElement::getLegacyPathEntry() const
    {
        return GetLegacyFootpathEntry(getLegacyPathEntryIndex());
    }

    const PathSurfaceDescriptor* PathElement::getSurfaceDescriptor() const
    {
        if (hasLegacyPathEntry())
        {
            const auto* legacyPathEntry = getLegacyPathEntry();
            if (legacyPathEntry == nullptr)
                return nullptr;

            if (isQueue())
                return &legacyPathEntry->GetQueueSurfaceDescriptor();

            return &legacyPathEntry->GetPathSurfaceDescriptor();
        }

        const auto* surfaceEntry = getSurfaceEntry();
        if (surfaceEntry == nullptr)
            return nullptr;

        return &surfaceEntry->GetDescriptor();
    }

    const PathRailingsDescriptor* PathElement::getRailingsDescriptor() const
    {
        if (hasLegacyPathEntry())
        {
            const auto* legacyPathEntry = getLegacyPathEntry();
            if (legacyPathEntry == nullptr)
                return nullptr;

            return &legacyPathEntry->GetPathRailingsDescriptor();
        }

        const auto* railingsEntry = getRailingsEntry();
        if (railingsEntry == nullptr)
            return nullptr;

        return &railingsEntry->GetDescriptor();
    }

    const FootpathSurfaceObject* PathElement::getSurfaceEntry() const
    {
        auto& objMgr = GetContext()->GetObjectManager();
        return objMgr.GetLoadedObject<FootpathSurfaceObject>(getSurfaceEntryIndex());
    }

    const FootpathRailingsObject* PathElement::getRailingsEntry() const
    {
        auto& objMgr = GetContext()->GetObjectManager();
        return objMgr.GetLoadedObject<FootpathRailingsObject>(getRailingsEntryIndex());
    }

    bool PathElement::shouldDrawPathOverSupports() const
    {
        // TODO: make this an actual decision of the tile element.
        const auto* const railings = getRailingsDescriptor();
        return railings != nullptr ? railings->flags.has(RailingEntryFlag::drawPathOverSupports) : false;
    }

    void PathElement::setShouldDrawPathOverSupports(bool on)
    {
        LOG_VERBOSE("Setting 'draw path over supports' to %d", static_cast<size_t>(on));
    }
} // namespace OpenRCT2
