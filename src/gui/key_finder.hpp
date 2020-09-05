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

#ifndef _GPP_GUI_KEY_FINDER_H
#define _GPP_GUI_KEY_FINDER_H

#include <QDialog>

#include <tuple>
#include <vector>

#include <PRP/KeyedObject/plKey.h>

class QCommandLinkButton;
class QCompleter;
class QFormLayout;
class QLabel;
class QLineEdit;
class QListView;
class QStringListModel;
class QVBoxLayout;
class QWaitCondition;

namespace gpp
{
    class key_request : public QObject
    {
        Q_OBJECT

    public:
        const plKey& m_Needle;
        const std::vector<plKey>& m_Haystack;
        QWaitCondition* m_Signal;

        key_request(const plKey& needle, const std::vector<plKey>& haystack,
                    QWaitCondition* signal)
            : m_Needle(needle), m_Haystack(haystack), m_Signal(signal)
        { }
    };

    class key_finder : public QDialog
    {
        Q_OBJECT

    protected:
        QVBoxLayout* m_Layout;
        QLabel* m_Text;
        QFormLayout* m_Form;
        QLineEdit* m_Search;
        QCompleter* m_SearchCompleter;
        QListView* m_KeyList;
        QStringListModel* m_List;
        QCommandLinkButton* m_Accept;
        QCommandLinkButton* m_Decline;

        key_request* m_ActiveReq;
        std::vector<QString> m_QStrs;
        plKey m_PickedKey;


    public slots:
        void handle_KeyRequest(key_request*);

    private slots:
        void handle_SearchUpdate(const QString&);
        void handle_KeyActivated(const QModelIndex& index);
        void handle_Accept();
        void handle_Decline();

    private:
        void reset_KeyRequest();

    public:
        explicit key_finder(QWidget* parent=nullptr);
        ~key_finder();

    public:
        plKey steal_key() { return std::move(m_PickedKey); }
    };
};

#endif
