#include "core/project/ProjectBackup.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/project/ZipArchive.h"
#include "engines/mugen/MugenEngine.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTextDocument>

#include <memory>

using namespace OFS;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QByteArray readFile(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

const QByteArray kDef = "[Info]\nname = \"C\"\nmugenversion = 1.0\n[Files]\ncmd = c.cmd\ncns = sub/c.cns\n";
const QByteArray kCmd = "[Command]\nname = \"x\"\ncommand = x\n";
const QByteArray kCns = "[Data]\nlife = 1000\n";

} // namespace

class ProjectBackupTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void names()
    {
        const QDate day(2026, 10, 4);
        QCOMPARE(Backup::fileName(QDateTime(day, QTime(22, 32, 40)), false),
                 QStringLiteral("04.10.2026_10.32.40_pm_.zip"));
        QCOMPARE(Backup::fileName(QDateTime(day, QTime(1, 35, 3)), false), QStringLiteral("04.10.2026_1.35.3_am_.zip"));
        QCOMPARE(Backup::fileName(QDateTime(day, QTime(12, 0, 5)), true),
                 QStringLiteral("04.10.2026_12.0.5_pm_Auto.zip"));
        QCOMPARE(Backup::fileName(QDateTime(day, QTime(0, 7, 0)), false), QStringLiteral("04.10.2026_12.7.0_am_.zip"));

        bool automatic = false;
        QCOMPARE(Backup::timeOf("04.10.2026_10.32.40_pm_.zip", &automatic), QDateTime(day, QTime(22, 32, 40)));
        QVERIFY(!automatic);
        QCOMPARE(Backup::timeOf("04.10.2026_12.0.5_pm_Auto.zip", &automatic), QDateTime(day, QTime(12, 0, 5)));
        QVERIFY(automatic);
        QCOMPARE(Backup::timeOf("04.10.2026_12.7.0_am_", &automatic), QDateTime(day, QTime(0, 7, 0)));
        QCOMPARE(Backup::timeOf("04.10.2026_1.2.3_am_2.zip"), QDateTime(day, QTime(1, 2, 3)));
        QVERIFY(!Backup::timeOf("backup.zip").isValid());
        QVERIFY(!Backup::timeOf("31.02.2026_1.2.3_am_.zip").isValid());
    }

    void createListRestore()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("c.def"), kDef);
        writeFile(dir.filePath("c.cmd"), kCmd);
        writeFile(dir.filePath("sub/c.cns"), kCns);
        writeFile(dir.filePath("readme.txt"), "not part of the project");
        Mugen::MugenEngine engine;
        QString error;
        std::unique_ptr<Project> project(engine.openProject(dir.filePath("c.def"), &error));
        QVERIFY2(project, qPrintable(error));
        CodeFile* commands = project->codeFile(dir.filePath("c.cmd"), "commands", &error);
        QVERIFY(commands);
        commands->document()->setPlainText("[Command]\nname = \"y\"\ncommand = y\n");
        QCOMPARE(Backup::folder(*project), dir.filePath("backup"));

        const QDateTime first(QDate(2026, 10, 4), QTime(22, 32, 40));
        const QString zip = Backup::create(*project, first, false, &error);
        QVERIFY2(!zip.isEmpty(), qPrintable(error));
        QCOMPARE(zip, dir.filePath("backup/04.10.2026_10.32.40_pm_.zip"));
        QStringList names;
        for (const Zip::Entry& entry : Zip::entries(zip))
            names << entry.name;
        QCOMPARE(names, (QStringList{"c.cmd", "c.def", "sub/c.cns"}));
        QCOMPARE(Zip::read(zip, "c.def"), kDef);
        QCOMPARE(Zip::read(zip, "c.cmd"), QByteArray("[Command]\nname = \"y\"\ncommand = y\n"));
        QCOMPARE(Zip::read(zip, "sub/c.cns"), kCns);
        QVERIFY(commands->isModified());
        QCOMPARE(readFile(dir.filePath("c.cmd")), kCmd);

        const QString again = Backup::create(*project, first, false, &error);
        QCOMPARE(again, dir.filePath("backup/04.10.2026_10.32.40_pm_2.zip"));
        QVERIFY(!Backup::create(*project, QDateTime(QDate(2026, 10, 5), QTime(9, 0, 0)), true, &error).isEmpty());
        QDir().mkpath(dir.filePath("backup/03.10.2026_9.5.7_am_"));
        writeFile(dir.filePath("backup/03.10.2026_9.5.7_am_/c.cmd"), "; from the folder\n");
        QDir().mkpath(dir.filePath("backup/mine"));
        writeFile(dir.filePath("backup/notes.txt"), "x");

        const QList<Backup::Item> items = Backup::list(*project);
        QStringList labels;
        for (const Backup::Item& item : items)
            labels << item.label;
        QCOMPARE(labels, (QStringList{"03/10/2026 9:5:7 am", "04/10/2026 10:32:40 pm", "04/10/2026 10:32:40 pm",
                                      "05/10/2026 9:0:0 am (automatic)", "Unknown"}));

        writeFile(dir.filePath("sub/c.cns"), "[Data]\nlife = 1\n");
        QVERIFY2(Backup::restore(*project, zip, &error), qPrintable(error));
        QCOMPARE(readFile(dir.filePath("sub/c.cns")), kCns);
        QCOMPARE(readFile(dir.filePath("c.cmd")), QByteArray("[Command]\nname = \"y\"\ncommand = y\n"));
        QVERIFY2(Backup::restore(*project, items.first().path, &error), qPrintable(error));
        QCOMPARE(readFile(dir.filePath("c.cmd")), QByteArray("; from the folder\n"));
        QVERIFY(!Backup::restore(*project, dir.filePath("backup/missing.zip"), &error));
    }

    void projectsWithoutFolders()
    {
        Mugen::MugenEngine engine;
        QTemporaryDir dir;
        QString error;
        std::unique_ptr<Project> blank(engine.newProject("char", dir.path(), &error));
        QVERIFY2(blank, qPrintable(error));
        QVERIFY(Backup::create(*blank, QDateTime::currentDateTime(), false, &error).isEmpty());
        QVERIFY(!error.isEmpty());
        QVERIFY(!QFileInfo::exists(dir.filePath("backup")));

        writeFile(dir.filePath("x/c.def"), kDef);
        std::unique_ptr<Project> archived(engine.openProject(dir.filePath("x/c.def"), &error));
        QVERIFY2(archived, qPrintable(error));
        archived->setArchive(dir.filePath("packs/c.zip"), QString());
        QCOMPARE(Backup::folder(*archived), dir.filePath("packs/backup"));
    }
};

QTEST_MAIN(ProjectBackupTest)
#include "ProjectBackupTest.moc"
