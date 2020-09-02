/* This file is part of GnastyPlasmaPatcher.
 *
 * GnastyPlasmaPatcher is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnastyPlasmaPatcher is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GnastyPlasmaPatcher.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "patcher.hpp"
#include "errors.hpp"

#include <Debug/plDebug.h>
#include <PRP/Object/plSceneObject.h>
#include <PRP/Object/plSimulationInterface.h>
#include <PRP/Physics/plGenericPhysical.h>
#include <ResManager/plResManager.h>

// ===========================================================================

void gpp::patcher::process_collision()
{
    plDebug::Debug("Processing colliders...");

    auto oldMap = std::move(m_MapFunc);
    m_MapFunc = [this, &oldMap](const plKey& srcKey, const std::vector<plKey>& keys) {
        // This is a common ZLZ replacement for us to check before prompting.
        plKey result = find_named_key(srcKey->getLocation(), srcKey->getType(),
                                      srcKey->getName(), ST_LITERAL("_COLLISION_001"),
                                      keys);
        if (!result.Exists() && oldMap)
            result = oldMap(srcKey, keys);
        return result;
    };

    iterate_objects<plSceneObject>(
        [this](const plSceneObject* srcSO, plSceneObject* dstSO) {
            if (!srcSO->getSimInterface().Exists() && !dstSO->getSimInterface().Exists())
                return true;

            // Was the physical deleted?
            if (!srcSO->getSimInterface().Exists() && dstSO->getSimInterface().Exists()) {
                plDebug::Debug("  -> Deleting '{}' collision...", dstSO->getKey()->getName());
                {
                    auto simIface = plSimulationInterface::Convert(dstSO->getSimInterface()->getObj());
                    m_Destination->DelObject(simIface->getPhysical());
                    m_Destination->DelObject(simIface->getKey());
                }
                dstSO->setSimInterface(plKey());
            } else if (srcSO->getSimInterface().Exists() && dstSO->getSimInterface().Exists()) {
                plDebug::Debug("  -> Patching '{}'", dstSO->getKey()->getName());
                auto srcSimIface = plSimulationInterface::Convert(srcSO->getSimInterface()->getObj());
                auto dstSimIface = plSimulationInterface::Convert(dstSO->getSimInterface()->getObj());
                auto src = plGenericPhysical::Convert(srcSimIface->getPhysical()->getObj());
                auto dst = plGenericPhysical::Convert(dstSimIface->getPhysical()->getObj());

                // Note: these are not all the properties...
                dst->setMass(src->getMass());
                dst->setMemberGroup(src->getMemberGroup());
                dst->setCollideGroup(src->getCollideGroup());

                dst->setBoundsType(src->getBoundsType());
                dst->setDimensions(src->getDimensions());
                dst->setOffset(src->getOffset());
                dst->setRadius(src->getRadius());
                dst->setLength(src->getLength());
                dst->setIndices(src->getIndices().size(), src->getIndices().data());
                dst->setVerts(src->getVerts().size(), src->getVerts().data());
            } else if (srcSO->getSimInterface().Exists() && !dstSO->getSimInterface().Exists()) {
                error::raise("Cannot add collision to '{}' - this is not yet implemented.", dstSO->getKey()->getName());
            }

            m_DirtyPages.insert(dstSO->getKey()->getLocation());
            return true;
        }
    );

    m_MapFunc = std::move(oldMap);
}
