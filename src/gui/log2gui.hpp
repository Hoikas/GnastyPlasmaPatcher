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

#ifndef _GPP_LOG2GUI_H
#define _GPP_LOG2GUI_H

#include <memory>
#include <QObject>

class QTextEdit;

namespace gpp
{
    class log2gui : public QObject
    {
        Q_OBJECT

    signals:
        void append(const QString&);

    protected:
        log2gui() = default;
        log2gui(const log2gui&) = delete;
        log2gui(log2gui&&) = delete;

    public:
        virtual ~log2gui() = default;

    public:
        static std::unique_ptr<log2gui> create();
    };
};

#endif
