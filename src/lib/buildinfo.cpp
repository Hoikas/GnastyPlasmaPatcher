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

#include "buildinfo.hpp"
#include <string_theory/string_stream>

// =================================================================================

namespace gpp
{
    namespace buildinfo
    {
        extern const char* BUILD_HASH;
        extern const char* BUILD_TAG;
        extern const char* BUILD_BRANCH;
        extern const char* BUILD_DATE;
        extern const char* BUILD_TIME;
    };
};

// =================================================================================

const char* gpp::build_hash()
{
    return gpp::buildinfo::BUILD_HASH;
}

const char* gpp::build_branch()
{
    return gpp::buildinfo::BUILD_BRANCH;
}

const char* gpp::build_tag()
{
    return gpp::buildinfo::BUILD_TAG;
}

const char* gpp::build_version()
{
    if (*gpp::buildinfo::BUILD_TAG == 0)
        return gpp::buildinfo::BUILD_HASH;
    else
        return gpp::buildinfo::BUILD_TAG;
}

// =================================================================================

ST::string gpp::build_info()
{
    ST::string_stream stream;
    stream << "gpp - gnasty plasma patcher";
    if (*gpp::buildinfo::BUILD_TAG != 0)
        stream << " (" << gpp::buildinfo::BUILD_TAG << ")";
    stream << " [" << gpp::buildinfo::BUILD_HASH << " (" << gpp::buildinfo::BUILD_BRANCH << ")] ";
#if defined(__clang__)
    stream << "clang++ " << __clang_version__;
#elif defined(__GNUC__)
    stream << "g++ " << __VERSION__;
#elif defined(_MSC_VER)
    stream << "MSVC " << _MSC_VER;
#endif

    // Don't whine at me about this because I don't care right now.
    if (sizeof(void*) == 4)
        stream << " (x86)";
    else if (sizeof(void*) == 8)
        stream << " (x64)";
    return stream.to_string();
}
