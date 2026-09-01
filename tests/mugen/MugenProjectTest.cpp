#include "engines/mugen/MugenProject.h"
#include "engines/mugen/DefFile.h"
#include "engines/mugen/MugenEngine.h"

#include "core/project/ProjectFile.h"
#include "core/sounds/SoundFile.h"
#include "core/sprites/SpriteFile.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextDocument>

#include <memory>

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
        QCOMPARE(MugenProject::editorsForType("storyboard"),
                 (QStringList{"definitions", "sprites", "animations", "backgrounds", "emulator"}));
    }

    void motifFiles()
    {
        QTemporaryDir dir;
        QVERIFY(QDir(dir.path()).mkpath("data/mine"));
        writeFile(dir.filePath("data/select.def"), "[Characters]\nkfm\n");
        writeFile(dir.filePath("data/mine/system.def"), "[Info]\nname = \"Mine\"\nmugenversion = 1.0\n"
                                                        "[Files]\nspr = system.sff\nsnd = system.snd\n"
                                                        "select = select.def\n[Title Info]\n[TitleBGdef]\n");
        MugenEngine engine;
        QString error;
        std::unique_ptr<Project> project(engine.openProject(dir.filePath("data/mine/system.def"), &error));
        QVERIFY2(project, qPrintable(error));
        QCOMPARE(project->typeId(), QStringLiteral("motif"));
        const QString base = QDir::fromNativeSeparators(dir.path());
        QCOMPARE(project->editorFiles("definitions"),
                 (QStringList{base + "/data/mine/system.def", base + "/data/select.def"}));
        QCOMPARE(project->editorFiles("backgrounds"), QStringList{base + "/data/mine/system.def"});
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

    void newCharacter()
    {
        MugenEngine engine;
        QTemporaryDir folder;
        const QString dir = folder.path();
        QString error;
        std::unique_ptr<Project> project(engine.newProject("char", dir, &error));
        QVERIFY2(project, qPrintable(error));
        QVERIFY(project->isMemoryOnly());
        QCOMPARE(project->name(), QStringLiteral("Char name"));
        QCOMPARE(project->typeId(), QStringLiteral("char"));
        QCOMPARE(project->versionId(), QStringLiteral("1.0"));
        QCOMPARE(project->filePath(), dir + "/char.def");
        QCOMPARE(project->editorIds(),
                 (QStringList{"definitions", "sprites", "animations", "commands", "states", "sounds", "emulator"}));
        QCOMPARE(project->editorFiles("commands"), QStringList{dir + "/char.cmd"});
        QCOMPARE(project->editorFiles("states"), QStringList{dir + "/char.cns"});
        QCOMPARE(project->editorFiles("sprites"), QStringList{dir + "/char.sff"});

        QCOMPARE(project->files().size(), qsizetype(6));
        for (const ProjectFile* f : project->files()) {
            QVERIFY(f->isUntitled());
            QVERIFY(f->isNameless());
            QVERIFY(!f->isModified());
        }
        QVERIFY(!project->isModified());
        QVERIFY(QDir(dir).isEmpty());
        auto* definitions = qobject_cast<CodeFile*>(project->file(dir + "/char.def"));
        auto* commands = qobject_cast<CodeFile*>(project->file(dir + "/char.cmd"));
        auto* states = qobject_cast<CodeFile*>(project->file(dir + "/char.cns"));
        auto* anims = qobject_cast<CodeFile*>(project->file(dir + "/char.air"));
        QVERIFY(definitions && commands && states && anims);
        QVERIFY(definitions->document()->toPlainText().contains("name         = \"Char name\""));
        QVERIFY(commands->document()->toPlainText().trimmed().endsWith("[Statedef -1]"));
        QVERIFY(states->document()->toPlainText().contains("[Velocity]"));
        QVERIFY(anims->document()->isEmpty());
        QVERIFY(!definitions->document()->isUndoAvailable());
        auto* sprites = qobject_cast<SpriteFile*>(project->file(dir + "/char.sff"));
        QVERIFY(sprites && sprites->document().sprites().empty());
        QVERIFY(qobject_cast<SoundFile*>(project->file(dir + "/char.snd")));

        QTemporaryDir target;
        QSignalSpy moved(project.get(), &Project::filePathChanged);
        QVERIFY(definitions->saveAs(target.filePath("mychar.def"), &error));
        QCOMPARE(moved.count(), 1);
        QVERIFY(!project->isMemoryOnly());
        QCOMPARE(project->filePath(), target.path() + "/mychar.def");
        QCOMPARE(project->editorFiles("definitions"), QStringList{target.path() + "/mychar.def"});
        QCOMPARE(project->editorFiles("commands"), QStringList{target.path() + "/char.cmd"});
        QVERIFY(commands->isUntitled() && commands->isNameless());
        QVERIFY(!definitions->isNameless());
        QFile saved(target.filePath("mychar.def"));
        QVERIFY(saved.open(QIODevice::ReadOnly));
        const QByteArray bytes = saved.readAll();
        QVERIFY(bytes.startsWith("\r\n;"));
        QVERIFY(bytes.contains("\r\n[Files]\r\n"));
        QVERIFY(!bytes.contains("\n\n"));

        QSignalSpy renamed(commands, &ProjectFile::pathChanged);
        QVERIFY(commands->saveAs(target.filePath("char.cmd"), &error));
        QCOMPARE(renamed.count(), 1);
        QVERIFY(!commands->isUntitled() && !commands->isNameless());
    }

    void newProjectTypes()
    {
        MugenEngine engine;
        QTemporaryDir folder;
        QString error;
        std::unique_ptr<Project> stage(engine.newProject("stage", folder.path(), &error));
        QVERIFY2(stage, qPrintable(error));
        QCOMPARE(stage->name(), QStringLiteral("Stage name"));
        QCOMPARE(stage->editorIds(), (QStringList{"definitions", "sprites", "animations", "backgrounds", "emulator"}));
        QCOMPARE(stage->editorFiles("backgrounds"), QStringList{stage->filePath()});
        QCOMPARE(stage->files().size(), qsizetype(2));

        std::unique_ptr<Project> font(engine.newProject("font", folder.path(), &error));
        QVERIFY2(font, qPrintable(error));
        QVERIFY(font->name().startsWith("Project "));
        QCOMPARE(font->versionId(), QStringLiteral("dosbeta"));
        QCOMPARE(font->editorIds(), (QStringList{"definitions", "sprites", "emulator"}));
        QCOMPARE(font->editorFiles("sprites"), QStringList{folder.path() + "/font.sff"});
        std::unique_ptr<Project> story(engine.newProject("storyboard", folder.path(), &error));
        QVERIFY2(story, qPrintable(error));
        const int fontNumber = font->name().mid(8).toInt();
        QCOMPARE(story->name(), QStringLiteral("Project %1").arg(fontNumber + 1));
        QCOMPARE(story->editorIds(), (QStringList{"definitions", "sprites", "animations", "backgrounds", "emulator"}));
        QCOMPARE(story->filePath(), folder.path() + "/intro.def");

        QVERIFY(!engine.newProject("select", folder.path(), &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(engine.scriptName(), QStringLiteral("MUGEN"));
        QCOMPARE(engine.scriptEnums().value("ProjectType").value("Character").toString(), QStringLiteral("char"));
    }

    void openedAsCopy()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("tpl.def"), "[Info]\nname = \"Template\"\nmugenversion = 1.0\n[Files]\ncmd = tpl.cmd\n");
        writeFile(dir.filePath("tpl.cmd"), "[Command]\nname = \"x\"\ncommand = x\n");
        MugenEngine engine;
        QString error;
        std::unique_ptr<Project> project(engine.openProject(dir.filePath("tpl.def"), &error));
        QVERIFY2(project, qPrintable(error));
        project->openAsCopy();
        QVERIFY(project->isMemoryOnly());
        CodeFile* commands = project->codeFile(dir.filePath("tpl.cmd"), "commands", &error);
        QVERIFY(commands);
        QVERIFY(commands->isUntitled() && commands->isNameless() && commands->isCopy());
        QVERIFY(commands->isModified());
        QVERIFY(project->isModified());

        QTemporaryDir target;
        QSignalSpy modified(commands, &ProjectFile::modifiedChanged);
        QVERIFY(commands->saveAs(target.filePath("mine.cmd"), &error));
        QVERIFY(!commands->isModified() && !commands->isCopy());
        QVERIFY(!modified.isEmpty());
        QCOMPARE(modified.last().first().toBool(), false);
        QFile original(dir.filePath("tpl.cmd"));
        QVERIFY(original.open(QIODevice::ReadOnly));
        QCOMPARE(original.readAll(), QByteArray("[Command]\nname = \"x\"\ncommand = x\n"));
    }

    void missingFileFails()
    {
        MugenEngine engine;
        QString error;
        QVERIFY(!engine.openProject(QStringLiteral("does/not/exist.def"), &error));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_MAIN(MugenProjectTest)
#include "MugenProjectTest.moc"
