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

#include "span_hacker.hpp"
#include "errors.hpp"

#include <PRP/Geometry/plDrawableSpans.h>
#include <PRP/Geometry/plIcicle.h>
#include <PRP/KeyedObject/plKey.h>
#include <PRP/Object/plDrawInterface.h>
#include <PRP/Object/plSceneObject.h>
#include <PRP/plSceneNode.h>
#include <ResManager/plResManager.h>

// ===========================================================================

gpp::span_hacker::~span_hacker()
{
    cleanup_dirty_spans();
    purge_empty_drawables(m_Destination.get());
}

// ===========================================================================

namespace
{
    using ssize_t = std::make_signed_t<size_t>;
    using drawables_t = std::vector<std::tuple<plDrawableSpans*, size_t>>;
    using interfaces_t = std::map<plDrawInterface*, std::vector<size_t>>;

    /** Naughty touching happens here. */
    class naughty_draw_interface : public plDrawInterface
    {
    public:
        decltype(fDrawables)& get_drawables() { return fDrawables; }
        decltype(fDrawableKeys)& get_diis() { return fDrawableKeys; }
    };

    static inline bool compare_passes(const plDrawableSpans* testDSpan,
        gpp::render_pass other_pass,
        size_t other_minor)
    {
        auto [old_pass, old_minor] = gpp::span_hacker::translate_render_pass(testDSpan);
        return (other_pass == old_pass && other_minor == old_minor);
    }

    static inline plDrawInterface* find_diface(const plKey& obj)
    {
        if (!obj.Exists())
            gpp::error::raise("a null key?");

        plDrawInterface* diface = nullptr;
        switch (obj->getType()) {
        case kSceneObject:
        {
            plSceneObject* so = plSceneObject::Convert(obj->getObj());
            if (so->getDrawInterface().Exists())
                diface = plDrawInterface::Convert(so->getDrawInterface()->getObj());
        }
        break;
        case kDrawInterface:
            diface = plDrawInterface::Convert(obj->getObj());
            break;
        default:
            gpp::error::raise("Cannot iterate passes for {}", obj.toString());
            break;
        }
        return diface;
    }

    [[nodiscard]]
    static inline plDrawableSpans* create_dspan(plResManager* mgr, const plLocation& loc,
        gpp::render_pass new_pass, size_t minor,
        size_t criteria, size_t props)
    {
        plPageInfo* page = mgr->FindPage(loc);
        const char* span_type = (new_pass == gpp::render_pass::e_opaque ? "Spans" : "BlendSpans");
        size_t renderLevel = gpp::span_hacker::translate_render_pass(new_pass, minor);
        ST::string name = ST::format(
            "{}_{}_{08X}_{X}{}",
            page->getAge(),
            page->getPage(),
            renderLevel,
            criteria,
            span_type
        );
        plDrawableSpans* dstDSpan = new plDrawableSpans();
        dstDSpan->init(name);
        dstDSpan->setCriteria(criteria);
        dstDSpan->setProps(props);
        dstDSpan->setRenderLevel(renderLevel);
        dstDSpan->setSceneNode(mgr->getSceneNode(loc)->getKey());
        mgr->AddObject(loc, dstDSpan);

        plDebug::Debug(
            "  -> Behold! A new DrawableSpan: '{}'@{X}",
            dstDSpan->getKey().toString(),
            (uintptr_t)dstDSpan
        );

        return dstDSpan;
    }

    [[nodiscard]]
    static inline plDrawableSpans* find_matching_dspan(plResManager* mgr, const plLocation& loc,
        gpp::render_pass new_pass, size_t minor,
        size_t criteria, size_t props)
    {
        // Try to find a matching DrawableSpan for us to merge into.
        auto drawKeys = mgr->getKeys(loc, kDrawableSpans);
        auto it = std::find_if(drawKeys.begin(), drawKeys.end(),
            [&](const plKey& test) {
            const auto* maybeDSpan = plDrawableSpans::Convert(test->getObj());
            if (compare_passes(maybeDSpan, new_pass, minor)) {
                if (maybeDSpan->getCriteria() == criteria) {
                    if (maybeDSpan->getProps() == props)
                        return true;
                }
            }
            return false;
        }
        );

        if (it != drawKeys.end())
            return plDrawableSpans::Convert((*it)->getObj());
        return nullptr;
    }

