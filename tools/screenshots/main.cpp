// ofs-screenshots: drives the program through its project types, editors, dialogs, menus and
// themes, saving a screenshot of every step and whether it worked (index.json). It works on a
// copy of a M.U.G.E.N folder (characters, stages, screenpack, fonts), so nothing of the original
// is changed. Run it on a desktop (not the offscreen platform) to get real fonts and OpenGL.
//
//   ofs-screenshots --mugen <M.U.G.E.N folder> --out <folder> [--sffv2 <character folder>]

#include "core/actions/ActionRegistry.h"
#include "core/app/AppContext.h"
#include "core/app/Paths.h"
#include "core/code/CodeEditor.h"
#include "core/settings/Config.h"
#include "core/settings/History.h"
#include "core/widgets/RenderSurface.h"
#include "shell/FileDialogs.h"
#include "shell/MainWindow.h"
#include "shell/ProjectView.h"
#include "shell/dialogs/FirstRunDialog.h"

#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QCommandLineParser>
#include <QDialog>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPointer>
#include <QScreen>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>

#include <cstring>
#include <functional>

using namespace OFS;

namespace {

void wait(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

bool copyFolder(const QString& from, const QString& to)
{
    QDirIterator it(from, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString file = it.next();
        const QString target = to + file.mid(from.size());
        QDir().mkpath(QFileInfo(target).absolutePath());
        if (!QFile::copy(file, target))
            return false;
    }
    return true;
}

class Tour
{
public:
    Tour(AppContext& context, MainWindow& window, const QString& out)
        : m_context(context)
        , m_window(window)
        , m_out(out)
    {
        QDir().mkpath(out);
    }

    // Only the steps of these areas (all when empty); the others are skipped.
    void setOnly(const QStringList& areas) { m_only = areas; }
    bool wants(const QString& area) const { return m_only.isEmpty() || m_only.contains(area, Qt::CaseInsensitive); }

    void shot(QWidget* widget, const QString& area, const QString& title, bool ok = true, const QString& note = {})
    {
        if (!wants(area))
            return;
        wait(300);
        QString file;
        if (widget) {
            file = QStringLiteral("%1.png").arg(++m_count, 3, 10, QLatin1Char('0'));
            widget->grab().save(QDir(m_out).filePath(file));
        }
        record(area, title, ok && widget, note, file);
    }
    void window(const QString& area, const QString& title, bool ok = true, const QString& note = {})
    {
        shot(&m_window, area, title, ok, note);
    }

    // What the screen shows over the window (popups included).
    void screen(const QString& area, const QString& title, bool ok = true, const QString& note = {})
    {
        if (!wants(area))
            return;
        wait(400);
        const QString file = QStringLiteral("%1.png").arg(++m_count, 3, 10, QLatin1Char('0'));
        const QRect frame = m_window.frameGeometry();
        QScreen* where = m_window.screen();
        const QPixmap picture = where->grabWindow(0, frame.x() - where->geometry().x(),
                                                  frame.y() - where->geometry().y(), frame.width(), frame.height());
        picture.save(QDir(m_out).filePath(file));
        record(area, title, ok && !picture.isNull(), note, file);
    }

    void record(const QString& area, const QString& title, bool ok, const QString& note = {}, const QString& file = {})
    {
        QJsonObject entry{{"area", area}, {"title", title}, {"ok", ok}, {"file", file}};
        if (!note.isEmpty())
            entry.insert("note", note);
        m_entries.append(entry);
        qInfo().noquote() << (ok ? "ok  " : "FAIL") << area << "|" << title
                          << (note.isEmpty() ? QString() : QStringLiteral("| ") + note);
    }

    QAction* action(const QString& id) const { return m_context.actions().action(id); }

    bool trigger(const QString& id)
    {
        QAction* a = action(id);
        if (!a || !a->isEnabled())
            return false;
        a->trigger();
        return true;
    }

    // Runs start, photographs the first modal window it opens and closes it. No window is a failure.
    bool modal(const QString& area, const QString& title, const std::function<void()>& start,
               const std::function<void(QWidget*)>& inside = {})
    {
        if (!wants(area))
            return false;
        QPointer<QWidget> before = QApplication::activeModalWidget();
        bool seen = false;
        int elapsed = 0;
        QTimer timer;
        timer.setInterval(40);
        QObject::connect(&timer, &QTimer::timeout, [&] {
            elapsed += 40;
            QWidget* m = QApplication::activeModalWidget();
            if (!m || m == before || !m->isVisible()) {
                if (!seen && elapsed > 8000)
                    timer.stop();
                return;
            }
            if (seen) {
                close(m); // follow-up questions of the same action
                return;
            }
            seen = true;
            timer.stop();
            wait(400);
            if (inside) {
                inside(m);
            } else {
                // A warning or an error is a failed step; a question is part of the action.
                auto* box = qobject_cast<QMessageBox*>(m);
                const bool problem =
                    box && (box->icon() == QMessageBox::Warning || box->icon() == QMessageBox::Critical);
                shot(m, area, title, !problem, box ? box->windowTitle() + ": " + box->text() : QString());
            }
            close(m);
            timer.start();
        });
        timer.start();
        start();
        // Not modal (the call returned at once), so wait for it a moment.
        for (int i = 0; i < 25 && timer.isActive(); ++i)
            wait(40);
        timer.stop();
        if (!seen)
            record(area, title, false, QStringLiteral("no window opened"));
        return seen;
    }

    // A window that isn't modal (Calculator, Clipboard container...), photographed and closed.
    bool topLevel(const QString& area, const QString& title, const std::function<void()>& start)
    {
        if (!wants(area))
            return false;
        const QList<QWidget*> before = QApplication::topLevelWidgets();
        start();
        wait(500);
        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (!before.contains(w) && w->isVisible() && !qobject_cast<QMenu*>(w)) {
                shot(w, area, title);
                w->close();
                return true;
            }
        }
        record(area, title, false, QStringLiteral("no window opened"));
        return false;
    }

    static void close(QWidget* m)
    {
        if (auto* box = qobject_cast<QMessageBox*>(m)) {
            QAbstractButton* button = box->escapeButton() ? box->escapeButton() : box->buttons().value(0);
            if (box->button(QMessageBox::No))
                button = box->button(QMessageBox::No);
            else if (box->button(QMessageBox::Cancel))
                button = box->button(QMessageBox::Cancel);
            if (button)
                button->click();
            else
                box->reject();
        } else if (auto* dialog = qobject_cast<QDialog*>(m)) {
            dialog->reject();
        } else {
            m->close();
        }
    }

    void menu(const QString& area, const QString& objectName, const QString& prefix = {})
    {
        if (!wants(area))
            return;
        QMenu* menu = nullptr;
        for (QAction* a : m_window.menuBar()->actions()) {
            if (a->menu() && a->menu()->objectName() == objectName && a->isVisible())
                menu = a->menu();
        }
        if (!menu) {
            record(area, objectName, false, QStringLiteral("menu not shown"));
            return;
        }
        menu->popup(m_window.mapToGlobal(QPoint(40, 60)));
        wait(400);
        shot(menu, area, prefix + QStringLiteral("Menu: ") + menu->title().remove(QLatin1Char('&')));
        menu->hide();
        wait(100);
    }

    ProjectView* view() const { return m_window.currentProjectView(); }

    bool editor(const QString& id)
    {
        ProjectView* v = view();
        if (!v || !v->editorIds().contains(id))
            return false;
        v->setCurrentEditor(id);
        wait(600);
        return v->currentEditor() == id;
    }

    void closeAll()
    {
        while (ProjectView* v = view()) {
            QTimer::singleShot(0, [] {
                if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                    Tour::close(box);
            });
            // Discard changes: the tour never saves.
            m_context.config().setValue("showmsg_discard", "1");
            m_context.config().setValue("confirm_close", false);
            trigger(QStringLiteral("file_close"));
            wait(300);
            if (view() == v)
                break;
        }
    }

    bool save()
    {
        QFile file(QDir(m_out).filePath(QStringLiteral("index.json")));
        if (!file.open(QIODevice::WriteOnly))
            return false;
        file.write(QJsonDocument(m_entries).toJson());
        return true;
    }

    int failures() const
    {
        int n = 0;
        for (const QJsonValue& v : m_entries)
            n += v.toObject().value("ok").toBool() ? 0 : 1;
        return n;
    }

private:
    AppContext& m_context;
    MainWindow& m_window;
    QString m_out;
    QStringList m_only;
    QJsonArray m_entries;
    int m_count = 0;
};

// Counts the paint events of a widget and its children (frames drawn).
class PaintCounter : public QObject
{
public:
    explicit PaintCounter(QWidget* root)
    {
        root->installEventFilter(this);
        for (QWidget* child : root->findChildren<QWidget*>())
            child->installEventFilter(this);
    }
    int count = 0;

protected:
    bool eventFilter(QObject*, QEvent* event) override
    {
        if (event->type() == QEvent::Paint)
            ++count;
        return false;
    }
};

// The panel's combo box `name` set to the item `text`.
bool chooseItem(QWidget* root, const QString& name, const QString& text)
{
    auto* combo = root ? root->findChild<QComboBox*>(name) : nullptr;
    const int index = combo ? combo->findText(text) : -1;
    if (index < 0)
        return false;
    combo->setCurrentIndex(index);
    return true;
}

bool click(QWidget* root, const QString& name)
{
    auto* button = root ? root->findChild<QAbstractButton*>(name) : nullptr;
    if (!button || !button->isEnabled())
        return false;
    button->click();
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    QApplication::setApplicationName(QStringLiteral("Open Factory Studio"));
    QApplication::setApplicationDisplayName(QStringLiteral("Open Factory Studio"));
    QApplication::setOrganizationName(QStringLiteral("Open Factory Studio"));
    QApplication app(argc, argv);
    QApplication::setStyle(QStringLiteral("windowsvista"));
    QCommandLineParser parser;
    parser.addOption({"mugen", "M.U.G.E.N folder to copy (chars/kfm, stages, data, font).", "folder"});
    parser.addOption({"sffv2", "A character with an SFF v2 sprite file (optional).", "folder"});
    parser.addOption({"out", "Folder for the screenshots and index.json.", "folder"});
    parser.addOption({"only", "Only the steps of these areas (comma separated, e.g. Character,Sounds).", "areas"});
    parser.addHelpOption();
    parser.process(app);
    if (!parser.isSet("mugen") || !parser.isSet("out"))
        parser.showHelp(2);

    // A copy of the engine folder and fresh settings.
    QTemporaryDir work;
    const QString engine = work.filePath("mugen");
    if (!copyFolder(QDir(parser.value("mugen")).absolutePath(), engine)) {
        qCritical("could not copy %s", qPrintable(parser.value("mugen")));
        return 1;
    }
    qputenv("OFS_CONFIG_DIR", work.filePath("config").toLocal8Bit());
    qputenv("OFS_AUDIO_BACKEND", "null");
    QDir().mkpath(work.filePath("config"));
    {
        QFile config(work.filePath("config/config.xml"));
        if (config.open(QIODevice::WriteOnly))
            config.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<configuration>\n"
                         "    <config name=\"initialized\" value=\"true\"/>\n"
                         "    <config name=\"config_wizard\" value=\"false\"/>\n"
                         "    <config name=\"confirm_close\" value=\"false\"/>\n"
                         "    <config name=\"auto_update\" value=\"false\"/>\n"
                         "</configuration>\n");
    }

