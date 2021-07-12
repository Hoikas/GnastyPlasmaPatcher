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
#include "span_hacker.hpp"

#include <algorithm>
#include <regex>

#include <Debug/plDebug.h>
#include <PRP/Object/plDrawInterface.h>
#include <PRP/Object/plSceneObject.h>
#include <PRP/Object/plSimulationInterface.h>
#include <PRP/Physics/plGenericPhysical.h>
#include <PRP/plSceneNode.h>
#include <ResManager/plFactory.h>
#include <ResManager/plResManager.h>

using namespace ST::literals;

 // ===========================================================================

gpp::patcher::patcher(const std::filesystem::path& source, const std::filesystem::path& dest)
{
    sanity_check_paths(source, dest);
    m_Source = load(source);
    m_Destination = load(dest);
    sanity_check_registry();
}

void gpp::patcher::sanity_check_registry() const
{
    plDebug::Debug("Checking if merge environment is sane...");

    if (!m_Source || !m_Destination)
        error::raise("WTF? ResManagers are null?!?!");

    if (m_Source->getLocations().empty())
        error::raise("No pages were loaded from the source!");
    if (m_Destination->getLocations().empty())
        error::raise("No pages were loaded from the destination!");
    if (m_Source->getKeys(kSceneObject).empty())
        error::raise("No SceneObjects were loaded from the source!");
    if (m_Destination->getKeys(kSceneObject).empty())
        error::raise("No SceneObjects were loaded from the destination!");

    {
        auto dstLocs = m_Destination->getLocations();
        for (const auto& i : m_Source->getLocations()) {
            auto result = std::find(dstLocs.begin(), dstLocs.end(), i);
            auto srcPage = m_Source->FindPage(i);
            if (result == dstLocs.end()) {
                error::raise("Unable to find '{}' in the destination",
                    srcPage->getFilename(m_Source->getVer()));
            }

            auto dstPage = m_Destination->FindPage(i);
            if (srcPage->getAge().compare_i(dstPage->getAge()) != 0) {
                error::raise("Age name mismatch in source page '{}'... [SRC: {}] [DST: {}]",
                    srcPage->getFilename(m_Source->getVer()), srcPage->getAge(),
                    dstPage->getAge());
            }

            if (srcPage->getPage().compare_i(dstPage->getPage()) != 0) {
                error::raise("Page name mismatch in source page '{}'... [SRC: {}] [DST: {}]",
                    srcPage->getFilename(m_Source->getVer()), srcPage->getPage(),
                    dstPage->getPage());
            }
        }
    }

    plDebug::Debug("... patch environment is grinning and holding a spatula.");
}

// ===========================================================================

plKey gpp::patcher::find_named_key(const plLocation& loc, uint16_t classType, const ST::string& name,
                                   const std::vector<plKey>& haystack) const
{
    plKey result;
    auto findIt = std::find_if(haystack.begin(), haystack.end(),
        [&loc, classType, &name](const plKey& i) {
        return (loc == i->getLocation()) &&
            (classType == i->getType()) &&
            (name.compare_i(i->getName()) == 0);
    }
    );
    if (findIt != haystack.end())
        result = *findIt;
    return result;
}

plKey gpp::patcher::find_named_key(const plLocation& loc, uint16_t classType, const ST::string& name,
                                   const ST::string& suffix, const std::vector<plKey>& haystack) const
{
    plKey result;
    if (!suffix.empty()) {
        ST_ssize_t pos = name.find_last(suffix);
        if (pos + suffix.size() == name.size())
            result = find_named_key(loc, classType, name.left(pos), haystack);
    } else {
        result = find_named_key(loc, classType, name, haystack);
    }
    return result;
}