    [[nodiscard]]
    static inline plDrawableSpans* find_or_create_dspan(plResManager* mgr, const plLocation& loc,
        gpp::render_pass new_pass, size_t minor,
        size_t criteria, size_t props)
    {
        auto* dspan = find_matching_dspan(mgr, loc, new_pass, minor, criteria, props);
        if (!dspan)
            dspan = create_dspan(mgr, loc, new_pass, minor, criteria, props);
        return dspan;
    }

    /** Returns a COPY of the drawables in a DI. */
    [[nodiscard]]
    static inline drawables_t
    get_drawables(const plDrawInterface* diface)
    {
        drawables_t ret(diface->getNumDrawables());
        for (size_t i = 0; i < diface->getNumDrawables(); ++i) {
            ret[i] = std::make_tuple(
                plDrawableSpans::Convert(diface->getDrawable(i)->getObj()),
                diface->getDrawableKey(i)
            );
        }
        return ret;
    }

    /**
     * SAFELY get all the interfaces that use this drawable.
     * Use this when you don't know which ResManager it came from!
     * \returns A mapping of DrawInterfaces to drawIdxes that use this DSpan.
     */
    [[nodiscard]]
    static inline interfaces_t
    get_interfaces(const plDrawableSpans* dspan)
    {
        std::map<plDrawInterface*, std::vector<size_t>> ret;
        plSceneNode* node = plSceneNode::Convert(dspan->getSceneNode()->getObj());
        for (const auto& soKey : node->getSceneObjects()) {
            plDrawInterface* dIface = find_diface(soKey);
            if (!dIface)
                continue;

            for (size_t i = 0; i < dIface->getNumDrawables(); ++i) {
                if (dIface->getDrawable(i)->getObj() == dspan) {
                    ret[dIface].push_back(i);
                }
            }
        }
        return ret;
    }
};

// ===========================================================================

bool gpp::span_hacker::iterate_passes(const gpp::span_hacker::pass_iter& func, const plKey& obj) const
{
    plDrawInterface* diface = find_diface(obj);
    if (diface) {
        for (size_t i = 0; i < diface->getNumDrawables(); ++i) {
            plDrawableSpans* dspan = plDrawableSpans::Convert(diface->getDrawable(i)->getObj());
            const auto& diindex = dspan->getDIIndex(diface->getDrawableKey(i));

            std::vector<plKey> materials;
            for (uint32_t spanidx : diindex.fIndices) {
                const plSpan* span = dspan->getSpan(spanidx);
                materials.emplace_back(dspan->getMaterials().at(span->getMaterialIdx()));
            }

            func(obj, render_pass::e_blend, i, materials);
        }
        return true;
    }
    return false;
}

bool gpp::span_hacker::change_pass(const plKey& obj, size_t idx, render_pass new_pass, size_t minor)
{
    plDrawInterface* diface = find_diface(obj);

    plDrawableSpans* sourceDSpan = plDrawableSpans::Convert(diface->getDrawable(idx)->getObj());
    if (compare_passes(sourceDSpan, new_pass, minor))
        return false;

    plDrawableSpans* dstDSpan = find_or_create_dspan(
        m_Destination.get(),
        obj->getLocation(),
        new_pass, minor, sourceDSpan->getCriteria(),
        sourceDSpan->getProps()
    );

    change_span(diface, idx, dstDSpan);
    return true;
}

// ===========================================================================

bool gpp::span_hacker::change_page(const plKey& obj, const plLocation& page)
{
    plDrawInterface* diface = find_diface(obj);

    auto srcDrawables = get_drawables(diface);
    for (ssize_t i = srcDrawables.size() - 1; i >= 0; --i) {
        auto [srcDSpan, srcDrawKey] = srcDrawables[i];
        auto [pass, minor] = translate_render_pass(srcDSpan);
        plDrawableSpans* dstDSpan = find_or_create_dspan(
            m_Destination.get(),
            page,
            pass,
            minor,
            srcDSpan->getCriteria(),
            srcDSpan->getProps()
        );
        change_span(diface, i, dstDSpan);
    }

    return true;
}

