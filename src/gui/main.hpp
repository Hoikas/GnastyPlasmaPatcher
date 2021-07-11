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

#ifndef _GPP_GUI_MAIN_H
#define _GPP_GUI_MAIN_H

#include <QFutureWatcher>
#include <QMainWindow>
#include <QSignalMapper>

#include <filesystem>
#include <memory>
#include <tuple>

class QCommandLinkButton;
class QCompleter;
class QFileSystemModel;
class QFormLayout;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QVBoxLayout;

namespace gpp
{
    class key_finder;
    class key_request;
    class log2gui;

    class main_window : public QMainWindow
    {
        Q_OBJECT

    protected:
        QVBoxLayout* m_Layout;

        QFormLayout* m_Form;
        QLineEdit* m_SrcPath;
        QPushButton* m_SrcBtn;
        QSignalMapper* m_SrcMapper;
        QCompleter* m_SrcCompleter;
        QFileSystemModel* m_SrcFsModel;
        QLineEdit* m_DstPath;
        QPushButton* m_DstBtn;
        QSignalMapper* m_DstMapper;
        QCompleter* m_DstCompleter;
        QFileSystemModel* m_DstFsModel;

        QCommandLinkButton* m_PatchBtn;
        QCommandLinkButton* m_MergeBtn;
        QFutureWatcher<std::tuple<QString, QString>> m_Patcher;
        key_finder* m_KeyFinderDialog;

        QTextEdit* m_LogEdit;
        std::unique_ptr<log2gui> m_Log2Gui;

    private:
        void create_path_widgets(const QString& label,
                                 QLineEdit*& path, QPushButton*& btn, QSignalMapper*& mapper,
                                 QCompleter*& completer, QFileSystemModel*& model);
        void clear_log();

        std::tuple<QString, QString> patch(const std::filesystem::path& src,
                                           const std::filesystem::path& dst);
        std::tuple<QString, QString> merge(const std::filesystem::path& src,
                                           const std::filesystem::path& dst);

    private slots:
        void handle_PathBtn(QWidget* widget);
        void handle_ConvertBtnPush();
        void handle_MergeBtnPush();

        void handle_PatchStart();
        void handle_PatchFinished();
        void handle_LogMsg(const QString& msg);

    signals:
        void on_KeyRequest(key_request*);

    protected:
        void closeEvent(QCloseEvent* event) override;

    public:
        explicit main_window(QWidget* parent=nullptr);
        ~main_window();

    };

};

#endif // MAIN_H
