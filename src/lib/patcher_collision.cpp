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
#include <PRP/Physics/plGenericPhysical.h>
#include <ResManager/plResManager.h>

// ===========================================================================

void gpp::patcher::process_collision()
{
    plDebug::Debug("Processing colliders...");

    iterate_objects<plGenericPhysical>(
        [this](const plGenericPhysical* src, plGenericPhysical* dst) {
            plDebug::Debug("  -> Patching '{}'", dst->getKey()->getName());
            m_DirtyPages.insert(dst->getKey()->getLocation());

            // TODO: the various properties
            dst->setBoundsType(src->getBoundsType());
            dst->setDimensions(src->getDimensions());
            dst->setOffset(src->getOffset());
            dst->setRadius(src->getRadius());
            dst->setLength(src->getLength());
            dst->setIndices(src->getIndices().size(), src->getIndices().data());
            dst->setVerts(src->getVerts().size(), src->getVerts().data());

            // All the key resolve stuff was handled by the iterator code. If we have to drop
            // back down to object level, I will be sad.
            return true;
        }
    );
}