bool gpp::span_hacker::overwrite_spans(const plKey& srcObj, const plKey& dstObj)
{
    plDrawInterface* srcDIface = find_diface(srcObj);
    plDrawInterface* dstDIface = find_diface(dstObj);

    // This might have to be made less naïve if we ever support bone animations.
    // Anyway, just blow up all the drawables currently in the dst DI and dirty them.
    // We'll want to have them marked dirty for the cleanup pass.
    for (ssize_t i = dstDIface->getNumDrawables() - 1; i >= 0; --i) {
        plDrawableSpans* dspan = plDrawableSpans::Convert(dstDIface->getDrawable(i)->getObj());
        //m_DirtySpans.insert(dspan);
        // Dirty also means unpacked, dammit.
        unpack_span(dspan);
        dstDIface->delDrawable(i);
    }

    auto srcDrawables = get_drawables(srcDIface);
    for (size_t i = 0; i < srcDIface->getNumDrawables(); ++i) {
        auto [srcDSpan, srcDrawKey] = srcDrawables[i];
        auto [pass, minor] = translate_render_pass(srcDSpan);
        plDrawableSpans* dstDSpan = find_or_create_dspan(
            m_Destination.get(),
            dstObj->getLocation(),
            pass,
            minor,
            srcDSpan->getCriteria(),
            srcDSpan->getProps()
        );
        size_t dstDII = import_span(srcDSpan, srcDrawKey, dstDSpan);
        dstDIface->addDrawable(dstDSpan->getKey(), dstDII);
    }

    // TODO QUESTION: should we scan for anyone else using these DIIs and
    // fixup those references to use our new spans? Seems useful, but
    // potentially misleading.

    // Cleanup pass will happen when the hacker is destroyed.
    return true;
}

// ===========================================================================

void gpp::span_hacker::change_span(plDrawInterface* diface, size_t idx, plDrawableSpans* dstDSpan)
{
    plDrawableSpans* srcDSpan = plDrawableSpans::Convert(diface->getDrawable(idx)->getObj());
    size_t srcDII = diface->getDrawableKey(idx);

    // Maybe this was already done by an optimization pass?
    if (srcDSpan == dstDSpan) {
        plDebug::Debug(
            "  -> Move request for '{}' [DII: {}] into '{}'@{X} already done?",
            diface->getKey().toString(),
            srcDII,
            dstDSpan->getKey().toString(),
            (uintptr_t)dstDSpan
        );
        return;
    }

    size_t newDII = import_span(srcDSpan, srcDII, dstDSpan);
    diface->setDrawable(idx, dstDSpan->getKey(), newDII);
}

size_t gpp::span_hacker::import_span(plDrawableSpans* srcDSpan, size_t srcDII,
                                     plDrawableSpans* dstDSpan)
{
    auto doneIt = m_PatchedKeys.find(std::make_tuple(srcDSpan, srcDII, dstDSpan));
    if (doneIt != m_PatchedKeys.end()) {
        plDebug::Debug(
            "  -> Import for '{}'@{X} [DII: {}] into '{}'@{X} already done",
            srcDSpan->getKey().toString(),
            (uintptr_t)srcDSpan,
            srcDII,
            dstDSpan->getKey().toString(),
            (uintptr_t)dstDSpan
        );
        return doneIt->second;
    } else {
        plDebug::Debug(
            "  -> Importing '{}'@{X} [DII: {}] into '{}'@{X}",
            srcDSpan->getKey().toString(),
            (uintptr_t)srcDSpan,
            srcDII,
            dstDSpan->getKey().toString(),
            (uintptr_t)dstDSpan
        );

        const auto& srcDIIndices = srcDSpan->getDIIndices()[srcDII];
        plDISpanIndex dstDIIndices;
        dstDIIndices.fFlags = srcDIIndices.fFlags;

        if (srcDIIndices.fFlags & plDISpanIndex::kMatrixOnly)
            copy_transforms(srcDIIndices, srcDSpan, dstDIIndices, dstDSpan);
        else
            copy_geometry(srcDIIndices, srcDSpan, dstDIIndices, dstDSpan);

        size_t dstDII = dstDSpan->addDIIndex(dstDIIndices);
        m_PatchedKeys[std::make_tuple(srcDSpan, srcDII, dstDSpan)] = dstDII;
        return dstDII;
    }
}

