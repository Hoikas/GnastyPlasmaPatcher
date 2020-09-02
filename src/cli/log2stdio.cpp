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

#include "log2stdio.hpp"
#include "errors.hpp"

#include <iostream>
#include <memory>

#include <Debug/plDebug.h>
#include <Stream/hsStream.h>

// ===========================================================================

namespace gpp
{
    class log2stdio_stream : public hsStream
    {
    public:
        log2stdio_stream()
        {
            plDebug::Init(plDebug::kDLAll, this);
        }

        log2stdio_stream(const log2stdio_stream&) = delete;
        log2stdio_stream(log2stdio_stream&&) = delete;

        ~log2stdio_stream()
        {
            plDebug::Init(plDebug::kDLNone);
        }

        [[noreturn]]
        uint32_t size() const override { error::raise("log2stdio_stream::size()"); }

        uint32_t pos() const override { return std::cout.tellp(); }
        bool eof() const override { return std::cout.eof(); }
        void seek(uint32_t pos) override { std::cout.seekp(pos); }
        void skip(int32_t count) override { std::cout.seekp(std::cout.tellp() + (std::streampos)count); }

        [[noreturn]]
        void fastForward() override { error::raise("log2stdio_stream::fastForward()"); }

        void rewind() override { std::cout.seekp(0); }
        void flush() override { std::cout.flush(); }

        [[noreturn]]
        size_t read(size_t size, void* buf) override { error::raise("log2stdio_stream::read()"); }

        size_t write(size_t size, const void* buf) override
        {
            std::streampos pos = std::cout.tellp();
            std::cout.write(reinterpret_cast<const char*>(buf), size);
            return std::cout.tellp() - pos;
        }

    };
};

// ===========================================================================

static std::unique_ptr<gpp::log2stdio_stream> s_StdioStream;

void gpp::log::init_stdio()
{
    s_StdioStream = std::make_unique<decltype(s_StdioStream)::element_type>();
}
