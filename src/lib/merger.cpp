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

#include <type_traits>

#include <Debug/plDebug.h>
#include <PRP/plSceneNode.h>
#include <PRP/Audio/plAudible.h>
#include <PRP/Geometry/plClusterGroup.h>
#include <PRP/Geometry/plDrawableSpans.h>
#include <PRP/Geometry/plOccluder.h>
#include <PRP/GUI/pfGUIDialogMod.h>
#include <PRP/Light/plLightInfo.h>
#include <PRP/Object/plSceneObject.h>
#include <PRP/Physics/plGenericPhysical.h>
#include <ResManager/plResManager.h>

// ===========================================================================

gpp::merger::merger(const std::filesystem::path& source, const std::filesystem::path& dest)
{
    sanity_check_paths(source, dest);
    // For clarification, see process() about why the res managers are identical.
    m_Source = m_Destination = std::make_shared<plResManager>();
    m_SourcePage = load_location(source, m_Source.get());
    m_DestinationPage = load_location(dest, m_Destination.get());
    if (m_SourcePage.getSeqPrefix() != m_DestinationPage.getSeqPrefix())
        gpp::error::raise("The source and destination pages must belong to the same Age");
    m_ParentPath = source.parent_path();
}

plLocation gpp::merger::load_location(const std::filesystem::path& file,
                                      plResManager* mgr) const
{
    ST::string ext = file.extension();
    if (ext.compare_i(".prp") != 0)
        gpp::error::raise("Only individual PRP files may be merged.");

    plPageInfo* page;

    plDebug::Debug("  -> Loading PRP");
    try {
        page = mgr->ReadPage(file);
    } catch (const hsException& ex) {
        gpp::error::raise("Unable to read page {}: {}", file, ex.what());
    }

    return page->getLocation();
}

void gpp::merger::load_age(const std::filesystem::path& wd, plResManager* mgr) const
{
    ST::string ageName = ST::format("{}.age", mgr->FindPage(m_DestinationPage)->getAge());
    std::filesystem::path ageFile = wd / ageName.to_path();
    if (!std::filesystem::is_regular_file(ageFile)) {
        plDebug::Warning(" -> Hmm... We wanted the whole Age, but it can't be loaded.");
        return;
    }

    plDebug::Debug("  -> Ah, whole Age is available. Load it as well!");

    plAgeInfo age;
    age.readFromFile(ageFile);
    for (size_t i = 0; i < age.getNumPages(); ++i) {
        plLocation loc = age.getPageLoc(i, mgr->getVer());
        if (loc == m_DestinationPage || loc == m_SourcePage)
            continue;
        std::filesystem::path prp = wd / age.getPageFilename(i, mgr->getVer()).to_path();
        if (std::filesystem::is_regular_file(prp))
            load_location(prp, mgr);
    }
    for (size_t i = 0; i < age.getNumCommonPages(mgr->getVer()); ++i) {
        plLocation loc = age.getCommonPageLoc(i, mgr->getVer());
        if (loc == m_DestinationPage || loc == m_SourcePage)
            continue;
        std::filesystem::path prp = wd / age.getCommonPageFilename(i, mgr->getVer()).to_path();
        if (std::filesystem::is_regular_file(prp))
            load_location(prp, mgr);
    }
}

// ===========================================================================

template<typename T, typename Pr>
std::enable_if_t<std::is_base_of_v<hsKeyedObject, T>>
visit_object(const plKey& key, Pr func)
{
    T* obj = T::Convert(key->getObj(), false);
    if (obj) {
        func(obj);
    }
}