// ===========================================================================

void gpp::span_hacker::copy_transforms(const plDISpanIndex& srcDIIndices, plDrawableSpans* srcDSpan,
                                       plDISpanIndex& dstDIIndices, plDrawableSpans* dstDSpan)
{
    gpp::error::raise("Bone animated spans are not supported. Implement me, pretty please? ^_^");
}

void gpp::span_hacker::copy_geometry(const plDISpanIndex& srcDIIndices, plDrawableSpans* srcDSpan,
                                     plDISpanIndex& dstDIIndices, plDrawableSpans* dstDSpan)
{
    // Deferred alllll the way down here because only geometry needs to be unpacked
    unpack_span(srcDSpan);
    unpack_span(dstDSpan);

    dstDIIndices.fIndices.reserve(srcDIIndices.fIndices.size());
    for (auto srcIndex : srcDIIndices.fIndices) {
        plGeometrySpan* srcGeoSpan = srcDSpan->getSourceSpans()[srcIndex];
        plGeometrySpan* dstGeoSpan = new plGeometrySpan();

        // Copy by hand because the copy ctor is apparently a pile of junk.
        dstGeoSpan->setMaterial(map_key(srcGeoSpan->getMaterial()));
        dstGeoSpan->setFogEnvironment(map_key(srcGeoSpan->getFogEnvironment()));
        dstGeoSpan->setLocalToWorld(srcGeoSpan->getLocalToWorld());
        dstGeoSpan->setWorldToLocal(srcGeoSpan->getWorldToLocal());
        dstGeoSpan->setLocalBounds(srcGeoSpan->getLocalBounds());
        dstGeoSpan->setWorldBounds(srcGeoSpan->getWorldBounds());
        dstGeoSpan->setFormat(srcGeoSpan->getFormat());
        dstGeoSpan->setNumMatrices(srcGeoSpan->getNumMatrices()); // gulp...
        dstGeoSpan->setBaseMatrix(srcGeoSpan->getBaseMatrix()); // gulp...
        dstGeoSpan->setLocalUVWChans(srcGeoSpan->getLocalUVWChans());
        dstGeoSpan->setMaxBoneIdx(srcGeoSpan->getMaxBoneIdx());
        dstGeoSpan->setPenBoneIdx(srcGeoSpan->getPenBoneIdx());
        dstGeoSpan->setMinDist(srcGeoSpan->getMinDist());
        dstGeoSpan->setMaxDist(srcGeoSpan->getMaxDist());
        dstGeoSpan->setWaterHeight(srcGeoSpan->getWaterHeight());
        dstGeoSpan->setProps(srcGeoSpan->getProps());
        dstGeoSpan->setVertices(srcGeoSpan->getVertices());
        dstGeoSpan->setIndices(srcGeoSpan->getIndices());
        // decal level, instance group, l2obb, obb2l... nope

        dstGeoSpan->getPermaLights().reserve(srcGeoSpan->getPermaLights().size());
        for (const auto& light : srcGeoSpan->getPermaLights())
            dstGeoSpan->addPermaLight(map_key(light));
        dstGeoSpan->getPermaProjs().reserve(srcGeoSpan->getPermaProjs().size());
        for (const auto& light : srcGeoSpan->getPermaProjs())
            dstGeoSpan->addPermaProj(map_key(light));

        auto dstIndex = dstDSpan->addSourceSpan(dstGeoSpan);
        dstDIIndices.fIndices.push_back(dstIndex);
    }
}

// ===========================================================================