    AppContext context;
    context.loadSettings();
    Rendering::choose(context.config(), Paths::userDir());
    context.initializeEngines();
    MainWindow window(context);
    window.resize(1600, 1000);
    window.move(40, 30);
    window.show();
    wait(1500);
    Tour tour(context, window, parser.value("out"));
    if (parser.isSet("only"))
        tour.setOnly(parser.value("only").split(QLatin1Char(','), Qt::SkipEmptyParts));

    // File dialogs answer from the tour: an image or a sound when one is asked for, else nothing.
    const QString image = work.filePath("tour.png");
    {
        QImage picture(48, 64, QImage::Format_Indexed8);
        picture.setColorCount(4);
        picture.setColor(0, qRgb(255, 0, 255));
        picture.setColor(1, qRgb(40, 40, 160));
        picture.setColor(2, qRgb(240, 200, 120));
        picture.setColor(3, qRgb(20, 20, 20));
        picture.fill(0);
        for (int y = 8; y < 60; ++y)
            for (int x = 12; x < 36; ++x)
                picture.setPixel(x, y, y < 24 ? 2 : (x + y) % 7 ? 1 : 3);
        picture.save(image);
    }
    bool imagesWanted = false; // the Sprites editor asks for images with the caption "Add"
    FileDialogs::setOpenPromptOverride([&](const QString& caption, const QString&, const QString&) {
        return caption.contains("image", Qt::CaseInsensitive) || caption.contains("sprite", Qt::CaseInsensitive) ||
                       (imagesWanted && caption == QLatin1String("Add"))
                   ? image
                   : QString();
    });
    const QString gif = work.filePath("tour.gif");
    FileDialogs::setSavePromptOverride([&](const QString& caption, const QString&, const QString& filter) {
        return caption.contains("GIF", Qt::CaseInsensitive) || filter.contains("gif", Qt::CaseInsensitive) ? gif
                                                                                                           : QString();
    });

