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

#include <algorithm>

#include <Debug/plDebug.h>
#include <PRP/Object/plSceneObject.h>
#include <PRP/Physics/plGenericPhysical.h>
#include <ResManager/plFactory.h>
#include <ResManager/plResManager.h>

// ===========================================================================

gpp::patcher::patcher(const std::filesystem::path& source, const std::filesystem::path& dest)
{
    sanity_check_paths(source, dest);
    load(source, m_Source);
    load(dest, m_Destination);
    sanity_check_registry();
}

gpp::patcher::~patcher() { }

void gpp::patcher::load(const std::filesystem::path& file, std::unique_ptr<plResManager>& mgr)
{
    mgr = std::make_unique<plResManager>();
    ST::string stupidPath = ST::string::from_path(file);
    ST::string stupidExt = ST::string::from_path(file.extension());

    plDebug::Debug("Loading registry: {}", file);
    if (stupidExt.compare_i(".age") == 0) {
        plDebug::Debug("  -> Loading AGE");
        // not a memory leak...
        mgr->ReadAge(stupidPath, true);
    } else if (stupidExt.compare_i(".prp") == 0) {
        plDebug::Debug("  -> Loading PRP");
        // not a memory leak...
        mgr->ReadPage(stupidPath, false);
    }
}

// ===========================================================================

void gpp::patcher::sanity_check_paths(const std::filesystem::path& source, const std::filesystem::path& dest) const
{
    if (source.empty())
        error::raise("Source filename is empty.");
    if (dest.empty())
        error::raise("Destination filename is empty.");

    if (!std::filesystem::is_regular_file(source))
        error::raise("Source file '{}' is not a valid regular file.", source);
    if (!std::filesystem::is_regular_file(dest))
        error::raise("Destination file '{}' is not a valid regular file.", dest);
    if (source.filename() != dest.filename())
        error::raise("The source and destination files names must match exactly.");
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

    plDebug::Debug("... merge environment is grinning and holding a spatula.");
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
        if (func && func(needle, suggestion)) {
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
            plKey keyThatTheIteratorDealtWithSoDontWarn = find_homologous_key(i, iter);
        }
    }
}

// ===========================================================================

void gpp::patcher::save_damage(const std::filesystem::path& source, const std::filesystem::path& dest) const
{
    plDebug::Debug("Saving damage...");
    if (m_DirtyPages.empty())
        error::raise("No damage is available to save.");
    sanity_check_paths(source, dest);

    ST::string stupidExt = ST::string::from_path(source.extension());
    if (stupidExt.compare_i(".age") == 0) {
        save_age(dest);
    } else if (stupidExt.compare_i(".prp") == 0) {
        if (m_DirtyPages.size() != 1)
            error::raise("WTF: Multiple dirty pages?!");
        save_page(*(m_DirtyPages.begin()), dest);
    }
}

void gpp::patcher::save_age(const std::filesystem::path& agePath) const
{
    // Only save the specific pages that have been damaged to prevent large deltas.
    for (const auto& i : m_DirtyPages) {
        plPageInfo* page = m_Destination->FindPage(i);
        if (!page)
            error::raise("WTF: Could not find page '{}' in registry!", i.toString());

        ST::string pageFn = page->getFilename(m_Destination->getVer());
        std::filesystem::path pagePath = agePath;
        pagePath.replace_filename(pageFn.to_path());
        save_page(page, pagePath);
    }
}

void gpp::patcher::save_page(const plLocation& loc, const std::filesystem::path& pagePath) const
{
    plPageInfo* page = m_Destination->FindPage(loc);
    if (!page)
        error::raise("WTF: Could not find page '{}' in registry!", loc.toString());
    save_page(page, pagePath);
}

void gpp::patcher::save_page(plPageInfo* page, const std::filesystem::path& pagePath) const
{
    if (!std::filesystem::is_regular_file(pagePath))
        plDebug::Error("WARNING: Saving a brand new '{}_{}' page to '{}' -- is this intended?",
                       page->getAge(), page->getPage(), pagePath);
    else
        plDebug::Debug("Saving '{}_{}' to '{}'...", page->getAge(), page->getPage(), pagePath);
    m_Destination->WritePage(pagePath, page);
}
