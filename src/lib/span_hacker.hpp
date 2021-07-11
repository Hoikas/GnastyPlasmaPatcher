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

#ifndef _GPP_SPAN_HACKER_H
#define _GPP_SPAN_HACKER_H

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <tuple>
#include <vector>

#include <ResManager/plResManager.h>

class plDISpanIndex;
class plDrawInterface;
class plDrawableSpans;

namespace gpp
{
    using span_key_map_func = std::function<plKey(const plKey&)>;

    enum class render_pass
    {
        opaque = 0,
        framebuf = 1,
        default = 2,
        blend = 3,
        late = 4,
    };

    class span_hacker
    {
        std::set<plDrawableSpans*> m_DirtySpans;
        std::map<std::tuple<plDrawableSpans*, size_t, plDrawableSpans*>, size_t> m_PatchedKeys;
        std::shared_ptr<plResManager> m_Source;
        std::shared_ptr<plResManager> m_Destination;
        span_key_map_func m_MapFunc;

    public:
        using pass_iter = std::function<void(const plKey&, render_pass, size_t, const std::vector<plKey>&)>;

    public:
        span_hacker() = delete;
        span_hacker(const span_hacker&) = delete;
        span_hacker(span_hacker&&) = default;

        /** Single res manager ctor, if you are just working on one data set. */
        span_hacker(const std::shared_ptr<plResManager>& mgr)
            : m_Source(mgr), m_Destination(mgr)
        { }

        /** Split res manager ctor, if you are merging from parallel data sets. */
        span_hacker(std::shared_ptr<plResManager> source, std::shared_ptr<plResManager> destination)
            : m_Source(std::move(source)), m_Destination(std::move(destination))
        { }

        ~span_hacker();

    public:
        void set_map_func(span_key_map_func func) { m_MapFunc = std::move(func); }

    public:
        /** Iterates through all render passes on an object. */
        bool iterate_passes(const pass_iter& func, const plKey& obj) const;

        /** Changes the render pass of a specific material. */
        bool change_pass(const plKey& obj, size_t idx, render_pass new_pass, size_t minor=0);

    public:
        /** Moves an object's drawable data from its current location to the specified page. */
        bool change_page(const plKey& obj, const plLocation& page);

        bool overwrite_spans(const plKey& srcObj, const plKey& dstObj);

    private:
        void change_span(plDrawInterface* obj, size_t idx, plDrawableSpans* dstDSpan);

        /**
         * Imports a set of spans from one DrawableSpan to another.
         * \returns The DISpanIndex of the imported data.
         */
        size_t import_span(plDrawableSpans* srcDSpan, size_t srcDII, plDrawableSpans* dstDSpan);

    private:
        void copy_transforms(const plDISpanIndex& srcDIIndices, plDrawableSpans* srcDSpan,
                             plDISpanIndex& dstDIIndices, plDrawableSpans* dstDSpan);
        void copy_geometry(const plDISpanIndex& srcDIIndices, plDrawableSpans* srcDSpan,
                           plDISpanIndex& dstDIIndices, plDrawableSpans* dstDSpan);

    public:
        static std::tuple<render_pass, size_t> translate_render_pass(const plDrawableSpans* dspan);

        static size_t translate_render_pass(gpp::render_pass major, size_t minor=0);

    private:
        void cleanup_dirty_spans(std::optional<std::vector<plDrawableSpans*>> dspans = std::nullopt);
        void pack_span(plDrawableSpans* dspan);
        void unpack_span(plDrawableSpans* dspan);
        void purge_empty_drawables(plResManager* mgr) const;

    private:
        plKey map_key(const plKey& obj) const
        {
            if (obj.Exists() && m_MapFunc)
                return m_MapFunc(obj);
            return obj;
        }
    };
};

#endif