    // ---- The program without a project ----------------------------------------------------------
    tour.window("Start", "Welcome screen");
    for (const char* menu : {"menu_project", "menu_edit", "menu_view", "menu_debug", "menu_tools", "menu_help"})
        tour.menu("Start", QString::fromLatin1(menu));
    tour.modal("Dialogs", "About", [&] { tour.trigger("help_about"); });
    tour.modal("Dialogs", "New project", [&] { tour.trigger("file_new"); });
    tour.modal(
        "Options", "Options", [&] { tour.trigger("tools_options"); },
        [&](QWidget* dialog) {
            auto* tabs = dialog->findChild<QTabWidget*>("tabWidget");
            for (int i = 0; tabs && i < tabs->count(); ++i) {
                tabs->setCurrentIndex(i);
                tour.shot(dialog, "Options", "Options: " + tabs->tabText(i));
            }
        });
    tour.topLevel("Tools", "Calculator", [&] { tour.trigger("view_calculator"); });
    tour.topLevel("Tools", "Clipboard container", [&] { tour.trigger("view_clipboard"); });
    tour.modal("Tools", "Script editor", [&] { tour.trigger("tools_script_editor"); });

    // ---- A character -------------------------------------------------------------------------------
    const QString kfm = engine + "/chars/kfm/kfm.def";
    const bool opened = window.openProject(kfm);
    wait(1500);
    tour.window("Character", "Kung Fu Man opened", opened);
    struct EditorShot
    {
        const char* id;
        const char* title;
    };
    for (const EditorShot& e : {EditorShot{"definitions", "Definitions editor"},
                                {"commands", "Commands editor"},
                                {"states", "States editor"},
                                {"sprites", "Sprites editor"},
                                {"animations", "Animations editor"},
                                {"sounds", "Sounds editor"}})
        tour.window("Character", e.title, tour.editor(e.id));
    for (const char* menu : {"menu_project", "menu_edit", "menu_view", "menu_debug", "menu_palettes", "menu_sprites",
                             "menu_animations", "menu_commands", "menu_states", "menu_sounds", "menu_tools"})
        tour.menu("Character menus", QString::fromLatin1(menu));

