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

#include <QtCore>
#include <QtConcurrent>
#include <QtWidgets>

#include <Debug/plDebug.h>

#include "main.hpp"
#include "key_finder.hpp"
#include "buildinfo.hpp"
#include "errors.hpp"
#include "log2gui.hpp"
#include "patcher.hpp"

// ===========================================================================

gpp::main_window::main_window(QWidget* parent)
    : m_Layout(new QVBoxLayout(this)),
      m_Form(new QFormLayout(this)),
      m_KeyFinderDialog(new key_finder(this)),
      m_LogEdit(new QTextEdit(this)),
      m_Log2Gui(log2gui::create()),
      QMainWindow(parent)
{
    setWindowTitle(QString("Gnasty Plasma Patcher (%1)").arg(build_version()));
    resize({ 550, 400 });

    create_path_widgets("Source Age/PRP:", m_SrcPath, m_SrcBtn, m_SrcMapper, m_SrcCompleter, m_SrcFsModel);
    create_path_widgets("Desintation Age/PRP:", m_DstPath, m_DstBtn, m_DstMapper, m_DstCompleter, m_DstFsModel);
    m_Layout->addLayout(m_Form);

    m_PatchBtn = new QCommandLinkButton("Patch Existing Objects",
                                        "Merge the changed objects from Source into Destination.",
                                        this);
    connect(m_PatchBtn, &QCommandLinkButton::released, this, &main_window::handle_ConvertBtnPush);
    m_Layout->addWidget(m_PatchBtn);

    m_MergeBtn = new QCommandLinkButton("Merge New Objects",
                                        "Merge all objects from Source into Destination.",
                                         this);
    connect(m_MergeBtn, &QCommandLinkButton::released, this, &main_window::handle_MergeBtnPush);
    m_Layout->addWidget(m_MergeBtn);
    m_Layout->addSpacing(20);

    m_LogEdit->setAcceptRichText(true);
    {
        QFont f("Cascadia Code");
        f.setStyleHint(QFont::Monospace);
        f.setPointSize(10);
        m_LogEdit->setFont(f);
    }
    m_LogEdit->setReadOnly(true);

    clear_log();
    m_Layout->addWidget(m_LogEdit);

    // DEBUG: REMOVE BEFORE PUSH!!!
    QSettings settings;
    m_SrcPath->setText(settings.value("source_path").toString());
    m_DstPath->setText(settings.value("destination_path").toString());

    // enable/disable things while the patcher runs
    connect(&m_Patcher, &decltype(m_Patcher)::started, this, &main_window::handle_PatchStart);
    connect(&m_Patcher, &decltype(m_Patcher)::finished, this, &main_window::handle_PatchFinished);
    connect(this, &main_window::on_KeyRequest, m_KeyFinderDialog, &key_finder::handle_KeyRequest);
    connect(m_Log2Gui.get(), &log2gui::append, this, &main_window::handle_LogMsg);

    // what the Qt? so crazy...
    auto dummy = new QWidget(this);
    dummy->setLayout(m_Layout);
    setCentralWidget(dummy);
}

void gpp::main_window::create_path_widgets(const QString& label,
                                           QLineEdit*& path, QPushButton*& btn, QSignalMapper*& mapper,
                                           QCompleter*& completer, QFileSystemModel*& model)
{
    path = new QLineEdit();
    btn = new QPushButton("Browse");

    QStringList filters;
    filters.append("*.age");
    filters.append("*.prp");

    completer = new QCompleter(this);
    model = new QFileSystemModel(completer);
    model->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    model->setNameFilters(filters);
    model->setRootPath(QString());
    completer->setModel(model);
    path->setCompleter(completer);

    auto hlayout = new QHBoxLayout();
    hlayout->addWidget(path);
    hlayout->addWidget(btn);
    m_Form->addRow(label, hlayout);

    // Goal is to get the QLineEdit* matching the button in the callback (not signal, not slot, or
    // whatever the hell Qt calls it. It's a ruddy callback).
    mapper = new QSignalMapper(this);
    connect(btn, SIGNAL(released()), mapper, SLOT(map()));
    mapper->setMapping(btn, path);
    connect(mapper, SIGNAL(mapped(QWidget*)), this, SLOT(handle_PathBtn(QWidget*)));
}

void gpp::main_window::clear_log()
{
    m_LogEdit->clear();
    m_LogEdit->insertPlainText("Welcome to GnastyPlasmaPatcher!\n\n");
}

gpp::main_window::~main_window()
{
    QSettings settings;
    settings.setValue("source_path", m_SrcPath->text());
    settings.setValue("destination_path", m_DstPath->text());
}

// ===========================================================================

void gpp::main_window::closeEvent(QCloseEvent* event)
{
    if (m_Patcher.isRunning())
        event->ignore();
    else
        QMainWindow::closeEvent(event);
}

// ===========================================================================

void gpp::main_window::handle_PathBtn(QWidget* widget)
{
    QLineEdit* le = static_cast<QLineEdit*>(widget);
    QString prevPath = le->text().isEmpty() ? QCoreApplication::applicationDirPath() :
                                              le->text();

    // Make sure it's a valid ass-path.
    if (!QDir(prevPath).exists())
        prevPath = QString();

    QString path = QFileDialog::getOpenFileName(this,
                                                "Select Age or PRP",
                                                prevPath,
                                                "All files (*.*) ;; Age files (*.age);; PRP files (*.prp)");
    if (!path.isNull())
        le->setText(QDir::toNativeSeparators(path));
}

