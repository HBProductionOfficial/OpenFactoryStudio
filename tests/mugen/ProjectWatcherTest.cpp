#include "core/project/ProjectWatcher.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "engines/mugen/MugenEngine.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextDocument>

#include <memory>

using namespace OFS;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

} // namespace

class ProjectWatcherTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void changesOnDisk()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("w.def"), "[Info]\nname = \"W\"\n[Files]\ncmd = w.cmd\ncns = w.cns\n");
        writeFile(dir.filePath("w.cmd"), "[Command]\nname = \"x\"\ncommand = x\n");
        writeFile(dir.filePath("w.cns"), "[Data]\nlife = 1000\n");
        Mugen::MugenEngine engine;
        QString error;
        std::unique_ptr<Project> project(engine.openProject(dir.filePath("w.def"), &error));
        QVERIFY2(project, qPrintable(error));
        CodeFile* commands = project->codeFile(dir.filePath("w.cmd"), "commands", &error);
        ProjectWatcher watcher(project.get());
        CodeFile* states = project->codeFile(dir.filePath("w.cns"), "states", &error);
        QVERIFY(commands && states);
        QSignalSpy changed(&watcher, &ProjectWatcher::fileChanged);
        QTest::qWait(1000); // the system's watching starts a moment after a file is added (later under load)

        writeFile(dir.filePath("w.cmd"), "; changed elsewhere\n");
        QTRY_COMPARE(changed.count(), 1);
        QCOMPARE(changed.first().first().value<ProjectFile*>(), commands);
        QTest::qWait(400);
        QCOMPARE(changed.count(), 1);
        writeFile(dir.filePath("w.cns"), "[Data]\nlife = 2\n");
        QTRY_COMPARE(changed.count(), 2);
        QCOMPARE(changed.last().first().value<ProjectFile*>(), states);

        states->document()->setPlainText("[Data]\nlife = 3\n");
        QVERIFY2(states->save(&error), qPrintable(error));
        QTest::qWait(400);
        QCOMPARE(changed.count(), 2);
        writeFile(dir.filePath("w.cns"), "[Data]\nlife = 4\n");
        QTRY_COMPARE(changed.count(), 3);

        watcher.setEnabled(false);
        writeFile(dir.filePath("w.cmd"), "; while off\n");
        QTest::qWait(400);
        QCOMPARE(changed.count(), 3);
        watcher.setEnabled(true);
        QTest::qWait(1000);
        QCOMPARE(changed.count(), 3);
        writeFile(dir.filePath("w.cmd"), "; on again\n");
        QTRY_COMPARE(changed.count(), 4);
    }
};

QTEST_MAIN(ProjectWatcherTest)
#include "ProjectWatcherTest.moc"
