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

#include <algorithm>

#include <QtWidgets>

#include <PRP/KeyedObject/plKey.h>
#include <ResManager/plFactory.h>

#include "key_finder.hpp"

// ===========================================================================

gpp::key_finder::key_finder(QWidget* parent)
    : m_Layout(new QVBoxLayout(this)), m_Text(new QLabel("What do you see?", this)),
      m_Form(new QFormLayout(this)), m_Search(new QLineEdit(this)),
      m_SearchCompleter(new QCompleter(this)), m_KeyList(new QListView(this)),
      m_List(new QStringListModel(this)), m_ActiveReq(), QDialog(parent)
{
    setWindowTitle("Select Key");

    m_Accept = new QCommandLinkButton("Accept", "Patch the key currently selected in the list.", this);
    m_Accept->setEnabled(false);
    m_Decline = new QCommandLinkButton("Skip", "Skip patching this key. Note the behavior is "
                                               "undefined if you do this -- here be dragons!",
                                       this);
    m_KeyList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_SearchCompleter->setModel(m_List); // gulp...
    m_SearchCompleter->setCompletionMode(QCompleter::InlineCompletion);
    m_Search->setCompleter(m_SearchCompleter);

    m_Layout->addWidget(m_Text);
    m_Layout->addSpacing(20);
    m_Layout->addWidget(m_KeyList);
    m_Form->addRow("Search:", m_Search);
    m_Layout->addLayout(m_Form);
    m_Layout->addSpacing(10);
    m_Layout->addWidget(m_Accept);
    m_Layout->addWidget(m_Decline);
    setLayout(m_Layout);

    m_KeyList->setModel(m_List);

    connect(m_Search, &QLineEdit::textEdited, this, &key_finder::handle_SearchUpdate);
    connect(m_KeyList, &QListView::activated, this, &key_finder::handle_KeyActivated);
    connect(m_KeyList, &QListView::clicked, this, &key_finder::handle_KeyActivated);
    connect(m_Accept, &QCommandLinkButton::released, this, &key_finder::handle_Accept);
    connect(m_Decline, &QCommandLinkButton::released, this, &key_finder::handle_Decline);
    connect(this, &QDialog::rejected, this, &key_finder::handle_Decline);
}

gpp::key_finder::~key_finder()
{
}

// ===========================================================================

void gpp::key_finder::handle_KeyRequest(gpp::key_request* req)
{
    m_ActiveReq = req;
    m_QStrs.reserve(req->m_Haystack.size());
    std::for_each(req->m_Haystack.begin(), req->m_Haystack.end(),
                  [this](const plKey& i) {
                      ST::utf16_buffer buf = i->getName().to_utf16();
                      m_QStrs.emplace_back(QString::fromUtf16(buf.data(), buf.size()));
                  }
    );
    std::sort(m_QStrs.begin(), m_QStrs.end(),
              [](const QString& s1, const QString& s2) {
                  return s1.compare(s2, Qt::CaseInsensitive) < 1;
              }
    );

    m_Search->setText(QString());
    handle_SearchUpdate(QString());
    m_Search->setFocus(Qt::PopupFocusReason);

    const char* pClass = plFactory::ClassName(req->m_Needle->getType());
    ST::utf16_buffer nameBuf = req->m_Needle->getName().to_utf16();
    QString name = QString::fromUtf16(nameBuf.data(), nameBuf.size());
    QString msg = QString("We were unable to reolve the key [%1] '%2'.\n "
                          "Please select the matching key in the list.").arg(pClass, name);
    m_Text->setText(msg);

    // show thyself
    show();
}

void gpp::key_finder::handle_SearchUpdate(const QString& needle)
{
    QStringList strList;
    std::for_each(m_QStrs.begin(), m_QStrs.end(),
                  [&needle, &strList](const QString& i) {
                      if (i.startsWith(needle, Qt::CaseInsensitive))
                          strList.append(i);
                  }
    );
    m_List->setStringList(strList);
}

void gpp::key_finder::handle_KeyActivated(const QModelIndex& index)
{
    m_Accept->setEnabled(index.isValid());

    // update ths search box to match the selection but purposefully do not filter the list.
    if (index.isValid()) 
        m_Search->setText(m_List->data(index).toString());
}

// ===========================================================================

void gpp::key_finder::handle_Accept()
{
    if (m_ActiveReq) {
        QString qName = m_List->data(m_KeyList->currentIndex()).toString();
        static_assert(sizeof(QChar) == sizeof(char16_t), "QString is not utf-16!!!");
        ST::string name = ST::string::from_utf16(reinterpret_cast<const char16_t*>(qName.data()),
                                                 qName.size(), ST::assume_valid);

        auto it = std::find_if(m_ActiveReq->m_Haystack.begin(), m_ActiveReq->m_Haystack.end(),
                               [&name](const plKey& i) {
                                   return i->getName() == name;
                               }
        );
        if (it == m_ActiveReq->m_Haystack.end()) {
            QMessageBox::critical(this, "Error", "Mapping selection into the key haystack failed.\n"
                                                 "Contact a h4xx0r to fix this problem.",
                                  QMessageBox::Ok, QMessageBox::NoButton);
            return;
        }

        m_PickedKey = *it;
        m_ActiveReq->m_Signal->notify_one();
        reset_KeyRequest();
    }
}

void gpp::key_finder::handle_Decline()
{
    if (m_ActiveReq) {
        m_ActiveReq->m_Signal->notify_one();
        reset_KeyRequest();
    }
}

void gpp::key_finder::reset_KeyRequest()
{
    hide();
    m_Accept->setEnabled(false);
    m_ActiveReq = nullptr;
    m_QStrs.clear();
    m_List->setStringList(QStringList());
}
