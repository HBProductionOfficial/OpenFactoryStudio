#include "core/app/AppContext.h"
#include "core/engine/RunConfiguration.h"
#include "core/settings/Config.h"
#include "shell/MainWindow.h"

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
};

QTEST_MAIN(EngineRunTest)
#include "EngineRunTest.moc"
