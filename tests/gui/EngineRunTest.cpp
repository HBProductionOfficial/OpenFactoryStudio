// Runs Kung Fu Man in real engines. debugWinMugen needs PauseOnDefocus = 0 in WinMUGEN's mugen.cfg.

#include "core/app/AppContext.h"
#include "core/code/CodeEditor.h"
#include "core/debug/Breakpoints.h"
#include "core/engine/EngineDebugSession.h"
#include "core/engine/RunConfiguration.h"
#include "core/settings/Config.h"
#include "shell/MainWindow.h"
#include "shell/ProjectView.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>
#include <QTextEdit>

using namespace OFS;

class EngineRunTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void runInEngines()
    {
        const QStringList engines = qEnvironmentVariable("OFS_TEST_ENGINES").split(';', Qt::SkipEmptyParts);
        const QString kfm = qEnvironmentVariable("OFS_CORPUS_KFM");
        if (engines.isEmpty() || kfm.isEmpty())
            QSKIP("Set OFS_TEST_ENGINES and OFS_CORPUS_KFM to run Kung Fu Man in real engines.");

        QTemporaryDir project;
        QDirIterator files(kfm, QDir::Files);
        while (files.hasNext()) {
            const QString file = files.next();
            QVERIFY(QFile::copy(file, project.filePath(QFileInfo(file).fileName())));
        }
        AppContext context;
        context.loadSettings();
        context.initializeEngines();
        Config& engineConfig = context.engineConfig("mugen");
        context.config().setValue("confirm_close", false);
        for (const QString& exe : engines) {
            QVERIFY2(QFileInfo(exe).isFile(), qPrintable(exe));
            const QDir engineDir = QFileInfo(exe).absoluteDir();
            MainWindow window(context);
            window.show();
            QVERIFY(window.openProject(project.filePath("kfm.def")));
            RunConfiguration::save(engineConfig, {{"Engine", QDir::toNativeSeparators(exe), "", "%0 kfm"}});
            window.runInEngine();
            QVERIFY(window.isEngineRunning());
            QVERIFY2(QFileInfo::exists(engineDir.filePath("chars/ofs_0/kfm.def")), qPrintable(exe));
            // Wait a few seconds so a load error or a crash would stop the engine.
            QTest::qWait(6000);
            QVERIFY2(window.isEngineRunning(), qPrintable(exe + ": the engine stopped early"));
            auto* process = window.findChild<QProcess*>();
            QVERIFY(process);
            process->kill();
            QTRY_VERIFY_WITH_TIMEOUT(!window.isEngineRunning(), 15000);
            QVERIFY2(!QFileInfo::exists(engineDir.filePath("chars/ofs_0")), qPrintable(exe));
            QVERIFY(QFileInfo::exists(engineDir.filePath("chars")));
            qInfo("%s: ran and was cleaned up", qPrintable(exe));
        }
        RunConfiguration::save(engineConfig, {});
    }

    void debugWinMugen()
    {
        const QString exe = qEnvironmentVariable("OFS_TEST_WINMUGEN");
        if (exe.isEmpty())
            QSKIP("Set OFS_TEST_WINMUGEN to debug Kung Fu Man in a real WinMUGEN.");
#ifndef Q_OS_WIN
        QSKIP("The external debugger reads the engine's memory on Windows only.");
#endif
        QVERIFY2(QFileInfo(exe).isFile(), qPrintable(exe));
        const QDir engineDir = QFileInfo(exe).absoluteDir();
        QTemporaryDir project;
        QDirIterator files(engineDir.filePath("chars/kfm"), QDir::Files);
        while (files.hasNext()) {
            const QString file = files.next();
            QVERIFY(QFile::copy(file, project.filePath(QFileInfo(file).fileName())));
        }
        AppContext context;
        context.loadSettings();
        context.initializeEngines();
        context.config().setValue("confirm_close", false);
        Config& engineConfig = context.engineConfig("mugen");
        MainWindow window(context);
        window.show();
        QVERIFY(window.openProject(project.filePath("kfm.def")));
        RunConfiguration::save(engineConfig, {{"WinMUGEN", QDir::toNativeSeparators(exe), "", "%0 kfm"}});
        ProjectView* view = window.currentProjectView();
        EngineDebugSession* session = view->engineDebugSession();
        QVERIFY(session);
        auto value = [session](const char* path) {
            const std::optional<Debug::Variable> v = session->debugVariable(QLatin1String(path));
            return v ? v->value : QString();
        };
        // Break in Kung Fu Man's intro state, which P1 enters as the round starts.
        const QString states = QDir::cleanPath(project.filePath("kfm.cns"));
        QFile cns(states);
        QVERIFY(cns.open(QIODevice::ReadOnly));
        const QList<QByteArray> lines = cns.readAll().split('\n');
        int intro = 0;
        for (int i = 0; i < lines.size() && !intro; ++i) {
            if (lines[i].trimmed().startsWith("[Statedef 191]"))
                intro = i + 1;
        }
        QVERIFY(intro > 0);
        auto* breakpoints = window.findChild<Debug::BreakpointList*>();
        QVERIFY(breakpoints);
        breakpoints->toggle(states, intro + 3);

        window.startOnEngine();
        QCOMPARE(session->debugState(), Debug::SessionState::Running);
        QVERIFY(QFileInfo::exists(engineDir.filePath("chars/ofs_0/kfm.def")));
        QTRY_COMPARE_WITH_TIMEOUT(session->debugState(), Debug::SessionState::Interrupted, 20000);
        QCOMPARE(view->currentCodeEditor()->currentDebugLine(), intro - 1);
        QCOMPARE(value("game.pause"), QStringLiteral("1"));
        QCOMPARE(value("32.stateno"), QStringLiteral("191"));
        QCOMPARE(value("32.life"), QStringLiteral("1000"));
        qInfo("stopped at [Statedef 191] (line %d), game time %s, P1 %s, P2 %s", intro, qPrintable(value("game.time")),
              qPrintable(session->debugVariables().value(1).name), qPrintable(session->debugVariables().value(2).name));
        // Let the engine finish the frame it had already started before reading the time.
        QTest::qWait(200);
        const QString pausedAt = value("game.time");
        QTest::qWait(500);
        QCOMPARE(value("game.time"), pausedAt);
        for (int i = 1; i <= 10; ++i) {
            session->perform(QStringLiteral("debug_step_frame"));
            QTest::qWait(50);
            QCOMPARE(value("game.time").toInt(), pausedAt.toInt() + i);
        }
        QCOMPARE(session->debugState(), Debug::SessionState::Interrupted);

        breakpoints->removeAll();
        session->perform(QStringLiteral("debug_continue"));
        QCOMPARE(session->debugState(), Debug::SessionState::Running);
        QTest::qWait(1000);
        QVERIFY(value("game.time").toInt() > pausedAt.toInt() + 30);
        QCOMPARE(value("game.pause"), QStringLiteral("0"));

        session->perform(QStringLiteral("debug_interrupt"));
        QCOMPARE(session->debugState(), Debug::SessionState::Interrupted);
        QTest::qWait(200);
        const QString interruptedAt = value("game.time");
        QTest::qWait(500);
        QCOMPARE(value("game.time"), interruptedAt);
        qInfo("interrupted at game time %s: P1 state %s, pos x %s, P2 pos x %s", qPrintable(interruptedAt),
              qPrintable(value("32.stateno")), qPrintable(value("32.pos x")), qPrintable(value("33.pos x")));
        session->perform(QStringLiteral("debug_continue"));
        QTest::qWait(500);
        QVERIFY(value("game.time").toInt() > interruptedAt.toInt());

        session->perform(QStringLiteral("debug_stop"));
        QCOMPARE(session->debugState(), Debug::SessionState::None);
        QTRY_VERIFY(!QFileInfo::exists(engineDir.filePath("chars/ofs_0")));
        RunConfiguration::save(engineConfig, {});
    }
};

QTEST_MAIN(EngineRunTest)
#include "EngineRunTest.moc"
