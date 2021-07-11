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
#include <ResManager/plResManager.h>

// ===========================================================================

std::shared_ptr<plResManager> gpp::patcher_base::load(const std::filesystem::path& file) const
{
    auto mgr = std::make_shared<plResManager>();
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
    } else {
        gpp::error::raise("What the extension: {}???", stupidExt);
    }
    return mgr;
}

// ===========================================================================

void gpp::patcher_base::sanity_check_paths(const std::filesystem::path& source,
                                           const std::filesystem::path& dest) const
{
    if (source.empty())
        error::raise("Source filename is empty.");
    if (dest.empty())
        error::raise("Destination filename is empty.");

    if (!std::filesystem::is_regular_file(source))
        error::raise("Source file '{}' is not a valid regular file.", source);
    if (!std::filesystem::is_regular_file(dest))
        error::raise("Destination file '{}' is not a valid regular file.", dest);
}

// ===========================================================================

void gpp::patcher_base::save_damage(const std::filesystem::path& source, const std::filesystem::path& dest) const
{
    plDebug::Debug("Saving damage...");
    if (m_DirtyPages.empty())
        error::raise("No damage is available to save.");
    sanity_check_paths(source, dest);

    // Ok, so this is great and all, but in Plasma versions with ObjIDs
    // in the keys, as of the writing of this comment, simply reading the
    // page into libHSPlasma may re-order the ObjIDs. Previously, that was
    // not the case, but you ran the risk that the ObjIDs *might* change on
    // write, which was dangeously non-deterministic. So, just write out
    // all the pages we have loaded, m'kay?
#if 0
    ST::string stupidExt = ST::string::from_path(source.extension());
    if (stupidExt.compare_i(".age") == 0) {
        save_age(dest);
    } else if (stupidExt.compare_i(".prp") == 0) {
        // Hmm... the merger can dirty multiple pages from a single prp
        // Better just save the age, I guess.
        //save_page(*(m_DirtyPages.begin()), dest);
        save_age(dest);
    }
#else
    std::filesystem::path wd = dest.parent_path();
    // TODO: maybe allow writing out just the dirty pages for pvPrime, pvPots
    for (const auto& loc : m_Destination->getLocations()) {
        plPageInfo* page = m_Destination->FindPage(loc);
        if (page != nullptr)
            save_page(page, wd / page->getFilename(m_Destination->getVer()).to_path());
    }
#endif
}

void gpp::patcher_base::save_age(const std::filesystem::path& agePath) const
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

void gpp::patcher_base::save_page(const plLocation& loc, const std::filesystem::path& pagePath) const
{
    plPageInfo* page = m_Destination->FindPage(loc);
    if (!page)
        error::raise("WTF: Could not find page '{}' in registry!", loc.toString());
    save_page(page, pagePath);
}

void gpp::patcher_base::save_page(plPageInfo* page, const std::filesystem::path& pagePath) const
{
    if (!std::filesystem::is_regular_file(pagePath))
        plDebug::Error("WARNING: Saving a brand new '{}_{}' page to '{}' -- is this intended?",
            page->getAge(), page->getPage(), pagePath);
    else
        plDebug::Debug("Saving '{}_{}' to '{}'...", page->getAge(), page->getPage(), pagePath);
    m_Destination->WritePage(pagePath, page);
}