void gpp::span_hacker::cleanup_dirty_spans(std::optional<std::vector<plDrawableSpans*>> dspans)
{
    if (dspans) {
        for (plDrawableSpans* dspan : dspans.value()) {
            // This is a forced op, who cares if it's actually dirty?
            pack_span(dspan);
            m_DirtySpans.erase(dspan);
        }
    } else {
        for (plDrawableSpans* dspan : m_DirtySpans)
            pack_span(dspan);
        m_DirtySpans.clear();
    }
}

namespace
{
    template<typename _ItT>
    std::vector<size_t> find_unused_indices(_ItT begin, _ItT end)
    {
        std::vector<size_t> unusedIdxes;
        size_t nextIdx = 0;
        for (_ItT it = begin; it != end; ++it) {
            for (size_t i = nextIdx; i < *it; ++i)
                unusedIdxes.push_back(i);
            nextIdx = *it + 1;
        }
        return unusedIdxes;
    }

    static inline void nuke_dii(plDrawableSpans* dspan, size_t dii, interfaces_t& myDIfaces)
    {
        plDebug::Debug(
            "  -> Cleaning up unused DISpan [DII: {}] [BONE: {}]  in '{}'@{X}",
            dii,
            (bool)(dspan->getDIIndex(dii).fFlags & plDISpanIndex::kMatrixOnly),
            dspan->getKey().toString(),
            (uintptr_t)dspan
        );

        // This crap is complicated by the fact that we need to keep the DrawInterface
        // index LUT correct because it will continue to be used. Ugh.
        for (auto& [dIface, myIdxes] : myDIfaces) {
            size_t numDrawDeletions = 0;
            for (auto idxIt = myIdxes.begin(); idxIt != myIdxes.end();) {
                *idxIt -= numDrawDeletions;
                size_t myDII = dIface->getDrawableKey(*idxIt);
                if (myDII < dii) {
                    ++idxIt;
                } else if (myDII == dii) {
                    dIface->delDrawable(*idxIt); // KABLOOEY!
                    myIdxes.erase(idxIt);
                    ++numDrawDeletions;
                } else {
                    dIface->setDrawable(
                        *idxIt,
                        dIface->getDrawable(*idxIt),
                        dIface->getDrawableKey(*idxIt) - 1
                    );
                    ++idxIt;
                }
            }
        }

        dspan->delDIIndex(dii);
    }

    static inline void purge_unused_diis(plDrawableSpans* dspan)
    {
        // Prepare a set of all valid DIIs in this DrawableSpan by brute-forcing
        // backwards through the DrawInterfaces. Then, cleanup by evicting any
        // that were not used.
        auto myDIfaces = get_interfaces(dspan);
        std::set<size_t> usedDIIs;
        for (auto [dIface, myIdxes] : myDIfaces) {
            for (auto i : myIdxes) {
                // No -1 because those are particle systems
                int dii = dIface->getDrawableKey(i);
                if (dii != -1)
                    usedDIIs.insert(i);
            }
        }

        // Look for any gaps in the set prepared above - these are the ones
        // that we will terminate. This could probably be done in one loop,
        // but I'd prefer to logically separate the code until someone can
        // prove it's too slow.
        auto unusedDIIs = find_unused_indices(usedDIIs.cbegin(), usedDIIs.cend());
        for (auto diiIt = unusedDIIs.crbegin(); diiIt != unusedDIIs.crend(); ++diiIt) {
            // Yeah, there are multiple kinds of DISpans, but the actual handling of
            // the transform/geospan reference removal will be handled later.
            nuke_dii(dspan, *diiIt, myDIfaces);
        }
    }