    // Sprites.
    tour.editor("sprites");
    imagesWanted = true;
    tour.modal("Sprites", "Add sprites", [&] { tour.trigger("spr_add"); });
    imagesWanted = false;
    tour.modal("Sprites", "Resize", [&] { tour.trigger("spr_resize"); });
    tour.modal("Sprites", "Crop image", [&] { tour.trigger("spr_crop"); });
    tour.modal("Sprites", "Save as (format)", [&] { tour.trigger("spr_saveas"); });
    tour.modal("Sprites", "Alignment tool", [&] { tour.trigger("spr_align"); });
    tour.modal("Sprites", "Export", [&] { tour.trigger("spr_export"); });
    tour.modal("Sprites", "Change image (questions)", [&] { tour.trigger("spr_openimg"); });
    tour.window("Sprites", "Image editor", tour.trigger("spr_edit_img"));
    tour.window("Sprites", "Image editor with the Sprite explorer", tour.trigger("spr_explorer"));
    tour.trigger("spr_explorer");
    tour.trigger("spr_edit_img");
    wait(300);
    tour.modal("Sprites", "Advanced palette editor", [&] { tour.trigger("tools_pal_editor"); });
    tour.topLevel("Palettes", "Remap", [&] { tour.trigger("pal_remap"); });