plKey gpp::patcher::find_homologous_key(const plKey& needle,
                                        const std::function<bool(const plKey&, const plKey&)> func)
{
    auto dstKeys = m_Destination->getKeys(needle->getLocation(), needle->getType());

    plKey dstKey = find_homologous_key(needle, dstKeys);
    if (dstKey.Exists()) {
        if (func && func(needle, dstKey))
            return dstKey;
    }

    // now we ask external code for key name suggestions until they stop giving us any.
    if (!m_MapFunc) {
        plDebug::Error("  -> Cannot map [{}] '{}' to another key - no function available",
            plFactory::ClassName(needle->getType()), needle->getName());
        return dstKey;
    }

    do {
        plKey suggestion = map_homologous_key(needle, dstKeys);
        if (!suggestion.Exists()) {
            plDebug::Error("  -> No match available for [{}] '{}'",
                plFactory::ClassName(needle->getType()), needle->getName());
            break;
        }

        plDebug::Debug("  -> Trying suggested override for [{}] '{}' -> '{}'",
            plFactory::ClassName(needle->getType()),
            needle->getName(), suggestion->getName());
        if (!func || (func && func(needle, suggestion))) {
            m_KeyLUT[needle] = suggestion;
            break;
        } else {
            plDebug::Error("  -> Iterator rejected suggested override for [{}] '{}' -> '{}'",
                plFactory::ClassName(needle->getType()),
                needle->getName(), suggestion->getName());
            continue;
        }

        error::raise("gpp::patcher::find_homologous_key() - potential infinite loop");
    } while (1);

    return dstKey;
}

plKey gpp::patcher::find_homologous_key(const plKey& needle,
                                        const std::vector<plKey>& haystack)
{
    plKey result = find_named_key(needle->getLocation(), needle->getType(), needle->getName(), haystack);
    if (result.Exists())
        return result;

    // either the key was rejected by the iterator OR we just couldn't find it. so check the LUT
    // and the finder.
    auto lutIt = m_KeyLUT.find(needle);
    if (lutIt != m_KeyLUT.end()) {
        plDebug::Debug("  -> Using cached key override for [{}] '{}' -> '{}'",
            plFactory::ClassName(needle->getType()),
            needle->getName(), lutIt->second->getName());
        result = lutIt->second;
    }

    return result;
}

plKey gpp::patcher::map_homologous_key(const plKey& needle,
                                       const std::vector<plKey>& haystack) const
{
    plKey result;
    if (m_MapFunc) {
        result = m_MapFunc(needle, haystack);
        if (!result.Exists()) {
            plDebug::Error("  -> No match available for [{}] '{}'",
                plFactory::ClassName(needle->getType()), needle->getName());
        }
    }
    return result;
}

void gpp::patcher::iterate_keys(uint16_t classType,
                                const std::function<bool(const plKey&, const plKey&)> iter)
{
    for (const auto& loc : m_Source->getLocations()) {
        auto srcKeys = m_Source->getKeys(loc, classType);
        for (const auto& i : srcKeys) {
            (void)find_homologous_key(i, iter);
        }
    }
}

// ===========================================================================

namespace
{
    class override_map_func
    {
        class very_gnawty : public gpp::patcher
        {
        public:
            gpp::object_mapping_func& map_func() { return m_MapFunc; }
        };

        very_gnawty* m_Patcher;
        gpp::object_mapping_func m_MapFunc;

    public:
        override_map_func() = delete;
        override_map_func(gpp::patcher* patcher, gpp::object_mapping_func func)
            : m_Patcher((very_gnawty*)patcher),
              m_MapFunc(std::move(static_cast<very_gnawty*>(patcher)->map_func()))
        {
            m_Patcher->map_func() = [this, newMapFunc = std::move(func)]
                (auto&&... args) {
                    plKey result = newMapFunc(std::forward<decltype(args)>(args)...);
                    if (!result.Exists() && m_MapFunc)
                        result = m_MapFunc(std::forward<decltype(args)>(args)...);
                    return result;
            };
        }
        override_map_func(const override_map_func&) = delete;
        override_map_func(override_map_func&&) = delete;

        ~override_map_func()
        {
            m_Patcher->set_map_func(std::move(m_MapFunc));
        }
    };

    ST::string ST_regex_replace(const std::regex& regex, const ST::string& str, const char* replace) {
        class ST_ss_push_back : public ST::string_stream
        {
        public:
            using value_type = char;
            void push_back(value_type ch) { append_char(ch); }
        } ss;
        std::regex_replace(
            std::back_inserter(ss),
            str.begin(),
            str.end(),
            regex,
            replace
        );
        return ss.to_string();
    };
};