    static inline void nuke_source_span(plDrawableSpans* dspan, size_t idx)
    {
        plDebug::Debug(
            "  -> Cleaning up unused source span [IDX: {}] in '{}'@{X}",
            idx,
            dspan->getKey().toString(),
            (uintptr_t)dspan
        );

        // Adjust all remaining DIIs
        for (auto& diiSpan : dspan->getDIIndices()) {
            if (diiSpan.fFlags & plDISpanIndex::kMatrixOnly)
                continue;
            for (auto& diiSpanGeoIdx : diiSpan.fIndices) {
                if (diiSpanGeoIdx == idx)
                    gpp::error::raise("Hmm... Trying to delete an in-use geometry span, buddy?");
                if (diiSpanGeoIdx > idx)
                    --diiSpanGeoIdx;
            }
        }

        dspan->delSourceSpan(idx);
    }

    static inline void purge_unused_geometry(plDrawableSpans* dspan)
    {
        // Prepare a set of all valid Source Span indices so we can
        // know what to nuke out.
        std::set<size_t> usedSourceSpans;
        for (const auto& diiSpan : dspan->getDIIndices()) {
            if (diiSpan.fFlags & plDISpanIndex::kMatrixOnly)
                continue;
            usedSourceSpans.insert(diiSpan.fIndices.cbegin(), diiSpan.fIndices.cend());
        }

        auto deadSourceSpans = find_unused_indices(usedSourceSpans.cbegin(), usedSourceSpans.cend());
        for (auto idxIt = deadSourceSpans.crbegin(); idxIt != deadSourceSpans.crend(); ++idxIt)
            nuke_source_span(dspan, *idxIt);
    }
}

void gpp::span_hacker::pack_span(plDrawableSpans* dspan)
{
    // Just in case someone is trying to "compress" an otherwise
    // unmodified span.
    unpack_span(dspan);

    plDebug::Debug(
        "  -> Packing DSpan '{}'@{X}",
        dspan->getKey().toString(),
        (uintptr_t)dspan
    );

    purge_unused_diis(dspan);
    purge_unused_geometry(dspan);
    // TODO: bones/transforms... ugh

    dspan->composeGeometry();
}

void gpp::span_hacker::unpack_span(plDrawableSpans* dspan)
{
    if (m_DirtySpans.find(dspan) != m_DirtySpans.end())
        return;

    plDebug::Debug(
        "  -> Unpacking DSpan '{}'@{X}",
        dspan->getKey().toString(),
        (uintptr_t)dspan
    );
    dspan->decomposeGeometry();
    m_DirtySpans.insert(dspan);

    // Forcibly clear the span because we may be in an indeterminant state.
    dspan->clearSpans();
    for (ssize_t i = dspan->getNumBufferGroups() - 1; i >= 0; --i)
        dspan->deleteBufferGroup(i);
}

void gpp::span_hacker::purge_empty_drawables(plResManager* mgr) const
{
    for (const auto& diKey : mgr->getKeys(kDrawInterface)) {
        plDrawInterface* dIface = plDrawInterface::Convert(diKey->getObj());
        if (dIface->getNumDrawables() == 0) {
            plDebug::Debug("  -> Purging empty drawable '{}'", diKey.toString());
            mgr->DelObject(diKey);
        }
    }

    // This is way too simplisitic. I'm not 100% on how all this fits together,
    // but particle systems apparently spawn empty DSpan objects, but this would
    // force them to be deleted. Might not be a good idea. Revisit later.
#if 0
    for (const auto& dsKey : mgr->getKeys(kDrawableSpans)) {
        plDrawableSpans* dspan = plDrawableSpans::Convert(dsKey->getObj());

        // Is this too simplistic? Verify that bone anims are not deleted.
        if (dspan->getNumSpans() == 0) {
            plDebug::Debug("  -> Purging empty drawable '{}'", dsKey.toString());
            mgr->DelObject(dsKey);
        }
    }
#endif
}

// ===========================================================================

std::tuple<gpp::render_pass, size_t> gpp::span_hacker::translate_render_pass(const plDrawableSpans* dspan)
{
    unsigned int level = dspan->getRenderLevel();

    render_pass major = (render_pass)(level >> 28);
    size_t index = level & ((1 << 28) - 1);
    return std::make_tuple(major, index);
}

size_t gpp::span_hacker::translate_render_pass(gpp::render_pass major, size_t minor)
{
    return (((size_t)major << 28) & 0xFFFFFFFF) | minor;
}
