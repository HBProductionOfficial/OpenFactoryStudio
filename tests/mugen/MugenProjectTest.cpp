#include "engines/mugen/MugenProject.h"
#include "engines/mugen/DefFile.h"
#include "engines/mugen/MugenEngine.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace OFS;
using namespace OFS::Mugen;

namespace {

DefFile parse(const char* text)
{
    DefFile def;
    def.parse(QString::fromUtf8(text));
    return def;
}

void writeFile(const QString& path, const QByteArray& data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

} // namespace

class MugenProjectTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defFileParsing()
    {
        const DefFile def = parse("; comment\n"
                                  "[Info]\r\n"
                                  "name = \"Kung; Fu\" ; trailing comment\n"
                                  "Author=Someone\n"
                                  "[Files]\n"
                                  "st = a.cns\n"
                                  "st1 = b.cns\n"
                                  "ST2 = c.cns\n"
                                  "stcommon = common1.cns\n");
        QCOMPARE(def.value("info", "NAME"), QStringLiteral("Kung; Fu"));
        QCOMPARE(def.value("Info", "author"), QStringLiteral("Someone"));
        QCOMPARE(def.values("Files", "st[0-9]+"), (QStringList{"b.cns", "c.cns"}));
        QVERIFY(def.hasSection("FILES"));
        QVERIFY(!def.hasSection("Missing"));
        QCOMPARE(def.value("Missing", "x", "fallback"), QStringLiteral("fallback"));
    }

    void typeDetection_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("type");
        QTest::newRow("char") << "[Info]\nname=a\n[Files]\ncns=a.cns\n" << "char";
        QTest::newRow("stage") << "[Info]\nname=a\n[Camera]\nstartx=0\n[BGdef]\nspr=a.sff\n" << "stage";
        QTest::newRow("storyboard") << "[SceneDef]\nspr=a.sff\n[Scene 0]\n" << "storyboard";
        QTest::newRow("motif") << "[Info]\nname=a\n[Files]\nspr=s.sff\n[Title Info]\n" << "motif";
        QTest::newRow("fight") << "[Files]\nsff=fight.sff\n[FightFX]\nscale=1\n[Lifebar]\n" << "fight";
        QTest::newRow("select") << "[Characters]\nkfm\n[ExtraStages]\n" << "select";
        QTest::newRow("font v2") << "[FNT v2]\nfntversion=2,00\n[Def]\nType=bitmap\n" << "font";
    }

    void typeDetection()
    {
        QFETCH(QString, text);
        QFETCH(QString, type);
        DefFile def;
        def.parse(text);
        QCOMPARE(MugenProject::detectType(def), type);
    }

    void versionDetection()
    {
        QCOMPARE(MugenProject::detectVersion(parse("[Info]\nmugenversion = 1.0\n")), QStringLiteral("1.0"));
        QCOMPARE(MugenProject::detectVersion(parse("[Info]\nmugenversion = 1.1\n")), QStringLiteral("1.1"));
        QCOMPARE(MugenProject::detectVersion(parse("[Info]\nmugenversion = 04.14.2001\n")), QStringLiteral("winbeta"));
        QCOMPARE(MugenProject::detectVersion(parse("[Info]\nname = x\n")), QStringLiteral("dosbeta"));
    }

    void editorsPerType()
    {
        QCOMPARE(MugenProject::editorsForType("char"),
                 (QStringList{"definitions", "sprites", "animations", "commands", "states", "sounds", "emulator"}));
        QCOMPARE(MugenProject::editorsForType("stage"),
                 (QStringList{"definitions", "sprites", "animations", "backgrounds", "emulator"}));
        QCOMPARE(MugenProject::editorsForType("motif"),
                 (QStringList{"definitions", "sprites", "animations", "backgrounds", "sounds", "emulator"}));
        QCOMPARE(MugenProject::editorsForType("fight"),
                 (QStringList{"definitions", "sprites", "animations", "sounds", "emulator"}));
        QCOMPARE(MugenProject::editorsForType("font"), (QStringList{"definitions", "emulator"}));
    }

    void characterFiles()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("c.def"), "[Info]\nname = \"Hero\"\nmugenversion = 1.1\n"
                                         "[Files]\ncmd = c.cmd\ncns = c.cns\nst = c.cns\nst1 = moves/extra.st\n"
                                         "sprite = c.sff\nanim = c.air\nsound = c.snd\n");
        MugenEngine engine;
        QString error;
        std::unique_ptr<Project> project(engine.openProject(dir.filePath("c.def"), &error));
        QVERIFY2(project, qPrintable(error));
        QCOMPARE(project->name(), QStringLiteral("Hero"));
        QCOMPARE(project->typeId(), QStringLiteral("char"));
        QCOMPARE(project->typeName(), QStringLiteral("Character"));
        QCOMPARE(project->versionName(), QStringLiteral("M.U.G.E.N 1.1"));
        const QString base = QDir::fromNativeSeparators(dir.path());
        QCOMPARE(project->editorFiles("states"), (QStringList{base + "/c.cns", base + "/moves/extra.st"}));
        QCOMPARE(project->editorFiles("sprites"), QStringList{base + "/c.sff"});
        QCOMPARE(project->editorFiles("definitions"), QStringList{base + "/c.def"});
    }

    void unnamedProjectUsesFileName()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("fight.def"), "[Files]\nsff = fight.sff\nfightfx.sff = fightfx.sff\n"
                                             "fightfx.air = fightfx.air\nsnd = fight.snd\ncommon.snd = common.snd\n"
                                             "[FightFX]\nscale = 1\n");
        MugenEngine engine;
        QString error;
        std::unique_ptr<Project> project(engine.openProject(dir.filePath("fight.def"), &error));
        QVERIFY(project);
        QCOMPARE(project->name(), QStringLiteral("fight"));
        QCOMPARE(project->typeName(), QStringLiteral("Fight HUD & effects"));
        QCOMPARE(project->editorFiles("sprites").size(), 2);
        QCOMPARE(project->editorFiles("animations").size(), 2);
        QCOMPARE(project->editorFiles("sounds").size(), 2);
    }

    void missingFileFails()
    {
        MugenEngine engine;
        QString error;
        QVERIFY(!engine.openProject(QStringLiteral("does/not/exist.def"), &error));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(MugenProjectTest)
#include "MugenProjectTest.moc"