void gpp::merger::process()
{
    // Step 1: Merge geometry data into new file because DSpans are monoliths.
    {
        span_hacker geom(m_Source, m_Destination);
        for (const auto& diKey : m_Source->getKeys(m_SourcePage, kDrawInterface))
            geom.change_page(diKey, m_DestinationPage);
    }

    // Step 2: Delete the old drawable spans objects so they will not carry over
    //         into the new PRP file.
    for (const auto& dsKey : m_Source->getKeys(m_SourcePage, kDrawableSpans))
        m_Source->DelObject(dsKey);

    // Step 3: Many types contain back-references to the SceneNode. This is replaced,
    //         so we must manually fix that up. But, instead of hardcoding everything...
    plSceneNode* destNode = m_Destination->getSceneNode(m_DestinationPage);
    for (auto type : m_Source->getTypes(m_SourcePage)) {
        for (const auto& key : m_Source->getKeys(m_SourcePage, type)) {
            visit_object<plWinAudible>(key, [&](auto* obj) { obj->setSceneNode(destNode->getKey()); });
            visit_object<plClusterGroup>(key, [&](auto* obj) { obj->setSceneNode(destNode->getKey()); });
            visit_object<plDrawableSpans>(key, [&](auto* obj) { obj->setSceneNode(destNode->getKey()); });
            visit_object<plOccluder>(key, [&](auto* obj) { obj->setSceneNode(destNode->getKey()); });
            visit_object<pfGUIDialogMod>(key, [&](auto* obj) { obj->setSceneNode(destNode->getKey()); });
            visit_object<plLightInfo>(key, [&](auto* obj) { obj->setSceneNode(destNode->getKey()); });
            visit_object<plSceneObject>(key, [&](auto* obj) { obj->setSceneNode(destNode->getKey()); });
            visit_object<plGenericPhysical>(key, [&](auto* obj) { obj->setSceneNode(destNode->getKey()); });
        }
    }

    // Step 4: Merge the SceneNodes and delete the old one.
    {
        plSceneNode* srcNode = m_Source->getSceneNode(m_SourcePage);
        destNode->getSceneObjects().insert(
            destNode->getSceneObjects().cend(),
            srcNode->getSceneObjects().begin(),
            srcNode->getSceneObjects().end()
        );
        destNode->getPoolObjects().insert(
            destNode->getPoolObjects().cend(),
            srcNode->getPoolObjects().begin(),
            srcNode->getPoolObjects().end()
        );
        m_Source->DelObject(srcNode->getKey());
    }

    // Step 5: If there are any per-page Textures, resolve them into the Textures.prp
    if (m_DestinationPage.getSeqPrefix() > 0)
        merge_textures();

    // Safety: don't touch the source page. ChangeLocation() will definitely try
    // to do this oh so very "helpfully"
    m_Source->DelPage(m_SourcePage);
    auto sourceIt = m_DirtyPages.find(m_SourcePage);
    if (sourceIt != m_DirtyPages.end())
        m_DirtyPages.erase(sourceIt);

    // Mass move all remaining keys into the destination page, all should be OK.
    // Limitation: you cannot move objects from one res manager to another,
    // because HSPlasma will just throw an exception at you when you try
    // to reset the object contained by a key. So, just change the location.
    // Sad. I am sad.
    m_Source->ChangeLocation(m_SourcePage, m_DestinationPage);

    m_DirtyPages.insert(m_DestinationPage);
}

void gpp::merger::merge_textures()
{
    constexpr uint16_t kTextureTypes[] = { kMipmap, kCubicEnvironmap };

    std::vector<plKey> textureKeys;
    for (auto type : kTextureTypes) {
        auto keys = m_Source->getKeys(m_SourcePage, type);
        textureKeys.insert(textureKeys.end(), keys.cbegin(), keys.cend());
    }

    if (textureKeys.empty())
        return;

    // Try to load the whole Age so we can fool with the Textures PRP.
    plDebug::Debug("  -> Hmm... Textures... Let's try merging those!");
    load_age(m_ParentPath, m_Destination.get());

    plLocation texturesPage = m_DestinationPage;
    texturesPage.setFlags(plLocation::kBuiltIn);
    texturesPage.setPageNum(-1);
    if (!m_Destination->FindPage(texturesPage)) {
        plDebug::Warning("  -> No textures page, not merging textures.");
        return;
    }

    for (const auto& key : textureKeys) {
        m_Source->MoveKey(key, texturesPage);
        m_DirtyPages.insert(texturesPage);
    }
}