// ===========================================================================

void gpp::patcher::process_collision()
{
    plDebug::Debug("Processing colliders...");

    override_map_func keyHelper(
        this,
        [this](const plKey& srcKey, const std::vector<plKey>& keys) {
            // This is a common ZLZ replacement for us to check before prompting.
            return find_named_key(
                srcKey->getLocation(),
                srcKey->getType(),
                srcKey->getName(),
                "_COLLISION_001"_st,
                keys
            );
        }
    );

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
            plDebug::Debug("  -> Patching '{}' collision", dstSO->getKey()->getName());
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
            plDebug::Debug("  -> Adding collision to '{}' - be certain you are not spin washing colliders!",
                dstSO->getKey()->getName());

            auto simIface = plSimulationInterface::Convert(srcSO->getSimInterface()->getObj());
            auto phys = plGenericPhysical::Convert(simIface->getPhysical()->getObj());

            // Update all refs just to make sure...
            m_Destination->MoveKey(simIface->getKey(), dstSO->getKey()->getLocation());
            m_Destination->MoveKey(phys->getKey(), dstSO->getKey()->getLocation());
            simIface->setOwner(dstSO->getKey());
            simIface->setPhysical(phys->getKey());
            phys->setObject(dstSO->getKey());
            phys->setSceneNode(m_Destination->getSceneNode(dstSO->getKey()->getLocation())->getKey());
            if (phys->getSubWorld().Exists())
                phys->setSubWorld(find_homologous_key(phys->getSubWorld()));
            if (phys->getSoundGroup().Exists())
                phys->setSoundGroup(find_homologous_key(phys->getSoundGroup()));
            dstSO->setSimInterface(simIface->getKey());
        }

        m_DirtyPages.insert(dstSO->getKey()->getLocation());
        return true;
    }
    );
}

void gpp::patcher::process_drawables()
{
    plDebug::Debug("Processing drawables...");

    std::regex materialRegex(R"(^(?:m_)(\d+)(.+)?$)");
    std::regex lightmapRegex("_LM$");

    override_map_func keyHelper(
        this,
        [&](const plKey& srcKey, const std::vector<plKey>& keys) -> plKey {
            plKey result;
            if (srcKey->getType() == kSceneObject || srcKey->getType() == kDrawInterface) {
                result = find_named_key(
                    srcKey->getLocation(),
                    srcKey->getType(),
                    srcKey->getName(),
                    "_DRAW_001"_st,
                    keys
                );
                if (result.Exists())
                    return result;
            }

            // ZLZ does these translations unambiguously, so they should be safe.
            const auto& ogname = srcKey->getName();
            auto rename = ST_regex_replace(materialRegex, ogname, "Material #$01$02");
            result = find_named_key(
                srcKey->getLocation(),
                srcKey->getType(),
                rename,
                keys
            );
            if (result.Exists())
                return result;

            rename = ST_regex_replace(lightmapRegex, rename, "_LIGHTMAPGEN");
            result = find_named_key(
                srcKey->getLocation(),
                srcKey->getType(),
                rename,
                keys
            );
            if (result.Exists())
                return result;

            // There are other possibilities for replacement checking, but those might be
            // fairly difficult to handle. For now, bail out and wait for users to complain
            // about patterns that we could match.
            return result;
        }
    );

    span_hacker geom(m_Source, m_Destination);
    geom.set_map_func(
        [this](const plKey& obj) -> plKey {
            return find_homologous_key(obj);
        }
    );

    iterate_objects<plSceneObject>(
        [this, &geom](const plSceneObject* srcSO, plSceneObject* dstSO) -> bool {
            if (!srcSO->getDrawInterface().Exists() || !dstSO->getDrawInterface().Exists())
                return true;

            plDebug::Debug("  -> Patching '{}' drawable", dstSO->getKey()->getName());
            geom.overwrite_spans(srcSO->getDrawInterface(), dstSO->getDrawInterface());

            m_DirtyPages.insert(dstSO->getKey()->getLocation());
            return true;
        }
    );
}