// ===========================================================================

template<typename T = std::filesystem::path, typename _StrType = typename T::string_type>
std::enable_if_t<std::is_same_v<_StrType, std::string>, T>
IConvertQStr(const QString& path)
{
    T result = path.toStdString();
    result.make_preferred();
    return result;
}

template<typename T = std::filesystem::path, typename _StrType = typename T::string_type>
std::enable_if_t<std::is_same_v<_StrType, std::wstring>, T>
IConvertQStr(const QString& path)
{
    T result = path.toStdWString();
    result.make_preferred();
    return result;
}

// ===========================================================================

class ColorHax
{
    QTextEdit* m_Widget;
    QColor m_OldColor;

public:
    ColorHax() = delete;
    ColorHax(const ColorHax&) = delete;
    ColorHax(ColorHax&&) = delete;

    ColorHax(QTextEdit* widget, const QColor& desired)
        : m_Widget(widget), m_OldColor(widget->textColor())
    {
        m_Widget->setTextColor(desired);
    }

    ~ColorHax()
    {
        m_Widget->setTextColor(m_OldColor);
    }
};

// ===========================================================================

void gpp::main_window::handle_PatchStart()
{
    m_SrcPath->setDisabled(true);
    m_SrcBtn->setDisabled(true);
    m_DstPath->setDisabled(true);
    m_DstBtn->setDisabled(true);
    m_PatchBtn->setDisabled(true);
    m_MergeBtn->setDisabled(true);
}

void gpp::main_window::handle_PatchFinished()
{
    // If there was an error, log it and complain here.
    auto [error, detail] = m_Patcher.result();
    if (!error.isEmpty()) {
        ColorHax _(m_LogEdit, QColor::fromRgba(qRgb(255, 0, 0)));
        m_LogEdit->insertPlainText(QString("\nFatal Error: %1\n%2\n").arg(error, detail));
        QMessageBox::critical(this, error, detail, QMessageBox::Ok, QMessageBox::NoButton);
    }

    m_SrcPath->setDisabled(false);
    m_SrcBtn->setDisabled(false);
    m_DstPath->setDisabled(false);
    m_DstBtn->setDisabled(false);
    m_PatchBtn->setDisabled(false);
    m_MergeBtn->setDisabled(false);
}

// ===========================================================================

std::tuple<QString, QString> gpp::main_window::patch(const std::filesystem::path& src,
                                                     const std::filesystem::path& dst)
{
    try {
        patcher patcher(src, dst);
        patcher.set_map_func([this](const plKey& needle, const std::vector<plKey>& haystack) {
            QWaitCondition wait;
            QMutex mut;
            mut.lock();

            // We must block for the answer from the key request dialog once we dispatch
            // the singal -- otherwise the memory goes away and anarchy rules the earth.
            key_request req(needle, haystack, &wait);
            emit on_KeyRequest(&req);
            wait.wait(&mut);
            return m_KeyFinderDialog->steal_key();
        });
        patcher.process_collision();
        patcher.process_drawables();
        patcher.save_damage(src, dst);
    } catch (const error& ex) {
        return std::make_tuple("Patch Failed", ex.what());
#if !defined(_DEBUG) || defined(NDEBUG)
    } catch (const std::exception& ex) {
        return std::make_tuple("Unhandled Exception", ex.what());
#endif
    }

    return std::make_tuple(QString(), QString());
}

std::tuple<QString, QString> gpp::main_window::merge(const std::filesystem::path& src,
                                                     const std::filesystem::path& dst)
{
    try {
        merger patcher(src, dst);
        patcher.process();
        patcher.save_damage(src, dst);
    } catch (const error& ex) {
        return std::make_tuple("Merge Failed", ex.what());
#if !defined(_DEBUG) || defined(NDEBUG)
    } catch (const std::exception& ex) {
        return std::make_tuple("Unhandled Exception", ex.what());
#endif
    }

    return std::make_tuple(QString(), QString());
}

void gpp::main_window::handle_ConvertBtnPush()
{
    clear_log();

    auto src = IConvertQStr(m_SrcPath->text());
    auto dst = IConvertQStr(m_DstPath->text());
    // Flip to 0 for debugging
#if 1 // !defined(_DEBUG) || defined(NDEBUG)
    auto fut = QtConcurrent::run(this, &main_window::patch, src, dst);
    m_Patcher.setFuture(fut);
#else
    patch(src, dst);
#endif
}

void gpp::main_window::handle_MergeBtnPush()
{
    clear_log();

    auto src = IConvertQStr(m_SrcPath->text());
    auto dst = IConvertQStr(m_DstPath->text());

    // Flip to 0 for debugging
#if 1 // !defined(_DEBUG) || defined(NDEBUG)
    auto fut = QtConcurrent::run(this, &main_window::merge, src, dst);
    m_Patcher.setFuture(fut);
#else
    merge(src, dst);
#endif
}


void gpp::main_window::handle_LogMsg(const QString& msg)
{
    m_LogEdit->insertPlainText(msg);
    auto cursor = m_LogEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_LogEdit->setTextCursor(cursor);
}

// ===========================================================================

int main(int argc, char* argv[])
{
    QCoreApplication::setApplicationName("Gnasty Plasma Patcher");
    QCoreApplication::setOrganizationName("Tsar Hoikas");

    QApplication app(argc, argv);
    gpp::main_window mainWnd;
    mainWnd.show();
    return app.exec();
}