    // Animations.
    tour.editor("animations");
    tour.trigger("anim_play");
    wait(700);
    tour.window("Animations", "Playing the standing animation");
    tour.trigger("anim_stop");
    tour.trigger("anim_next_frame");
    tour.window("Animations", "Contact boxes on a frame");
    tour.modal("Animations", "Save as animated GIF", [&] { tour.trigger("anim_save_gif"); });
    tour.modal("Animations", "Alignment tool", [&] { tour.trigger("anim_frame_align"); });
    tour.topLevel("Animations", "Edit animation code", [&] { tour.trigger("anim_code"); });

    // States and commands.
    tour.editor("states");
    tour.modal("States", "State controller", [&] { tour.trigger("st_sctrl_new"); });
    tour.modal("States", "Throw creator", [&] { tour.trigger("st_throw"); });
    tour.modal("States", "Offset viewer", [&] { tour.trigger("st_offset"); });
    tour.modal("States", "PalFX editor", [&] { tour.trigger("st_palfx"); });
    tour.window("States", "Variable usage list", tour.trigger("st_var_list"));
    tour.trigger("st_var_list");

    // The code editor: completion while typing and the documentation of a controller.
    if (CodeEditor* code = tour.view() ? tour.view()->currentCodeEditor() : nullptr) {
        // The window active again after the dialogs, so the completion list may show.
        window.raise();
        window.activateWindow();
        wait(400);
        code->setFocus();
        QTextCursor cursor = code->textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.insertText("\n[State 9999, tour]\n");
        code->setTextCursor(cursor);
        // Typed as a user does: "=" opens the list of controllers, the letters go to the word.
        for (const QChar c : QStringLiteral("type = Chan")) {
            QKeyEvent press(QEvent::KeyPress, 0, Qt::NoModifier, QString(c));
            QApplication::sendEvent(code, &press);
        }
        for (int i = 0; i < 20 && !code->isCompleting(); ++i)
            wait(50);
        tour.screen("Code", "Completion while typing", code->isCompleting());
        QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(code, &escape);
        code->document()->undo();
        code->document()->undo();
        const int at = code->toPlainText().indexOf("ChangeState");
        if (at >= 0) {
            QTextCursor word(code->document());
            word.setPosition(at + 3);
            code->setTextCursor(word);
            code->centerCursor();
            wait(200);
            const QPoint point = code->cursorRect().center();
            QMouseEvent click(QEvent::MouseButtonPress, point, code->viewport()->mapToGlobal(point), Qt::LeftButton,
                              Qt::LeftButton, Qt::ControlModifier);
            QApplication::sendEvent(code->viewport(), &click);
            QMouseEvent release(QEvent::MouseButtonRelease, point, code->viewport()->mapToGlobal(point), Qt::LeftButton,
                                Qt::NoButton, Qt::ControlModifier);
            QApplication::sendEvent(code->viewport(), &release);
            tour.screen("Code", "Documentation of ChangeState (Ctrl+click)");
            QKeyEvent close(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(QApplication::activePopupWidget() ? QApplication::activePopupWidget() : code,
                                    &close);
            wait(200);
        }
    } else {
        tour.record("Code", "Completion while typing", false, "no code editor");
    }
    tour.modal("Code", "Go to line", [&] { tour.trigger("code_goto"); });
    tour.window("Code", "Find/Replace bar", tour.trigger("edit_find_replace"));
    tour.trigger("view_issues");
    tour.window("Code", "Issues panel", true);
    tour.trigger("view_advanced_search");
    tour.window("Code", "Advanced Search panel", true);

    // Sounds.
    tour.editor("sounds");
    tour.modal("Sounds", "Resample", [&] { tour.trigger("snd_resample"); });

    // Project management.
    tour.modal("Project", "Load backup", [&] { tour.trigger("file_load_backup"); });
    tour.modal("Project", "Export to HTML", [&] { tour.trigger("tools_export_html"); });
    tour.modal("Debugger", "Add breakpoint", [&] { tour.trigger("breakpoint_add"); });

    // Emulator: versus, training, the debugger panels.
    if (tour.editor("emulator")) {
        tour.window("Emulator", "Emulator editor");
        QWidget* panel = tour.view();
        const bool started = click(panel, "emul_start");
        wait(1000);
        // Frames drawn in five seconds of the match (60 a second when it keeps up).
        QString speed;
        if (auto* workspace = tour.view()->findChild<QWidget*>("workspace_emulator")) {
            PaintCounter counter(workspace);
            QElapsedTimer clock;
            clock.start();
            wait(5000);
            speed = QStringLiteral("%1 frames in %2 s (%3 fps)")
                        .arg(counter.count)
                        .arg(clock.elapsed() / 1000.0, 0, 'f', 1)
                        .arg(counter.count * 1000.0 / double(clock.elapsed()), 0, 'f', 1);
        }
        tour.window("Emulator", "Kung Fu Man vs Kung Fu Man", started, speed);
        click(panel, "emul_start"); // stop
        wait(500);
        chooseItem(panel, "emul_match_mode", "Training");
        click(panel, "emul_start");
        wait(3500);
        tour.window("Emulator", "Training mode");
        tour.trigger("debug_interrupt");
        wait(500);
        tour.trigger("view_debugger");
        tour.window("Emulator", "Debugger: the match interrupted");
        tour.trigger("debug_stop");
        wait(500);
    } else {
        tour.record("Emulator", "Emulator editor", false, "no Emulator editor");
    }

    // Themes: the presets and their icon sets, on the Sprites editor.
    tour.editor("sprites");
    Config& config = context.config();
    for (const char* preset : {"Classic", "Clean", "Neo", "Ultimate"}) {
        config.setValue("preset", QString::fromLatin1(preset));
        config.setValue("iconset", QString());
        config.setValue("stylesheet", QString());
        config.setValue("colorset", QString());
        window.applyInterface();
        wait(800);
        tour.window("Themes", QStringLiteral("Preset %1").arg(QString::fromLatin1(preset)));
    }
    for (const char* icons : {"Flat-Dark", "Flat-Light", "Nuvola"}) {
        config.setValue("preset", QStringLiteral("Neo"));
        config.setValue("iconset", QString::fromLatin1(icons));
        config.setValue("stylesheet", QString::fromLatin1(strcmp(icons, "Flat-Light") == 0 ? "Light" : "Dark"));
        window.applyInterface();
        wait(800);
        tour.window("Themes", QStringLiteral("Icon set %1").arg(QString::fromLatin1(icons)));
    }

    // Every first run theme over the editors, panels, docks, a menu and dialogs. The text must stay
    // readable on each.
    auto applyTheme = [&](const FirstRunDialog::Theme& theme) {
        config.setValue("preset", theme.preset);
        config.setValue("iconset", theme.iconset);
        config.setValue("stylesheet", theme.stylesheet);
        config.setValue("colorset", theme.colorset);
        window.applyInterface();
        wait(800);
    };
    for (const FirstRunDialog::Theme& theme : FirstRunDialog::themes()) {
        if (!tour.wants("Theme"))
            break;
        applyTheme(theme);
        const QString name = theme.name + QStringLiteral(": ");
        for (const char* id : {"definitions", "sprites", "animations", "states", "commands", "sounds", "emulator"}) {
            if (tour.editor(QString::fromLatin1(id)))
                tour.window("Theme", name + QString::fromLatin1(id));
        }
        tour.editor("states");
        for (const char* dock :
             {"view_issues", "view_advanced_search", "view_engine_output", "view_debugger", "view_watch"}) {
            tour.trigger(QString::fromLatin1(dock));
            wait(300);
            tour.window("Theme", name + QString::fromLatin1(dock));
        }
        if (auto* tabs = tour.view() ? tour.view()->findChild<QTabBar*>("leftPanelTabs") : nullptr) {
            tabs->setCurrentIndex(1);
            wait(300);
            tour.window("Theme", name + "Project explorer");
            tabs->setCurrentIndex(0);
        }
        tour.editor("sprites");
        tour.trigger("view_organizer");
        wait(300);
        tour.window("Theme", name + "Organizer");
        tour.trigger("view_organizer");
        tour.menu("Theme", "menu_sprites", name);
        tour.modal("Theme", name + "Options", [&] { tour.trigger("tools_options"); });
        tour.modal("Theme", name + "Question", [&] {
            QMessageBox::question(&window, QStringLiteral("Close"),
                                  QStringLiteral("Do you really want to close Open Factory Studio?"));
        });
        tour.modal("Theme", name + "About", [&] { tour.trigger("help_about"); });
    }
    tour.closeAll();
    if (tour.wants("Theme")) {
        for (const FirstRunDialog::Theme& theme : FirstRunDialog::themes()) {
            applyTheme(theme);
            tour.window("Theme", theme.name + QStringLiteral(": Welcome page"));
        }
    }

    // ---- A stage -----------------------------------------------------------------------------------
    if (window.openProject(engine + "/stages/stage0.def")) {
        wait(1200);
        tour.window("Stage", "Backgrounds editor", tour.editor("backgrounds"));
        tour.trigger("bgs_play");
        wait(1200);
        tour.window("Stage", "Backgrounds playing");
        tour.trigger("bgs_stop");
        tour.window("Stage", "Definitions editor", tour.editor("definitions"));
        if (tour.editor("emulator")) {
            click(tour.view(), "emul_start");
            wait(3000);
            tour.window("Stage", "Emulator on the stage");
            click(tour.view(), "emul_start");
        }
        tour.menu("Stage", "menu_backgrounds");
        tour.closeAll();
    } else {
        tour.record("Stage", "Open stage0.def", false);
    }

    // ---- The screenpack (motif), its fight screen, a storyboard, a font ----------------------------
    if (window.openProject(engine + "/data/system.def")) {
        wait(1500);
        tour.window("Screenpack", "Motif: Definitions (system, select, fight files)", tour.editor("definitions"));
        tour.window("Screenpack", "Motif: Backgrounds", tour.editor("backgrounds"));
        if (tour.editor("emulator")) {
            click(tour.view(), "emul_start");
            wait(2500);
            tour.window("Screenpack", "Emulator: title screen");
            click(tour.view(), "emul_start");
        }
        tour.closeAll();
    } else {
        tour.record("Screenpack", "Open data/system.def", false);
    }
    if (window.openProject(engine + "/data/fight.def")) {
        wait(1500);
        tour.window("Screenpack", "Fight screen: Definitions", tour.editor("definitions"));
        if (tour.editor("emulator")) {
            click(tour.view(), "emul_start");
            wait(4500);
            tour.window("Screenpack", "Emulator: the fight screen");
            click(tour.view(), "emul_start");
        }
        tour.closeAll();
    } else {
        tour.record("Screenpack", "Open data/fight.def", false);
    }
    if (window.openProject(engine + "/chars/kfm/intro.def")) {
        wait(1500);
        tour.window("Storyboard", "Storyboard: Backgrounds", tour.editor("backgrounds"));
        if (tour.editor("emulator")) {
            click(tour.view(), "emul_start");
            wait(3000);
            tour.window("Storyboard", "Emulator: the storyboard playing");
            click(tour.view(), "emul_start");
        }
        tour.closeAll();
    } else {
        tour.record("Storyboard", "Open kfm intro.def", false);
    }
    const QString font = QFileInfo::exists(engine + "/font/f-6x9.def") ? engine + "/font/f-6x9.def" : QString();
    if (!font.isEmpty() && window.openProject(font)) {
        wait(1200);
        tour.window("Font", "Font project");
        tour.closeAll();
    } else {
        tour.record("Font", "Open a font", false);
    }

    // ---- An SFF v2 character (true-color sprites) -------------------------------------------------
    if (parser.isSet("sffv2")) {
        const QString copy = work.filePath("sffv2");
        copyFolder(QDir(parser.value("sffv2")).absolutePath(), copy);
        const QStringList defs = QDir(copy).entryList({"*.def"}, QDir::Files);
        if (!defs.isEmpty() && window.openProject(QDir(copy).filePath(defs.first()))) {
            wait(1200);
            tour.window("Character (SFF v2)", "Sprites editor", tour.editor("sprites"));
            tour.closeAll();
        }
    }

    tour.window("Start", "All projects closed");
    tour.save();
    qInfo() << tour.failures() << "step(s) failed";
    window.close();
    return 0;
}
