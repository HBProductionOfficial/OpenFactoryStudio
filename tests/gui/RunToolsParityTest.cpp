// Run in engine, run configurations and the States editor tools.

#include "GuiTestSupport.h"
#include "core/actions/ActionRegistry.h"
#include "core/engine/RunConfiguration.h"
#include "core/settings/ShortcutSettings.h"
#include "shell/BottomDock.h"
#include "shell/FileDialogs.h"
#include "shell/MainWindow.h"
#include "shell/dialogs/MugenOptionsPage.h"
#include "shell/dialogs/RunConfigDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QTemporaryDir>
#include <QTextEdit>

using namespace OFS;
using namespace OFS::GuiTest;

namespace {

#ifdef Q_OS_WIN
const QString ExeSuffix = QStringLiteral(".exe");
#else
const QString ExeSuffix;
#endif

QString fakeEngine(const QString& folder)
{
    QDir().mkpath(folder + QStringLiteral("/chars"));
    const QString exe = folder + QStringLiteral("/mugen") + ExeSuffix;
    QFile::copy(QCoreApplication::applicationDirPath() + QStringLiteral("/fake_mugen") + ExeSuffix, exe);
    return QDir::toNativeSeparators(exe);
}

} // namespace

class RunToolsParityTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        m_context = makeContext();
        QVERIFY(m_dir.isValid());
        writeFile(m_dir.filePath("r.def"), "[Info]\nname = \"R\"\nmugenversion = 1.0\n"
                                           "[Files]\ncmd = r.cmd\ncns = r.cns\nst = r.cns\n");
        writeFile(m_dir.filePath("r.cmd"), "[Command]\nname = \"x\"\ncommand = x\n");
        writeFile(m_dir.filePath("r.cns"), "[Data]\nlife = 1000\n[Statedef 0]\ntype = S\n"
                                           "[State 0, 1]\ntype = VarSet\ntrigger1 = 1\nv = 3\nvalue = var(4)\n");
    }

    void cleanupTestCase()
    {
        RunConfiguration::save(m_context->engineConfig("mugen"), {});
        delete m_context;
    }

    void runConfigDialog()
    {
        RunConfigDialog dialog;
        dialog.show();
        QStringList captions;
        QStringList filters;
        FileDialogs::setOpenPromptOverride([&](const QString& caption, const QString&, const QString& filter) {
            captions << caption;
            filters << filter;
            return captions.size() == 1 ? QStringLiteral("C:/engines/mugen.exe") : QStringLiteral("C:/engines/m.png");
        });
        dialog.findChild<QAbstractButton*>("exe_browse")->click();
        QCOMPARE(captions.last(), QStringLiteral("Executable"));
        QVERIFY2(filters.last().contains(QStringLiteral("MUGENext")), qPrintable(filters.last()));
        QVERIFY(!dialog.findChild<QLineEdit*>("eng_exe")->text().isEmpty());
        QVERIFY(dialog.findChild<QLineEdit*>("eng_name")->text().isEmpty());
        QVERIFY(dialog.findChild<QLineEdit*>("eng_icon")->text().isEmpty());
        dialog.findChild<QAbstractButton*>("icon_browse")->click();
        QCOMPARE(captions.last(), QStringLiteral("Icon"));
        QVERIFY2(filters.last().contains(QStringLiteral("*.png")), qPrintable(filters.last()));
        FileDialogs::setOpenPromptOverride({});
    }

    void runConfigurationList()
    {
        Config& engineConfig = m_context->engineConfig("mugen");
        RunConfiguration::save(engineConfig,
                               {{"One", "C:/a/mugen.exe", "", "%0"}, {"Two", "C:/b/mugen.exe", "", "%0"}});
        MugenOptionsPage page(engineConfig);
        page.load();
        page.show();
        auto* list = page.findChild<QListWidget*>("engineList");
        QVERIFY(list);
        QCOMPARE(list->count(), 2);
        bool opened = false;
        QTimer::singleShot(50, this, [&opened] {
            if (QWidget* modal = QApplication::activeModalWidget()) {
                opened = true;
                modal->close();
            }
        });
        Q_EMIT list->itemDoubleClicked(list->item(0));
        QTest::qWait(100);
        QVERIFY(!opened);
        m_context->config().setValue("confirm_delete", false);
        page.findChild<QPushButton*>("engClear")->click();
        QCOMPARE(list->count(), 0);
        RunConfiguration::save(engineConfig, {});
    }

    // With several configurations Run in Engine asks which one to use and remembers the last one.
    void runChooser()
    {
        QTemporaryDir engines;
        const QString first = fakeEngine(engines.filePath("one"));
        const QString second = fakeEngine(engines.filePath("two"));
        Config& engineConfig = m_context->engineConfig("mugen");
        RunConfiguration::save(engineConfig, {{"First", first, "", "%0"}, {"Second", second, "", "%0"}});
        m_context->config().setValue("last_MUGENext", QString());
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("r.def")));

        int preselected = -1;
        whenModal<QDialog>([&preselected](QDialog* chooser) {
            QCOMPARE(chooser->objectName(), QStringLiteral("RunDlg"));
            QCOMPARE(chooser->windowTitle(), QStringLiteral("Run in Engine"));
            auto* list = chooser->findChild<QListWidget*>("list");
            QVERIFY(list);
            QCOMPARE(list->viewMode(), QListView::IconMode);
            QCOMPARE(list->count(), 2);
            preselected = list->currentRow();
            list->setCurrentRow(1);
            chooser->findChild<QDialogButtonBox*>("buttonBox")->button(QDialogButtonBox::Ok)->click();
        });
        window.runInEngine();
        QCOMPARE(preselected, 0);
        QCOMPARE(m_context->config().intValue("last_MUGENext", -1), 1);
        QTRY_VERIFY_WITH_TIMEOUT(!window.isEngineRunning(), 15000);
        QVERIFY(QFileInfo::exists(engines.filePath("two/mugen.log")));

        whenModal<QDialog>([&preselected](QDialog* chooser) {
            preselected = chooser->findChild<QListWidget*>("list")->currentRow();
            chooser->reject();
        });
        QFile::remove(engines.filePath("one/mugen.log"));
        QFile::remove(engines.filePath("two/mugen.log"));
        window.runInEngine();
        QCOMPARE(preselected, 1);
        QVERIFY(!window.isEngineRunning());
        QVERIFY(!QFileInfo::exists(engines.filePath("one/mugen.log")));
        QVERIFY(!QFileInfo::exists(engines.filePath("two/mugen.log")));
        RunConfiguration::save(engineConfig, {});
    }

    // An engine in a folder with spaces runs. A second run while it runs is refused.
    void runDetails()
    {
        QTemporaryDir engines;
        const QString exe = fakeEngine(engines.filePath("my engine folder"));
        Config& engineConfig = m_context->engineConfig("mugen");
        RunConfiguration::save(engineConfig, {{"Spaced", exe, "", "%0 kfm"}});
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("r.def")));
        auto* dock = window.findChild<BottomDock*>();
        QVERIFY(dock);
        dock->showPage(0); // Issues
        dock->setExpanded(false);

        qputenv("OFS_FAKE_ENGINE_WAIT_MS", "1500");
        window.runInEngine();
        QVERIFY(window.isEngineRunning());
        QStringList texts;
        answerBoxes({QMessageBox::Ok}, &texts);
        window.runInEngine();
        QCOMPARE(texts, QStringList{"The engine is already running."});
        qunsetenv("OFS_FAKE_ENGINE_WAIT_MS");
        QTRY_VERIFY_WITH_TIMEOUT(!window.isEngineRunning(), 15000);
        auto* output = window.findChild<QTextEdit*>("engineOutput");
        QVERIFY(output);
        QVERIFY2(output->toPlainText().contains("Arguments: ofs_0 kfm"), qPrintable(output->toPlainText()));
        QVERIFY2(output->toPlainText().contains("my engine folder"), qPrintable(output->toPlainText()));
        QCOMPARE(dock->currentPage(), 0);
        QVERIFY(!dock->isExpanded());
        RunConfiguration::save(engineConfig, {});
    }

    void toolShortcuts()
    {
        ActionRegistry& actions = m_context->actions();
        QVERIFY(!actions.action("file_run_mugen"));
        for (const QString& id : actions.ids())
            QVERIFY2(!actions.action(id)->shortcuts().contains(QKeySequence(QStringLiteral("Ctrl+M"))), qPrintable(id));
        // Editor keys are only set on the actions while their editor shows, so read the settings.
        QCOMPARE(m_context->shortcuts().keys("st_throw"), QKeySequence(QStringLiteral("Alt+T")));
        QCOMPARE(m_context->shortcuts().keys("st_offset"), QKeySequence(QStringLiteral("Alt+O")));
        QCOMPARE(m_context->shortcuts().defaultKeys("st_throw"), QKeySequence(QStringLiteral("Alt+T")));
    }

    // The variable log options go back to their defaults each time the dialog opens.
    void variableLogDefaults()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("r.def")));
        m_context->actions().action("editor_st")->trigger();
        QList<bool> first;
        QList<bool> second;
        auto states = [](QDialog* dialog) {
            QList<bool> s;
            for (const char* name : {"radioButton", "radioButton_2", "radioButton_3"})
                s << dialog->findChild<QRadioButton*>(name)->isChecked();
            for (const char* name : {"checkBox", "checkBox_2", "checkBox_3", "checkBox_4"})
                s << dialog->findChild<QCheckBox*>(name)->isChecked();
            return s;
        };
        whenModal<QDialog>([&](QDialog* dialog) {
            QCOMPARE(dialog->objectName(), QStringLiteral("VarDlg"));
            first = states(dialog);
            dialog->findChild<QRadioButton*>("radioButton_3")->setChecked(true);
            for (QCheckBox* box : dialog->findChildren<QCheckBox*>())
                box->setChecked(!box->isChecked());
            dialog->findChild<QDialogButtonBox*>("buttonBox")->button(QDialogButtonBox::Ok)->click();
        });
        m_context->actions().action("st_var_log")->trigger();
        whenModal<QDialog>([&](QDialog* dialog) {
            second = states(dialog);
            dialog->reject();
        });
        m_context->actions().action("st_var_log")->trigger();
        QVERIFY(!first.isEmpty());
        QCOMPARE(second, first);
    }

private:
    AppContext* m_context = nullptr;
    QTemporaryDir m_dir;
};

QTEST_MAIN(RunToolsParityTest)
#include "RunToolsParityTest.moc"
