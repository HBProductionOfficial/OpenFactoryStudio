#include "engines/mugen/MugenRun.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "engines/mugen/MugenEngine.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTextCursor>
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

} // namespace

class MugenRunTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void character()
    {
        QTemporaryDir work;
        const QString engine = work.filePath("mugen");
        const QString project = work.filePath("project");
        writeFile(engine + "/chars/ofs_0/existing.txt", "someone else's folder");
        writeFile(project + "/hero.def",
                  "[Info]\r\nname = \"Hero\"\r\nmugenversion = 1.0\r\n[Files]\r\ncmd = hero.cmd\r\n"
                  "cns = hero.cns\r\nsprite = hero.sff\r\nanim = hero.air\r\nstcommon = common1.cns\r\n");
        writeFile(project + "/hero.cmd", "[Command]\r\nname = \"x\"\r\ncommand = x\r\n");
        writeFile(project + "/hero.cns", "[Data]\r\nlife = 1000\r\n");
        writeFile(project + "/hero.sff", QByteArray("ElecbyteSpr\0fake", 16));
        writeFile(project + "/hero.air", "[Begin Action 0]\r\n0,0, 0,0, 10\r\n");

        Mugen::MugenEngine mugen;
        QString error;
        std::unique_ptr<Project> p(mugen.openProject(project + "/hero.def", &error));
        QVERIFY2(p, qPrintable(error));
        CodeFile* cns = p->codeFile(project + "/hero.cns", "states");
        QVERIFY(cns);
        QTextCursor(cns->document()).insertText("; edited\n");

        const RunCopy copy = mugen.prepareRun(*p, engine);
        QVERIFY2(copy.isValid(), qPrintable(copy.error));
        QCOMPARE(copy.name, QStringLiteral("ofs_1")); // ofs_0 is taken
        const QString dir = engine + "/chars/ofs_1";
        QCOMPARE(readFile(dir + "/ofs_1.def"), readFile(project + "/hero.def"));
        QCOMPARE(readFile(dir + "/hero.def"), readFile(project + "/hero.def"));
        QCOMPARE(readFile(dir + "/hero.sff"), readFile(project + "/hero.sff"));
        QCOMPARE(readFile(dir + "/hero.cns"), QByteArray("; edited\r\n[Data]\r\nlife = 1000\r\n"));
        QCOMPARE(readFile(project + "/hero.cns"), QByteArray("[Data]\r\nlife = 1000\r\n"));
        QVERIFY(cns->isModified());
        QVERIFY(!QFileInfo::exists(dir + "/common1.cns"));

        copy.remove();
        QVERIFY(!QFileInfo::exists(dir));
        QVERIFY(QFileInfo::exists(engine + "/chars/ofs_0/existing.txt"));
        cns->document()->setModified(false);
    }

    void stage()
    {
        QTemporaryDir work;
        const QString engine = work.filePath("mugen");
        const QString project = work.filePath("project");
        writeFile(project + "/arena.def", "[Info]\r\nname = \"Arena\"\r\n[Camera]\r\nstartx = 0\r\n[BGdef]\r\n"
                                          "spr = stages/arena.sff\r\n"
                                          "[BG 1]\r\ntype = normal\r\n[Music]\r\nbgmusic = sound/arena.mp3\r\n");
        writeFile(project + "/stages/arena.sff", "sprites");
        writeFile(engine + "/stages/other.def", "keep");

        Mugen::MugenEngine mugen;
        QString error;
        std::unique_ptr<Project> p(mugen.openProject(project + "/arena.def", &error));
        QVERIFY2(p, qPrintable(error));
        QCOMPARE(p->typeId(), QStringLiteral("stage"));

        RunCopy copy = mugen.prepareRun(*p, engine);
        QVERIFY2(copy.isValid(), qPrintable(copy.error));
        QCOMPARE(copy.name, QStringLiteral("ofs_0"));
        QCOMPARE(readFile(engine + "/stages/ofs_0.def"), readFile(project + "/arena.def"));
        QCOMPARE(readFile(engine + "/stages/arena.sff"), QByteArray("sprites"));
        copy.remove();
        QVERIFY(!QFileInfo::exists(engine + "/stages/arena.sff"));
        QVERIFY(!QFileInfo::exists(engine + "/stages/ofs_0.def"));
        QVERIFY(QFileInfo::exists(engine + "/stages/other.def"));

        writeFile(engine + "/stages/arena.sff", "sprites");
        copy = mugen.prepareRun(*p, engine);
        QVERIFY(copy.isValid());
        QVERIFY(!copy.files.contains(engine + "/stages/arena.sff"));
        copy.remove();
        QVERIFY(QFileInfo::exists(engine + "/stages/arena.sff"));

        writeFile(engine + "/stages/arena.sff", "another stage's sprites");
        copy = mugen.prepareRun(*p, engine);
        QVERIFY(!copy.isValid());
        QVERIFY(copy.error.contains("arena.sff"));
        QCOMPARE(readFile(engine + "/stages/arena.sff"), QByteArray("another stage's sprites"));
        QVERIFY(!QFileInfo::exists(engine + "/stages/ofs_0.def"));
    }
};

QTEST_MAIN(MugenRunTest)
#include "MugenRunTest.moc"
