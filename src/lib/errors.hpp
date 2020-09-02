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

#ifndef _GPP_ERRORS_H
#define _GPP_ERRORS_H

#include <exception>
#include <string_theory/format>

namespace gpp
{
    class error : public std::runtime_error
    {
    public:
        error(const char* msg)
              : std::runtime_error(msg)
        { }

        error(const error&) = delete;
        error(error&&) = delete;

    public:
        [[noreturn]]
        static void raise()
        {
            throw error("unspecified gpp error");
        }

        [[noreturn]]
        static void raise(const char* str)
        {
            throw error(str);
        }

        template<typename... _Args>
        [[noreturn]]
        static void raise(const char* fmt, _Args&&... args)
        {
            ST::string msg = ST::format(fmt, std::forward<_Args>(args)...);
            throw error(msg.c_str());
        }
    };
};

#endif
