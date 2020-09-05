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

#include "log2gui.hpp"
#include "errors.hpp"

#include <Debug/plDebug.h>
#include <Stream/hsStream.h>

#include <QObject>
#include <QTextEdit>

// ===========================================================================

namespace gpp
{
    class log2gui_stream : public log2gui, public hsStream
    {
        ST::string_stream m_Stream;

    public:
        log2gui_stream()
            : log2gui()
        {
            plDebug::Init(plDebug::kDLAll, this);
        }

        log2gui_stream(const log2gui_stream&) = delete;
        log2gui_stream(log2gui_stream&&) = delete;

        ~log2gui_stream()
        {
            plDebug::Init(plDebug::kDLNone);
        }

        [[noreturn]]
        uint32_t size() const override { error::raise("log2gui_stream::size()"); }

        [[noreturn]]
        uint32_t pos() const override { error::raise("log2gui_stream::position()"); }

        [[noreturn]]
        bool eof() const override { error::raise("log2gui_stream::eof()"); }

        [[noreturn]]
        void seek(uint32_t pos) override { error::raise("log2gui_stream::seek()"); }

        [[noreturn]]
        void skip(int32_t count) override { error::raise("log2gui_stream::skip()"); }

        [[noreturn]]
        void fastForward() override { error::raise("log2gui_stream::fastForward()"); }

        [[noreturn]]
        void rewind() override { error::raise("log2gui_stream::rewind()"); }

        void flush() override
        {
            // ensure no trailing newline
            bool nl = m_Stream.raw_buffer()[m_Stream.size() - 1] == '\n';

            // assumes blocking connection...
            QString str = QString::fromUtf8(m_Stream.raw_buffer(), m_Stream.size());
                                            //nl ? m_Stream.size() - 1 : m_Stream.size());
            m_Stream.truncate();
            emit append(str);
        }

        [[noreturn]]
        size_t read(size_t size, void* buf) override { error::raise("log2gui_stream::read()"); }

        size_t write(size_t size, const void* buf) override
        {
            m_Stream.append(reinterpret_cast<const char*>(buf), size);
            return size;
        }
    };
};

// ===========================================================================

std::unique_ptr<gpp::log2gui> gpp::log2gui::create()
{
    return std::make_unique<gpp::log2gui_stream>();
}
