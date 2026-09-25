// Tests for creating, opening, saving and closing projects, and the project tools.

#include "GuiTestSupport.h"
#include "core/actions/ActionRegistry.h"
#include "core/code/CodeEditor.h"
#include "core/project/Project.h"
#include "core/project/ProjectBackup.h"
#include "core/project/ProjectFile.h"
#include "core/project/ZipArchive.h"
#include "core/sprites/SpriteDocument.h"
#include "core/sprites/SpriteFile.h"
#include "shell/AnimationPage.h"
#include "shell/AnimationsEditor.h"
#include "shell/FileDialogs.h"
#include "shell/MainWindow.h"
#include "shell/ProjectView.h"
#include "shell/SpritePage.h"
#include "shell/SpritesEditor.h"
#include "shell/StatusBar.h"
#include "shell/WelcomePage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCommandLinkButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QToolButton>
#include <QTreeView>
#include <QUndoStack>
#include <QWizard>

using namespace OFS;
using namespace OFS::GuiTest;

namespace {

void chooseEmptyTemplate()
{
    whenModal<QDialog>([](QDialog* dialog) {
        QCOMPARE(dialog->objectName(), QStringLiteral("ProjDlg"));
        auto* list = qobject_cast<QListWidget*>(dialog->findChild<QTabWidget*>("tabs")->widget(0));
        list->setCurrentRow(0);
        dialog->findChild<QDialogButtonBox*>("buttonBox")->button(QDialogButtonBox::Ok)->click();
    });
}

void answerEmptyWizard(const QString& choice)
{
    whenModal<QWizard>([choice](QWizard* wizard) {
        for (QRadioButton* radio : wizard->currentPage()->findChildren<QRadioButton*>()) {
            if (radio->text() == choice)
                radio->setChecked(true);
        }
        wizard->button(QWizard::FinishButton)->click();
    });
}

void writeCharacter(const QString& dir, const QString& base, const QString& name)
{
    QDir().mkpath(dir);
    writeFile(dir + '/' + base + ".def", QStringLiteral("[Info]\nname = \"%1\"\nmugenversion = 1.0\n[Files]\n"
                                                        "cmd = %2.cmd\ncns = %2.cns\nst = %2.cns\n")
                                             .arg(name, base)
                                             .toUtf8());
    writeFile(dir + '/' + base + ".cmd", "[Command]\nname = \"x\"\ncommand = x\n");
    writeFile(dir + '/' + base + ".cns", "[Data]\nlife = 1000\n");
}

// A sprite file with one 2x2 sprite per (group, number, x axis).
QByteArray spriteFile(const QList<std::array<int, 3>>& sprites)
{
    SpriteDocument doc;
    Formats::Palette colors(256);
    colors[1] = Formats::Rgba{200, 100, 50, 255};
    doc.palettes() << SpriteDocument::makePalette(1, 1, colors);
    for (const auto& [group, number, axis] : sprites) {
        SpriteEntry entry;
        entry.header.group = std::uint16_t(group);
        entry.header.number = std::uint16_t(number);
        entry.header.axisX = std::int16_t(axis);
        entry.pixels =
            SpriteDocument::makePixels(Formats::Image::indexed(2, 2, std::vector<std::uint8_t>(4, 1), colors));
        entry.palette = doc.palettes().first();
        doc.sprites() << entry;
    }
    const auto bytes = doc.toSff().write();
    return QByteArray(reinterpret_cast<const char*>(bytes.value().data()), qsizetype(bytes.value().size()));
}

CodeFile* codeFile(ProjectView* view, const QString& path)
{
    return qobject_cast<CodeFile*>(view->project()->file(QDir::fromNativeSeparators(path)));
}

void edit(CodeFile* file, const QString& line)
{
    QTextCursor cursor(file->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(line);
}

// Clears unsaved changes so closing the window asks nothing.
void dropChanges(MainWindow& window)
{
    for (ProjectView* view : window.projectViews()) {
        for (ProjectFile* file : view->project()->files()) {
            if (auto* code = qobject_cast<CodeFile*>(file))
                code->document()->setModified(false);
            else if (auto* sprites = qobject_cast<SpriteFile*>(file))
                sprites->undoStack()->setClean();
        }
    }
}

QTabBar* projectTabs(MainWindow& window)
{
    return window.findChild<QTabBar*>("projectTabs");
}

int tabOf(MainWindow& window, ProjectView* view)
{
    QTabBar* tabs = projectTabs(window);
    for (int i = 0; i < tabs->count(); ++i) {
        if (tabs->tabData(i).value<QObject*>() == view)
            return i;
    }
    return -1;
}

QStringList actionTexts(const QMenu* menu)
{
    QStringList texts;
    for (QAction* a : menu->actions()) {
        if (a->isVisible())
            texts << (a->isSeparator() ? QStringLiteral("|") : a->text());
    }
    return texts;
}

QAction* actionNamed(const QMenu* menu, const QString& text)
{
    for (QAction* a : menu->actions()) {
        if (a->text() == text)
            return a;
    }
    return nullptr;
}

} // namespace

class ProjectParityTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        m_context = makeContext();
        Config& config = m_context->config();
        config.setValue("confirm_close", false);
        config.setValue("backup_onload", false);
        config.setValue("autosave_interval", 0);
        config.setValue("showmsg_discard", QString());
        config.setValue("proj_save_path", QString());
        QVERIFY(m_dir.isValid());
        config.setValue("project_path", m_dir.path());
    }

    void cleanupTestCase() { delete m_context; }

    void cleanup()
    {
        FileDialogs::setOpenPromptOverride({});
        FileDialogs::setSavePromptOverride({});
    }

    // A double-click on a template works like OK.
    void templateDoubleClick()
    {
        MainWindow window(*m_context);
        window.show();
        whenModal<QDialog>([](QDialog* dialog) {
            auto* list = qobject_cast<QListWidget*>(dialog->findChild<QTabWidget*>("tabs")->widget(0));
            QListWidgetItem* item = list->item(0);
            list->setCurrentItem(item);
            Q_EMIT list->itemDoubleClicked(item);
        });
        answerEmptyWizard("Character");
        m_context->actions().action("file_new")->trigger();
        QVERIFY(window.currentProjectView());
        QCOMPARE(window.currentProjectView()->project()->name(), QStringLiteral("Char name"));
    }

    void newProjectStatusAndExplorer()
    {
        MainWindow window(*m_context);
        window.show();
        const QList<QPair<QString, QString>> types{{"Character", "Project: Character, M.U.G.E.N 1.0"},
                                                   {"Stage", "Project: Stage, M.U.G.E.N 1.0"},
                                                   {"Font", "Project: Font, DOS M.U.G.E.N beta"},
                                                   {"Storyboard", "Project: Storyboard, DOS M.U.G.E.N beta"}};
        for (const auto& [type, status] : types) {
            chooseEmptyTemplate();
            answerEmptyWizard(type);
            m_context->actions().action("file_new")->trigger();
            ProjectView* view = window.currentProjectView();
            QVERIFY(view);
            QCOMPARE(window.ofsStatusBar()->section(3), status);
            QCOMPARE(view->findChild<QTreeView*>("projectExplorer")->model()->rowCount(), 0);
        }
    }

    void newProjectSave()
    {
        MainWindow window(*m_context);
        window.show();
        chooseEmptyTemplate();
        answerEmptyWizard("Character");
        m_context->actions().action("file_new")->trigger();
        ProjectView* view = window.currentProjectView();
        Project* project = view->project();
        const QString folder = project->directory();
        for (const char* name : {"char.cns", "char.cmd", "char.def"})
            edit(codeFile(view, folder + '/' + name), "; mine\n");
        QTemporaryDir target;
        QStringList asked;
        FileDialogs::setSavePromptOverride([&](const QString& caption, const QString& path, const QString& filter) {
            asked << caption + '|' + filter;
            return target.filePath(QFileInfo(path).fileName());
        });
        m_context->actions().action("file_save")->trigger();
        QCOMPARE(asked, (QStringList{"Save definitions as...|Definitions (*.def);;All files (*.*)",
                                     "Save commands as...|Commands (*.cmd *.*);;All files (*.*)",
                                     "Save states as...|States (*.cns *.st *.*);;All files (*.*)"}));
        QVERIFY(!project->isModified());
        QCOMPARE(project->filePath(), target.filePath("char.def"));
        QCOMPARE(projectTabs(window)->tabText(tabOf(window, view)), QStringLiteral("Char name"));
        QCOMPARE(window.windowTitle(), QStringLiteral("Char name - Open Factory Studio"));
    }

    // Character constants suit a 320x240 localcoord. Fighter Factory's were four times too large.
    void blankTemplates()
    {
        MainWindow window(*m_context);
        window.show();
        auto newProject = [&](const QString& type) {
            chooseEmptyTemplate();
            answerEmptyWizard(type);
            m_context->actions().action("file_new")->trigger();
            return window.currentProjectView();
        };
        auto text = [](ProjectView* view, const QString& editor) {
            const QString path = view->project()->editorFiles(editor).value(0);
            CodeFile* file = codeFile(view, path);
            return file ? file->document()->toPlainText() : QString();
        };
        ProjectView* character = newProject("Character");
        const QString def = text(character, "definitions");
        QVERIFY(def.contains(QRegularExpression("localcoord\\s*=\\s*320,\\s*240")));
        const QString cns = text(character, "states");
        for (const char* section : {"[Data]", "[Size]", "[Velocity]", "[Movement]", "[Quotes]"})
            QVERIFY2(cns.contains(QLatin1String(section)), section);
        QVERIFY(!cns.contains("[Statedef"));
        QVERIFY(cns.contains(QRegularExpression("\nwalk\\.fwd\\s*=\\s*2\\.4\\s")));
        QVERIFY(cns.contains(QRegularExpression("\nheight\\s*=\\s*60\\s")));
        QVERIFY(cns.contains(QRegularExpression("\nlife\\s*=\\s*1000\\s")));

        ProjectView* stage = newProject("Stage");
        const QString stageDef = text(stage, "definitions");
        for (const char* section : {"[Info]", "[Camera]", "[PlayerInfo]", "[Bound]", "[StageInfo]", "[Shadow]",
                                    "[Reflection]", "[Music]", "[BGdef]"})
            QVERIFY2(stageDef.contains(QLatin1String(section)), section);
        QVERIFY(stageDef.contains(QRegularExpression("boundleft\\s*=\\s*-150")));
        QVERIFY(stageDef.contains(QRegularExpression("spr\\s*=\\s*stage\\.sff")));
        QVERIFY(!stageDef.contains(QRegularExpression("^\\[BG ", QRegularExpression::MultilineOption)));

        const QString font = text(newProject("Font"), "definitions");
        QVERIFY(font.contains("[FNT v2]"));
        QVERIFY(font.contains(QRegularExpression("Type\\s*=\\s*truetype")));
        QVERIFY(font.contains(QRegularExpression("Size\\s*=\\s*18,36")));

        const QString story = text(newProject("Storyboard"), "definitions");
        QVERIFY(story.contains("[SceneDef]"));
        QVERIFY(story.contains(QRegularExpression("startscene\\s*=\\s*0")));
        const QString scene = story.mid(story.indexOf("[Scene 0]"));
        QVERIFY(scene.startsWith("[Scene 0]"));
        for (const QString& line : scene.split('\n').mid(1)) {
            if (!line.trimmed().isEmpty())
                QVERIFY2(line.startsWith(';'), qPrintable(line));
        }
    }

    // Two archives holding a definitions file of the same name open in two tabs.
    void archivesByPath()
    {
        QTemporaryDir dir;
        writeCharacter(dir.filePath("one/fighter"), "fighter", "First");
        writeCharacter(dir.filePath("two/fighter"), "fighter", "Second");
        const QStringList entries{"fighter/fighter.def", "fighter/fighter.cmd", "fighter/fighter.cns"};
        QVERIFY(Zip::create(dir.filePath("one.zip"), dir.filePath("one"), entries));
        QVERIFY(Zip::create(dir.filePath("two.zip"), dir.filePath("two"), entries));
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("one.zip")));
        QVERIFY(window.openProject(dir.filePath("two.zip")));
        QCOMPARE(window.projectViews().size(), qsizetype(2));
        QCOMPARE(window.projectViews().at(0)->project()->name(), QStringLiteral("First"));
        QCOMPARE(window.projectViews().at(1)->project()->name(), QStringLiteral("Second"));
        const auto history = m_context->history().entries();
        QCOMPARE(history.at(0).file, QDir::fromNativeSeparators(dir.filePath("two.zip")));
        QCOMPARE(history.at(1).file, QDir::fromNativeSeparators(dir.filePath("one.zip")));
    }

    void archiveSaveAs()
    {
        QTemporaryDir dir;
        writeCharacter(dir.filePath("src/fighter"), "fighter", "Zipped");
        QVERIFY(Zip::create(dir.filePath("pack.zip"), dir.filePath("src"),
                            {"fighter/fighter.def", "fighter/fighter.cmd", "fighter/fighter.cns"}));
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("pack.zip")));
        ProjectView* view = window.currentProjectView();
        edit(codeFile(view, view->project()->editorFiles("commands").value(0)), "; zipped\n");
        QString asked;
        FileDialogs::setSavePromptOverride([&](const QString& caption, const QString&, const QString& filter) {
            asked = caption + '|' + filter;
            return dir.filePath("out/copy.def");
        });
        m_context->actions().action("file_saveas")->trigger();
        QCOMPARE(asked, QStringLiteral("Save project as...|MUGENext project (*.def)"));
        QVERIFY(QFile::exists(dir.filePath("out/copy.def")));
        QVERIFY(readFile(dir.filePath("out/fighter.cmd")).endsWith("; zipped\n"));
        QVERIFY(QFile::exists(dir.filePath("out/fighter.cns")));
        ProjectView* copy = window.currentProjectView();
        QVERIFY(copy);
        QCOMPARE(copy->project()->filePath(), QDir::fromNativeSeparators(dir.filePath("out/copy.def")));
        QVERIFY(copy->project()->archivePath().isEmpty());
        QCOMPARE(window.projectViews().size(), qsizetype(1));
    }

    // A definitions file opened on its own does not load the files it names.
    void singleDefinitionsFile()
    {
        QTemporaryDir dir;
        writeCharacter(dir.path(), "solo", "Solo");
        MainWindow window(*m_context);
        window.show();
        FileDialogs::setOpenPromptOverride(
            [&](const QString&, const QString&, const QString&) { return dir.filePath("solo.def"); });
        m_context->actions().action("file_open_single")->trigger();
        ProjectView* view = window.currentProjectView();
        QVERIFY(view && view->project()->isSingleFile());
        QCOMPARE(view->editorIds(), (QStringList{"definitions", "emulator"}));
        QCOMPARE(view->project()->files().size(), qsizetype(1));
        QVERIFY(!view->project()->file(QDir::fromNativeSeparators(dir.filePath("solo.cmd"))));
        QVERIFY(!view->project()->file(QDir::fromNativeSeparators(dir.filePath("solo.cns"))));
    }

    void singleFileSaveAs()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("one.cmd"), "[Command]\nname = \"x\"\ncommand = x\n");
        MainWindow window(*m_context);
        window.show();
        FileDialogs::setOpenPromptOverride(
            [&](const QString&, const QString&, const QString&) { return dir.filePath("one.cmd"); });
        m_context->actions().action("file_open_single")->trigger();
        ProjectView* view = window.currentProjectView();
        QVERIFY(view);
        view->currentCodeEditor()->insertPlainText("; edited\n");
        QString asked;
        FileDialogs::setSavePromptOverride([&](const QString& caption, const QString&, const QString&) {
            asked = caption;
            return dir.filePath("two.cmd");
        });
        m_context->actions().action("file_saveas")->trigger();
        QCOMPARE(asked, QStringLiteral("Save commands as..."));
        QVERIFY(readFile(dir.filePath("two.cmd")).startsWith("; edited"));
        QCOMPARE(view->fileTabs("commands")->tabText(0), QStringLiteral("two.cmd"));
        QVERIFY(!view->project()->isModified());
    }

    void saveAndCloseAfterSaveAs()
    {
        QTemporaryDir dir;
        writeCharacter(dir.filePath("a"), "hero", "Hero");
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("a/hero.def")));
        FileDialogs::setSavePromptOverride(
            [&](const QString&, const QString&, const QString&) { return dir.filePath("b/hero.def"); });
        m_context->actions().action("file_saveas")->trigger();
        ProjectView* view = window.currentProjectView();
        QCOMPARE(view->project()->filePath(), QDir::fromNativeSeparators(dir.filePath("b/hero.def")));
        edit(codeFile(view, dir.filePath("b/hero.cmd")), "; after\n");
        m_context->actions().action("file_save")->trigger();
        window.waitForTasks();
        QVERIFY(readFile(dir.filePath("b/hero.cmd")).endsWith("; after\n"));
        QVERIFY(!readFile(dir.filePath("a/hero.cmd")).contains("; after"));
        edit(codeFile(view, dir.filePath("b/hero.cns")), "; unsaved\n");
        answerDiscard(QMessageBox::Yes);
        m_context->actions().action("file_close")->trigger();
        QVERIFY(window.projectViews().isEmpty());
    }

    void tabCloseButton()
    {
        QCOMPARE(m_context->actions().action("file_close")->shortcut(), QKeySequence(QStringLiteral("Ctrl+W")));
        QTemporaryDir dir;
        writeCharacter(dir.filePath("a"), "a", "A");
        writeCharacter(dir.filePath("b"), "b", "B");
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("a/a.def")));
        ProjectView* a = window.currentProjectView();
        edit(codeFile(a, dir.filePath("a/a.cmd")), "; changed\n");
        QVERIFY(window.openProject(dir.filePath("b/b.def")));
        ProjectView* b = window.currentProjectView();
        auto closeButton = [&](ProjectView* view) {
            QTabBar* tabs = projectTabs(window);
            auto* button = qobject_cast<QAbstractButton*>(tabs->tabButton(tabOf(window, view), QTabBar::RightSide));
            if (!button)
                button = qobject_cast<QAbstractButton*>(tabs->tabButton(tabOf(window, view), QTabBar::LeftSide));
            return button;
        };
        QVERIFY(closeButton(a));
        QStringList texts;
        answerBoxes({QMessageBox::No}, &texts);
        closeButton(a)->click();
        QCOMPARE(texts, QStringList{"Do you want to discard the changes made in commands?"});
        QCOMPARE(window.projectViews().size(), qsizetype(2));
        answerDiscard(QMessageBox::Yes);
        closeButton(a)->click();
        QCOMPARE(window.projectViews(), QList<ProjectView*>{b});
        QCOMPARE(window.currentProjectView(), b);
        closeButton(b)->click();
        QVERIFY(window.projectViews().isEmpty());
    }

    void closeOthers()
    {
        QTemporaryDir dir;
        for (const char* name : {"a", "b", "c"})
            writeCharacter(dir.filePath(name), name, QString::fromLatin1(name).toUpper());
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("a/a.def")));
        ProjectView* a = window.currentProjectView();
        QVERIFY(window.openProject(dir.filePath("b/b.def")));
        ProjectView* b = window.currentProjectView();
        QVERIFY(window.openProject(dir.filePath("c/c.def")));
        edit(codeFile(a, dir.filePath("a/a.cns")), "; a\n");
        edit(codeFile(b, dir.filePath("b/b.cmd")), "; b\n");
        projectTabs(window)->setCurrentIndex(tabOf(window, a));
        QStringList texts;
        answerBoxes({QMessageBox::No}, &texts);
        m_context->actions().action("file_close_others")->trigger();
        QCOMPARE(texts, QStringList{"Do you want to discard the changes made in commands?"});
        QCOMPARE(window.projectViews(), (QList<ProjectView*>{a, b}));
        texts.clear();
        answerBoxes({QMessageBox::Yes}, &texts);
        m_context->actions().action("file_close_others")->trigger();
        QCOMPARE(texts.size(), qsizetype(1));
        QCOMPARE(window.projectViews(), QList<ProjectView*>{a});
        QVERIFY(a->project()->isModified());
        dropChanges(window);
    }

    void exitCancelledByDiscard()
    {
        QTemporaryDir dir;
        writeCharacter(dir.path(), "x", "X");
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("x.def")));
        edit(codeFile(window.currentProjectView(), dir.filePath("x.cmd")), "; x\n");
        m_context->config().setValue("confirm_close", true);
        QStringList texts;
        answerBoxes({QMessageBox::Yes, QMessageBox::No}, &texts);
        QVERIFY(!window.close());
        m_context->config().setValue("confirm_close", false);
        QCOMPARE(texts, (QStringList{"Do you really want to close Open Factory Studio?",
                                     "Do you want to discard the changes made in commands?"}));
        QVERIFY(window.isVisible());
        QCOMPARE(window.projectViews().size(), qsizetype(1));
        QVERIFY(window.currentProjectView()->project()->isModified());
        dropChanges(window);
    }

    void saveCurrentProjectOnly()
    {
        QTemporaryDir dir;
        writeCharacter(dir.filePath("a"), "a", "A");
        writeCharacter(dir.filePath("b"), "b", "B");
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("a/a.def")));
        ProjectView* a = window.currentProjectView();
        QVERIFY(window.openProject(dir.filePath("b/b.def")));
        ProjectView* b = window.currentProjectView();
        edit(codeFile(a, dir.filePath("a/a.cmd")), "; a\n");
        edit(codeFile(b, dir.filePath("b/b.cmd")), "; b\n");
        m_context->actions().action("file_save")->trigger();
        window.waitForTasks();
        QVERIFY(readFile(dir.filePath("b/b.cmd")).endsWith("; b\n"));
        QVERIFY(!readFile(dir.filePath("a/a.cmd")).contains("; a"));
        QVERIFY(a->project()->isModified());
        QVERIFY(!b->project()->isModified());
        dropChanges(window);
    }

    // Cancel in Load backup changes nothing. Backup on load skips single files but covers archives.
    void backupCancelAndOnLoad()
    {
        QTemporaryDir dir;
        writeCharacter(dir.filePath("p"), "p", "P");
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("p/p.def")));
        ProjectView* view = window.currentProjectView();
        m_context->actions().action("file_backup")->trigger();
        edit(codeFile(view, dir.filePath("p/p.cmd")), "; unsaved\n");
        writeFile(dir.filePath("p/p.cns"), "; changed on disk\n");
        whenModal<QDialog>([](QDialog* dialog) {
            QCOMPARE(dialog->objectName(), QStringLiteral("BackupDlg"));
            dialog->findChild<QListWidget*>("listView")->setCurrentRow(0);
            dialog->findChild<QDialogButtonBox*>("buttonBox")->button(QDialogButtonBox::Cancel)->click();
        });
        m_context->actions().action("file_load_backup")->trigger();
        QCOMPARE(window.currentProjectView(), view);
        QVERIFY(view->project()->isModified());
        QCOMPARE(readFile(dir.filePath("p/p.cns")), QByteArray("; changed on disk\n"));
        dropChanges(window);

        m_context->config().setValue("backup_onload", true);
        QVERIFY(QDir(dir.path()).mkpath("single"));
        writeFile(dir.filePath("single/one.cmd"), "[Command]\nname = \"x\"\ncommand = x\n");
        FileDialogs::setOpenPromptOverride(
            [&](const QString&, const QString&, const QString&) { return dir.filePath("single/one.cmd"); });
        m_context->actions().action("file_open_single")->trigger();
        QVERIFY(window.currentProjectView()->project()->isSingleFile());
        QVERIFY(!QFileInfo::exists(dir.filePath("single/backup")));

        writeCharacter(dir.filePath("src/z"), "z", "Z");
        QVERIFY(QDir(dir.path()).mkpath("arch"));
        QVERIFY(Zip::create(dir.filePath("arch/z.zip"), dir.filePath("src"), {"z/z.def", "z/z.cmd", "z/z.cns"}));
        QVERIFY(window.openProject(dir.filePath("arch/z.zip")));
        m_context->config().setValue("backup_onload", false);
        const QStringList zips = QDir(dir.filePath("arch/backup")).entryList({"*_Auto.zip"});
        QCOMPARE(zips.size(), qsizetype(1));
        QStringList names;
        for (const Zip::Entry& entry : Zip::entries(dir.filePath("arch/backup/" + zips.first())))
            names << QFileInfo(entry.name).fileName();
        names.sort();
        QCOMPARE(names, (QStringList{"z.cmd", "z.cns", "z.def"}));
    }

    // autosave_async makes no difference here. Same as Fighter Factory.
    void autosaveKeepsUndo()
    {
        QTemporaryDir dir;
        writeCharacter(dir.path(), "au", "Auto");
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("au.def")));
        ProjectView* view = window.currentProjectView();
        m_context->actions().action("editor_cmd")->trigger();
        QTabWidget* tabs = view->fileTabs("commands");
        for (const bool asyncOnly : {true, false}) {
            m_context->config().setValue("autosave_async", asyncOnly);
            const QString line = asyncOnly ? "; async\n" : "; any\n";
            view->currentCodeEditor()->moveCursor(QTextCursor::End);
            view->currentCodeEditor()->insertPlainText(line);
            QCOMPARE(tabs->tabText(0), QStringLiteral("au.cmd*"));
            window.autosave();
            QTRY_VERIFY(readFile(dir.filePath("au.cmd")).endsWith(line.toUtf8()));
            QTRY_COMPARE(tabs->tabText(0), QStringLiteral("au.cmd"));
            QVERIFY(!view->project()->isModified());
            QVERIFY(view->currentCodeEditor()->document()->isUndoAvailable());
            QVERIFY(!QApplication::activeModalWidget());
        }
        m_context->config().setValue("autosave_async", true);
    }

    // "Don't show this again" in the reload question stores Yes as Always and No as Never.
    void watcherStoresAnswer()
    {
        QTemporaryDir dir;
        writeCharacter(dir.path(), "fw", "Watched");
        Config& config = m_context->config();
        config.setValue("enable_file_watcher", true);
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("fw.def")));
        CodeFile* commands = codeFile(window.currentProjectView(), dir.filePath("fw.cmd"));
        QTest::qWait(300); // the OS watcher starts a moment after a file is added
        for (const auto& [answer, stored] : {std::pair(QMessageBox::Yes, "1"), std::pair(QMessageBox::No, "2")}) {
            config.setValue("showmsg_reload_file", "0"); // Ask
            bool asked = false;
            const QMessageBox::StandardButton button = answer;
            whenModal<QMessageBox>([&asked, button](QMessageBox* box) {
                asked = true;
                QVERIFY(box->checkBox());
                box->checkBox()->setChecked(true);
                box->button(button)->click();
            });
            const QByteArray text = QByteArray("; ") + stored + '\n';
            writeFile(dir.filePath("fw.cmd"), text);
            QTRY_VERIFY(asked);
            QCOMPARE(config.value("showmsg_reload_file"), QString::fromLatin1(stored));
            if (answer == QMessageBox::Yes)
                QTRY_COMPARE(commands->document()->toPlainText(), QString::fromUtf8(text));
            QTest::qWait(300);
        }
        config.setValue("showmsg_reload_file", "1");
    }

    void watcherReloadsSprites()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("s.def"), "[Info]\nname = \"S\"\nmugenversion = 1.0\n[Files]\nsprite = s.sff\n");
        writeFile(dir.filePath("s.sff"), spriteFile({{0, 0, 18}, {0, 1, 5}}));
        Config& config = m_context->config();
        config.setValue("enable_file_watcher", true);
        config.setValue("showmsg_reload_file", "1"); // Always
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("s.def")));
        window.switchEditor("sprites");
        ProjectView* view = window.currentProjectView();
        auto* axis = view->findChild<QSpinBox*>("spr_xaxis");
        QVERIFY(axis);
        QCOMPARE(axis->value(), 18);
        QTest::qWait(300);
        writeFile(dir.filePath("s.sff"), spriteFile({{0, 0, 19}, {0, 1, 5}}));
        QTRY_COMPARE(view->spritesEditor()->page()->file()->document().sprites().first().header.axisX, 19);
        QCOMPARE(view->spritesEditor()->page()->current(), 0);
        QTRY_COMPARE(axis->value(), 19);
        QVERIFY(!QApplication::activeModalWidget());
    }

    void importFromAnotherProject()
    {
        QTemporaryDir dir;
        for (const char* name : {"a", "b"}) {
            writeFile(dir.filePath(QStringLiteral("%1.def").arg(name)),
                      QStringLiteral("[Info]\nname = \"%1\"\nmugenversion = 1.0\n[Files]\ncns = %2.cns\n"
                                     "anim = %2.air\nsprite = %2.sff\n")
                          .arg(QString::fromLatin1(name).toUpper(), name)
                          .toUtf8());
            writeFile(dir.filePath(QStringLiteral("%1.cns").arg(name)), "[Data]\nlife = 1000\n");
        }
        writeFile(dir.filePath("a.air"), "; Old\n[Begin Action 0]\n0,0, 0,0, 1\n");
        writeFile(dir.filePath("b.air"), "; Walking\n[Begin Action 20]\n20,0, 0,0, 5\n\n"
                                         "; Standing\n[Begin Action 0]\n0,0, 0,0, 10\n");
        writeFile(dir.filePath("a.sff"), spriteFile({{0, 0, 0}}));
        writeFile(dir.filePath("b.sff"), spriteFile({{0, 0, 0}, {20, 0, 3}}));

        MainWindow window(*m_context);
        window.show();
        ActionRegistry& actions = m_context->actions();
        QVERIFY(window.openProject(dir.filePath("b.def")));
        QVERIFY(window.openProject(dir.filePath("a.def")));
        ProjectView* a = window.currentProjectView();
        for (const char* menuName : {"menu_sprites", "menu_animations", "menu_sounds"}) {
            QMenu* m = menu(window, menuName);
            QVERIFY2(m, menuName);
            QVERIFY2(menuIds(m).filter("_import_proj").size() == 1, menuName);
        }
        for (const char* menuName : {"menu_commands", "menu_states", "menu_palettes", "menu_backgrounds"}) {
            QMenu* m = menu(window, menuName);
            QVERIFY2(m, menuName);
            QVERIFY2(menuIds(m).filter("import_proj").isEmpty(), menuName);
        }
        window.switchEditor("animations");
        QCOMPARE(actions.action("anim_import_proj")->shortcut(), QKeySequence("Ctrl+Shift+I"));
        window.switchEditor("sprites");
        QCOMPARE(actions.action("spr_import_proj")->shortcut(), QKeySequence("Ctrl+Shift+I"));
        window.switchEditor("animations");

        auto* anims = codeFile(a, dir.filePath("a.air"));
        auto run = [&](const std::function<void(QDialog*)>& fill, QList<QMessageBox::StandardButton> answers) {
            whenModal<QDialog>(fill);
            QStringList boxes;
            auto* timer = new QTimer(this);
            timer->setInterval(5);
            connect(timer, &QTimer::timeout, this, [timer, &answers, &boxes] {
                auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                if (!box || !box->isVisible() || answers.isEmpty())
                    return;
                boxes << box->windowTitle() + '|' + box->text();
                box->button(answers.takeFirst())->click();
                if (answers.isEmpty())
                    timer->stop();
            });
            timer->start();
            actions.action("anim_import_proj")->trigger();
            timer->stop();
            delete timer;
            return boxes;
        };
        auto ok = [](QDialog* dialog) {
            dialog->findChild<QDialogButtonBox*>("buttonBox")->button(QDialogButtonBox::Ok)->click();
        };

        QStringList boxes = run(
            [&](QDialog* dialog) {
                auto* list = dialog->findChild<QListWidget*>("list");
                auto rows = [list] {
                    QStringList r;
                    for (int i = 0; i < list->count(); ++i)
                        r << list->item(i)->text();
                    return r;
                };
                auto selected = [list] {
                    QStringList r;
                    for (int i = 0; i < list->count(); ++i) {
                        if (list->item(i)->isSelected())
                            r << list->item(i)->text();
                    }
                    return r;
                };
                auto* sorted = dialog->findChild<QCheckBox*>("sorted");
                QCOMPARE(sorted->text(), QStringLiteral("Sort "));
                QVERIFY(!sorted->isChecked());
                QCOMPARE(rows(), (QStringList{"20 - Walking", "0 - Standing"}));
                sorted->setChecked(true);
                QCOMPARE(rows(), (QStringList{"0 - Standing", "20 - Walking"}));
                dialog->findChild<QPushButton*>("allBut")->click();
                QCOMPARE(selected().size(), qsizetype(2));
                dialog->findChild<QPushButton*>("clearBut")->click();
                QVERIFY(selected().isEmpty());
                list->item(0)->setSelected(true);
                dialog->findChild<QPushButton*>("invBut")->click();
                QCOMPARE(selected(), QStringList{"20 - Walking"});
                QCheckBox* sprites = nullptr;
                for (QCheckBox* box : dialog->findChildren<QCheckBox*>()) {
                    if (box->text() == QLatin1String("Import sprites"))
                        sprites = box;
                }
                QVERIFY(sprites && !sprites->isChecked());
                sprites->setChecked(true);
                dialog->findChild<QPushButton*>("addBut")->click();
                auto* destination = dialog->findChild<QListWidget*>("destList");
                QCOMPARE(destination->count(), 1);
                destination->item(0)->setSelected(true);
                dialog->findChild<QPushButton*>("delBut")->click();
                QCOMPARE(destination->count(), 0);
                dialog->findChild<QPushButton*>("addBut")->click();
                dialog->findChild<QRadioButton*>("asIs")->setChecked(true);
                ok(dialog);
            },
            {QMessageBox::Ok});
        QCOMPARE(boxes, QStringList{boxTitle("Import from another project...") + "|1 animation(s) were imported!"});
        QVERIFY(anims->document()->toPlainText().contains("[Begin Action 20]"));
        SpriteFile* sprites =
            qobject_cast<SpriteFile*>(a->project()->file(QDir::fromNativeSeparators(dir.filePath("a.sff"))));
        QVERIFY(sprites);
        QVERIFY(sprites->document().findSprite(20, 0) >= 0);
        QCOMPARE(sprites->document().spriteCount(), 2);

        const QString before = anims->document()->toPlainText();
        boxes = run(
            [&](QDialog* dialog) {
                QVERIFY(!dialog->findChild<QCheckBox*>("sorted")->isChecked());
                QCOMPARE(dialog->findChild<QListWidget*>("destList")->count(), 0);
                QVERIFY(dialog->findChild<QRadioButton*>("ask")->isChecked());
                for (QCheckBox* box : dialog->findChildren<QCheckBox*>())
                    QVERIFY(!box->isChecked());
                auto* list = dialog->findChild<QListWidget*>("list");
                list->item(1)->setSelected(true);
                dialog->findChild<QPushButton*>("addBut")->click();
                dialog->findChild<QRadioButton*>("skip")->setChecked(true);
                ok(dialog);
            },
            {QMessageBox::Ok});
        QCOMPARE(boxes, QStringList{boxTitle("Import from another project...") + "|No animation imported!"});
        QCOMPARE(anims->document()->toPlainText(), before);

        boxes = run(
            [&](QDialog* dialog) {
                dialog->findChild<QListWidget*>("list")->item(1)->setSelected(true);
                dialog->findChild<QPushButton*>("addBut")->click();
                ok(dialog);
            },
            {QMessageBox::Yes, QMessageBox::Ok});
        QCOMPARE(boxes, (QStringList{boxTitle("Conflict resolution!") +
                                         "|Animation '0' already exists, do you want to replace it?",
                                     boxTitle("Import from another project...") + "|1 animation(s) were imported!"}));
        const QString replaced = anims->document()->toPlainText();
        QVERIFY(replaced.contains("0,0, 0,0, 10"));
        QVERIFY(!replaced.contains("0,0, 0,0, 1\n"));
        QCOMPARE(replaced.count("[Begin Action 0]"), 1);
        QVERIFY(replaced.indexOf("[Begin Action 0]") < replaced.indexOf("[Begin Action 20]"));

        boxes = run(
            [&](QDialog* dialog) {
                dialog->findChild<QListWidget*>("list")->item(1)->setSelected(true);
                dialog->findChild<QPushButton*>("addBut")->click();
                dialog->findChild<QRadioButton*>("asIs")->setChecked(true);
                ok(dialog);
            },
            {QMessageBox::Ok});
        QCOMPARE(boxes, QStringList{boxTitle("Import from another project...") + "|1 animation(s) were imported!"});
        QCOMPARE(anims->document()->toPlainText().count("[Begin Action 0]"), 2);

        bool checked = false;
        whenModal<QDialog>([&checked](QDialog* dialog) {
            QCOMPARE(dialog->objectName(), QStringLiteral("ProjImportDlg"));
            for (QCheckBox* box : dialog->findChildren<QCheckBox*>())
                QVERIFY(box->text() != QLatin1String("Import sprites"));
            checked = true;
            dialog->reject();
        });
        actions.action("spr_import_proj")->trigger();
        QVERIFY(checked);
        dropChanges(window);
    }

    // "!ff" in the exported page marks a placeholder that was not filled.
    void exportToHtml()
    {
        QTemporaryDir dir;
        writeCharacter(dir.path(), "h", "Html");
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("h.def")));
        QAction* action = m_context->actions().action("tools_export_html");
        for (int round = 0; round < 2; ++round) {
            whenModal<QDialog>([round](QDialog* dialog) {
                QCOMPARE(dialog->objectName(), QStringLiteral("ExpHtmlDlg"));
                auto* mail = dialog->findChild<QLineEdit*>("mail");
                auto* site = dialog->findChild<QLineEdit*>("site");
                QVERIFY(mail && site);
                if (round == 1) {
                    QVERIFY(mail->text().isEmpty());
                    QVERIFY(site->text().isEmpty());
                    for (QCheckBox* box : dialog->findChildren<QCheckBox*>()) {
                        if (box->objectName() == QLatin1String("hline") || box->objectName() == QLatin1String("bold"))
                            QVERIFY(!box->isChecked());
                    }
                    dialog->reject();
                    return;
                }
                mail->setText("me@example.com");
                site->setText("example.com");
                for (QCheckBox* box : dialog->findChildren<QCheckBox*>()) {
                    if (box->objectName() == QLatin1String("hline") || box->objectName() == QLatin1String("bold"))
                        box->setChecked(true);
                }
                dialog->reject();
            });
            action->trigger();
        }

        chooseEmptyTemplate();
        answerEmptyWizard("Storyboard");
        m_context->actions().action("file_new")->trigger();
        ProjectView* story = window.currentProjectView();
        QCOMPARE(story->project()->typeId(), QStringLiteral("storyboard"));
        FileDialogs::setSavePromptOverride(
            [&](const QString&, const QString&, const QString&) { return dir.filePath("out/story.html"); });
        whenModal<QDialog>([](QDialog* dialog) {
            dialog->findChild<QLineEdit*>("mail")->setText("me@example.com");
            dialog->findChild<QDialogButtonBox*>("buttonBox")->button(QDialogButtonBox::Ok)->click();
        });
        action->trigger();
        const QString page = QString::fromUtf8(readFile(dir.filePath("out/story.html")));
        QVERIFY(!page.isEmpty());
        QVERIFY(page.contains("me@example.com"));
        QVERIFY(!page.contains("!ff"));
        QVERIFY(page.contains(QStringLiteral("<title>%1</title>").arg(story->project()->name())));
    }

    void openRecent()
    {
        QTemporaryDir dir;
        writeCharacter(dir.filePath("a"), "a", "Alpha");
        writeCharacter(dir.filePath("b"), "b", "Beta");
        m_context->history().clear();
        MainWindow window(*m_context);
        window.show();
        QMenu* recent = menu(window, "menu_open_recent");
        QVERIFY(recent);
        Q_EMIT recent->aboutToShow();
        QCOMPARE(actionTexts(recent), (QStringList{"|", "Open all", "Clear"}));
        QVERIFY(recent->menuAction()->isEnabled());
        QVERIFY(actionNamed(recent, "Open all")->isEnabled());
        QVERIFY(actionNamed(recent, "Clear")->isEnabled());

        m_context->history().add("Alpha", QDir::fromNativeSeparators(dir.filePath("a/a.def")));
        m_context->history().add("Beta", QDir::fromNativeSeparators(dir.filePath("b/b.def")));
        Q_EMIT recent->aboutToShow();
        QCOMPARE(actionTexts(recent), (QStringList{"Beta\t" + QDir::toNativeSeparators(dir.filePath("b/b.def")),
                                                   "Alpha\t" + QDir::toNativeSeparators(dir.filePath("a/a.def")), "|",
                                                   "Open all", "Clear"}));

        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QTest::qWait(20);
        auto* welcome = window.findChild<WelcomePage*>();
        auto* first = welcome->findChild<QCommandLinkButton*>("recentProject1");
        auto* second = welcome->findChild<QCommandLinkButton*>("recentProject2");
        QVERIFY(first && second);
        QCOMPARE(first->text(), QStringLiteral("Beta"));
        QCOMPARE(first->description(), QDir::toNativeSeparators(dir.filePath("b/b.def")));
        QCOMPARE(second->text(), QStringLiteral("Alpha"));
        QCOMPARE(first->y(), second->y());
        QVERIFY(first->x() < second->x());

        actionNamed(recent, "Open all")->trigger();
        QCOMPARE(window.projectViews().size(), qsizetype(2));
        QCOMPARE(window.projectViews().at(0)->project()->name(), QStringLiteral("Alpha"));
        QCOMPARE(window.currentProjectView()->project()->name(), QStringLiteral("Beta"));
        QCOMPARE(m_context->history().entries().at(0).name, QStringLiteral("Beta"));
        QCOMPARE(m_context->history().entries().at(1).name, QStringLiteral("Alpha"));

        Q_EMIT recent->aboutToShow();
        actionNamed(recent, "Clear")->trigger();
        QVERIFY(!QApplication::activeModalWidget());
        QVERIFY(m_context->history().entries().isEmpty());
        window.closeAllProjects();
        QTRY_VERIFY(!welcome->findChild<QCommandLinkButton*>("recentProject1"));
        QVERIFY(welcome->findChild<QCommandLinkButton*>("welcomeOpenProject"));
    }

    void welcomeOpens()
    {
        QTemporaryDir dir;
        writeCharacter(dir.path(), "w", "Welcomed");
        m_context->history().clear();
        m_context->history().add("Welcomed", QDir::fromNativeSeparators(dir.filePath("w.def")));
        MainWindow window(*m_context);
        window.show();
        auto* button = window.findChild<WelcomePage*>()->findChild<QCommandLinkButton*>("recentProject1");
        QVERIFY(button);
        button->click();
        QVERIFY(window.currentProjectView());
        QCOMPARE(window.currentProjectView()->project()->name(), QStringLiteral("Welcomed"));
        m_context->history().clear();
    }

private:
    AppContext* m_context = nullptr;
    QTemporaryDir m_dir;
};

QTEST_MAIN(ProjectParityTest)
#include "ProjectParityTest.moc"
