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

#ifndef _GPP_PATCHER_H
#define _GPP_PATCHER_H

#include <string_theory/string>

#include <ResManager/plResManager.h>

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <tuple>

class plAgeInfo;
class plGenericPhysical;
class plPageInfo;

namespace gpp
{
    using object_mapping_func = std::function<plKey(const plKey&, const std::vector<plKey>&)>;

    class patcher_base
    {
    protected:
        std::shared_ptr<plResManager> m_Destination;
        std::shared_ptr<plResManager> m_Source;
        std::set<plLocation> m_DirtyPages;

    protected:
        patcher_base() = default;
        patcher_base(const patcher_base&) = delete;
        patcher_base(patcher_base&&) = delete;
        ~patcher_base() = default;

    protected:
        std::shared_ptr<plResManager> load(const std::filesystem::path& file) const;
        void sanity_check_paths(const std::filesystem::path& source, const std::filesystem::path& dest) const;

    private:
        void save_age(const std::filesystem::path& agePath) const;
        void save_page(const plLocation& loc, const std::filesystem::path& pagePath) const;
        void save_page(plPageInfo* page, const std::filesystem::path& pagePath) const;

    public:
        void save_damage(const std::filesystem::path& source, const std::filesystem::path& dest) const;
    };

    /**
     * Interactively patches objects from one registry to another.
     */
    class patcher : public patcher_base
    {
    protected:
        std::map<plKey, plKey> m_KeyLUT;
        object_mapping_func m_MapFunc;

    public:
        patcher() = delete;
        patcher(const std::filesystem::path& source, const std::filesystem::path& dest);
        ~patcher() = default;

        void sanity_check_registry() const;

    private:
        template<typename T>
        [[nodiscard]]
        uint16_t hack_determine_classID() const
        {
            // I hate this.
            return std::make_unique<T>()->ClassIndex();
        }

        [[nodiscard]]
        plKey find_named_key(const plLocation& loc, uint16_t classType, const ST::string& name,
                             const std::vector<plKey>& haystack) const;

        [[nodiscard]]
        plKey find_named_key(const plLocation& loc, uint16_t classType, const ST::string& name,
                             const ST::string& suffix, const std::vector<plKey>& haystack) const;

        [[nodiscard]]
        plKey find_homologous_key(const plKey& needle,
                                  const std::function<bool(const plKey&, const plKey&)> func = {});

        [[nodiscard]]
        plKey find_homologous_key(const plKey& needle, const std::vector<plKey>& haystack);

        [[nodiscard]]
        plKey map_homologous_key(const plKey& needle, const std::vector<plKey>& haystack) const;

        void iterate_keys(uint16_t classType, const std::function<bool(const plKey&, const plKey&)> iter);

        template<typename T>
        void iterate_objects(const std::function<bool(const T*, T*)> iter)
        {
            iterate_keys(hack_determine_classID<T>(),
                [&iter](const plKey& src, const plKey& dst) {
                return iter(T::Convert(src->getObj()), T::Convert(dst->getObj()));
            });
        }

    public:
        void set_map_func(object_mapping_func func) { m_MapFunc = std::move(func); }

        void process_collision();
        void process_drawables();
    };

    /**
     * Merges the contents of one registry into another.
     */
    class merger : public patcher_base
    {
        plLocation m_SourcePage;
        plLocation m_DestinationPage;
        std::filesystem::path m_ParentPath;

    private:
        plLocation load_location(const std::filesystem::path& path, plResManager* mgr) const;
        void load_age(const std::filesystem::path& wd, plResManager* mgr) const;

        void merge_textures();

    public:
        merger() = delete;
        merger(const std::filesystem::path& source, const std::filesystem::path& dest);
        ~merger() = default;

        void process();
    };
};

#endif
