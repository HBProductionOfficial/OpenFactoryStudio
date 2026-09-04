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
        QVERIFY(!definitions->isModified());
        QVERIFY(states->saveAs(target.filePath("sub/mine.cns"), &error));
        const QString text = definitions->document()->toPlainText();
        QVERIFY(text.contains("cns      = sub/mine.cns          ; Constants"));
        QVERIFY(text.contains("st       = sub/mine.cns          ; States"));
        QVERIFY(text.contains("cmd      = char.cmd"));
        QVERIFY(definitions->isModified());
        definitions->document()->undo();
        QVERIFY(definitions->document()->toPlainText().contains("cns      = char.cns"));
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

    void writeCopyTo()
    {
        QTemporaryDir dir;
        const QByteArray def = "[Info]\nname = \"C\"\nmugenversion = 1.0\n[Files]\ncmd = c.cmd\ncns = sub/c.cns\n"
                               "stcommon = common1.cns\nsprite = ../outside.sff\n";
        writeFile(dir.filePath("c.def"), def);
        writeFile(dir.filePath("c.cmd"), "[Command]\nname = \"x\"\ncommand = x\n");
        writeFile(dir.filePath("sub/c.cns"), "[Data]\nlife = 1000\n");
        MugenEngine engine;
        QString error;
        std::unique_ptr<Project> project(engine.openProject(dir.filePath("c.def"), &error));
        QVERIFY2(project, qPrintable(error));
        CodeFile* commands = project->codeFile(dir.filePath("c.cmd"), "commands", &error);
        QVERIFY(commands);
        commands->document()->setPlainText("[Command]\nname = \"y\"\ncommand = y\n");

        QTemporaryDir target;
        QVERIFY2(project->writeCopyTo(target.filePath("new/n.def"), &error), qPrintable(error));
        QCOMPARE(readFile(target.filePath("new/n.def")), def);
        QCOMPARE(readFile(target.filePath("new/c.cmd")), QByteArray("[Command]\nname = \"y\"\ncommand = y\n"));
        QCOMPARE(readFile(target.filePath("new/sub/c.cns")), QByteArray("[Data]\nlife = 1000\n"));
        QVERIFY(!QFile::exists(target.filePath("outside.sff")));
        QVERIFY(!QFile::exists(target.filePath("new/c.def")));
        QVERIFY(commands->isModified());
        QCOMPARE(project->filePath(), dir.filePath("c.def"));
        QCOMPARE(readFile(dir.filePath("c.cmd")), QByteArray("[Command]\nname = \"x\"\ncommand = x\n"));

        std::unique_ptr<Project> blank(engine.newProject("char", dir.filePath("blank"), &error));
        QVERIFY2(blank, qPrintable(error));
        QVERIFY2(blank->writeCopyTo(target.filePath("b/mine.def"), &error), qPrintable(error));
        for (const char* name : {"mine.def", "char.cmd", "char.cns", "char.air", "char.sff", "char.snd"})
            QVERIFY2(QFile::exists(target.filePath(QStringLiteral("b/") + name)), name);
        QVERIFY(readFile(target.filePath("b/mine.def")).contains("\r\n[Files]\r\n"));
    }

    void singleFiles()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("x.cmd"), "[Command]\nname = \"x\"\ncommand = x\n");
        writeFile(dir.filePath("x.txt"), "text");
        MugenEngine engine;
        QString error;
        std::unique_ptr<Project> project(engine.openSingleFile(dir.filePath("x.cmd"), &error));
        QVERIFY2(project, qPrintable(error));
        QVERIFY(project->isSingleFile());
        QVERIFY(!project->isMemoryOnly());
        QVERIFY(project->name().startsWith("Project "));
        QCOMPARE(project->typeId(), QStringLiteral("char"));
        QCOMPARE(project->versionId(), QStringLiteral("1.1"));
        QCOMPARE(project->editorIds(), (QStringList{"commands", "emulator"}));
        QCOMPARE(project->editorFiles("commands"), QStringList{dir.filePath("x.cmd")});
        QCOMPARE(MugenProject::singleFileEditor("A.ST"), QStringLiteral("states"));
        QCOMPARE(MugenProject::singleFileEditor("kfm.def"), QStringLiteral("definitions"));
        QVERIFY(!engine.openSingleFile(dir.filePath("x.txt"), &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!engine.openSingleFile(dir.filePath("missing.sff"), &error));
    }

    void archiveProjects()
    {
        QTemporaryDir extracted(QDir::tempPath() + "/ofs-test-XXXXXX");
        extracted.setAutoRemove(false);
        writeFile(extracted.filePath("a.def"), "[Info]\nname = \"A\"\n[Files]\ncmd = a.cmd\ncns = a.cns\n");
        writeFile(extracted.filePath("a.cmd"), "[Command]\nname = \"x\"\ncommand = x\n");
        writeFile(extracted.filePath("a.cns"), "[Data]\nlife = 1000\n");
        MugenEngine engine;
        QString error;
        std::unique_ptr<Project> project(engine.openProject(extracted.filePath("a.def"), &error));
        QVERIFY2(project, qPrintable(error));
        CodeFile* commands = project->codeFile(extracted.filePath("a.cmd"), "commands", &error);
        project->setArchive("C:/somewhere/pack.zip", extracted.path());
        CodeFile* states = project->codeFile(extracted.filePath("a.cns"), "states", &error);
        QVERIFY(commands && states);
        QVERIFY(commands->isUntitled() && states->isUntitled());
        QVERIFY(!commands->isNameless() && !commands->isModified());
        QCOMPARE(project->archivePath(), QStringLiteral("C:/somewhere/pack.zip"));
        QCOMPARE(project->suggestedPath(commands), QStringLiteral("C:/somewhere/a.cmd"));
        project.reset();
        QVERIFY(!QFileInfo::exists(extracted.path()));
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
