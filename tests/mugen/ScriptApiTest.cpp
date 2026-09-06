#include "core/engine/EngineManager.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/script/ScriptHost.h"
#include "engines/mugen/MugenEngine.h"
#include "fixtures/FixtureFactory.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTextDocument>

#include <memory>

using namespace OFS;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(data);
}

const char* kAir =
    "; Standing\n[Begin Action 0]\nClsn2Default: 1\n Clsn2[0] = -10,-60, 10,0\n0,0, 0,0, 5\n0,1, 2,-1, 7, H\n\n"
    "[Begin Action 200]\nClsn1: 1\n Clsn1[0] = 5,-50, 40,-40\n200,0, 0,0, 3, , A\n";

} // namespace

class ScriptApiTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        m_engines.addEngine(new Mugen::MugenEngine);
        m_environment.engines = &m_engines;
        m_environment.projectFolder = m_folder.path();
        m_environment.output = [this](const QString& text) {
            m_output += text;
        };
        m_environment.projects = [this] {
            QList<Project*> list;
            for (const auto& p : m_open)
                list << p.get();
            return list;
        };
        m_environment.currentProject = [this]() -> Project* {
            return m_open.empty() ? nullptr : m_open.back().get();
        };
        m_environment.openProject = [this](Project* project) {
            m_open.emplace_back(project);
        };
    }

    void init()
    {
        m_output.clear();
        m_open.clear();
    }

    void runAndOutput()
    {
        const Script::RunResult result = Script::runScript(
            "debugOutput('a'); debugOutput(42, 'ignored'); debugOutput(3.5); debugOutput(true); debugOutput(nil)\n"
            "print('x', 1, nil)\n"
            "function main() debugOutput('|main') end\n",
            QString(), m_environment);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(m_output, QStringLiteral("a423.5x\t1\tnil\n|main"));

        m_output.clear();
        for (int i = 0; i < 2; ++i)
            QVERIFY(
                Script::runScript("counter = (counter or 0) + 1; debugOutput(counter)", QString(), m_environment).ok);
        QCOMPARE(m_output, QStringLiteral("11"));

        m_output.clear();
        QVERIFY(Script::runScript("debugOutput(tostring(io == nil and os == nil and debug == nil and package == nil))\n"
                                  "debugOutput(tostring(base == _G and utf8 ~= nil and dofile ~= nil))",
                                  QString(), m_environment)
                    .ok);
        QCOMPARE(m_output, QStringLiteral("truetrue"));
    }

    void errors()
    {
        Script::RunResult result =
            Script::runScript("-- runtime\nlocal t = nil\nlocal x = t.field\n", QString(), m_environment);
        QVERIFY(!result.ok);
        QVERIFY2(
            result.error.startsWith("sol: runtime error: [string \"-- runtime...\"]:3: attempt to index a nil value"),
            qPrintable(result.error));
        QVERIFY(result.error.contains("\nstack traceback:\n\t[string \"-- runtime...\"]:3: in main chunk"));
        QVERIFY(!result.error.endsWith("in ?"));

        result = Script::runScript("-- syntax\nx = = 1\n", QString(), m_environment);
        QVERIFY(!result.ok);
        QVERIFY2(result.error.startsWith("sol: syntax error: [string \"-- syntax...\"]:2:"), qPrintable(result.error));

        result = Script::runScript("function main() error('inside main') end", QString(), m_environment);
        QVERIFY(!result.ok);
        QVERIFY(result.error.contains("inside main"));

        result = Script::runScript("error({code = 1})", QString(), m_environment);
        QVERIFY(result.error.contains("An unknown error has triggered the default error handler"));

        result = Script::runScript("local w = FF.wizard(); w.addPage(FF.wizardPage())", QString(), m_environment);
        QVERIFY(!result.ok);
        result = Script::runScript("local r = FF.radioButton(); local c = FF.comboBox(); local k = FF.checkBox('x', 5)",
                                   QString(), m_environment);
        QVERIFY2(result.ok, qPrintable(result.error));
    }

    void stop()
    {
        int asked = 0;
        Script::Environment environment = m_environment;
        environment.stopRequested = [&asked] {
            return ++asked > 3;
        };
        const Script::RunResult result = Script::runScript("while true do end", QString(), environment);
        QVERIFY(!result.ok);
        QVERIFY(result.stopped);
        QVERIFY(result.error.contains("script stopped"));
    }

    void enumsAndFunctions()
    {
        const Script::RunResult result = Script::runScript(
            "assert(FF.Filter.Files | FF.Filter.Dirs == 3)\n"
            "assert(MUGEN.ProjectType.Character == 0 and MUGEN.ProjectType.Fight == 6)\n"
            "assert(FF.MoveOperation.EndOfLine == 15 and FF.SelectionType.Document == 3)\n"
            "assert(not pcall(function() FF.Align.Left = 3 end))\n"
            "assert(not pcall(function() MUGEN.ProjectType.Character = 3 end))\n"
            "assert(FF.translate('Hello') == 'Hello' and FF.translate() == '')\n"
            "assert(FF.projectCount() == 0 and FF.currentProject() == nil and FF.currentEngine() == nil)\n"
            "assert(FF.engineList().mugen.name == 'mugen' and FF.engineByName('nope') == nil)\n"
            "local filters = FF.engineByName('mugen'):singleFileFilters()\n"
            "assert(filters[0] == '*.def' and filters[6] == '*.cmd *.*' and filters[5] == nil)\n"
            "assert(FF.emptyProject('nope', 0) == nil)\n"
            "local p = FF.emptyProject('mugen', MUGEN.ProjectType.Stage)\n"
            "assert(FF.projectCount() == 1 and FF.projectAt(0) == p and FF.projectAt(1) == nil)\n"
            "assert(FF.currentProject() == p and FF.currentEngine().name == 'mugen')\n"
            "assert(p.type == MUGEN.ProjectType.Stage)\n"
            "local rgb = FF.RGBA.new(1, 2, 3, 4):toRGB()\n"
            "assert(rgb.red == 1 and rgb.b == 3)\n"
            "local m = FF.Margins.new(1, 2, 3, 4)\n"
            "assert(m.left == 1 and m.bottom == 4)\n",
            QString(), m_environment);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(m_open.size(), std::size_t(1));
        QCOMPARE(m_open.front()->typeId(), QStringLiteral("stage"));
    }

    void projects()
    {
        QTemporaryDir dir;
        Formats::Test::FixtureFactory fixtures;
        QVERIFY(fixtures.sff(Formats::SffVersion::V1, 3).save(dir.filePath("p.sff").toStdWString()).ok());
        QVERIFY(fixtures.snd(2).save(dir.filePath("p.snd").toStdWString()).ok());
        writeFile(dir.filePath("p.air"), kAir);
        writeFile(dir.filePath("p.cns"), "[Statedef 0]\ntype = S\n");
        writeFile(dir.filePath("p.def"), "[Info]\nname = \"Scripted\"\nmugenversion = 1.0\n[Files]\nsprite = p.sff\n"
                                         "anim = p.air\nst = p.cns\nsound = p.snd\n");
        const QString def = QDir::fromNativeSeparators(dir.filePath("p.def"));
        const Script::RunResult result = Script::runScript(
            QStringLiteral(
                "local p = FF.projectFromFile('%1', false)\n"
                "assert(p.name == 'Scripted' and p.type == MUGEN.ProjectType.Character and not p.isModified)\n"
                "assert(#p:files() == 5)\n"
                "assert(#p:filesByType(FF.FileType.Scripts) == 1)\n"
                "local anims = p:fileByType(FF.FileType.Animations):asAnimations()\n"
                "assert(anims.count == 2)\n"
                "local stand = anims:at(0)\n"
                "assert(stand.displayName == '0 - Standing' and stand.text == 'Standing' and stand.frameCount == 2)\n"
                "assert(#stand.frames == 2 and anims:at(2) == nil and anims:find(200).number == 200)\n"
                "local f = stand:frameAt(1)\n"
                "assert(f.group == 0 and f.number == 1 and f.offsetX == 2 and f.offsetY == -1 and f.time == 7)\n"
                "assert(f.flip == FF.Flip.Horizontal and f.offset.x == 2 and stand:frameIndex(f) == 1)\n"
                "local boxes = f:collisionBoxes()\n"
                "assert(boxes.count == 1 and boxes.boxType == 6 and boxes:at(0).rect.left == -10)\n"
                "assert(boxes:at(0).code:find('Clsn2%[0%]') ~= nil)\n"
                "local hit = anims:at(1):frameAt(0)\n"
                "assert(hit.blend.mode == FF.Mode.Addition and hit:attackBoxes():at(0).right == 40)\n"
                "assert(stand:propertyCount() == 3 and stand:propertyName(0) == 'DisplayName')\n"
                "assert(f:propertyName(2) == 'Delay' and f:propertyValue(2) == 7)\n"
                "local sprites = p:fileByType(FF.FileType.Sprites):asSprites()\n"
                "assert(sprites.count == 3 and sprites:at(3) == nil)\n"
                "local s = sprites:at(0)\n"
                "assert(sprites:find(s.group, s.number).index == 0 and sprites:find(9000, 0) == nil)\n"
                "assert(s.imageProperties.width == s.width and s.palette ~= nil)\n"
                "local colors = s.palette.colorTable\n"
                "assert(#colors >= 1)\n"
                "s.x = 77\n"
                "assert(sprites:at(0).x == 77)\n"
                "local sounds = p:fileByType(FF.FileType.Sounds):asSounds()\n"
                "assert(sounds.count == 2 and sounds:at(0).size > 0)\n"
                "local cns = p:fileByType(FF.FileType.Scripts):asSourceCode()\n"
                "assert(cns.title == 'p.cns' and cns.cursor.atEnd)\n"
                "cns:insertCode('; added\\n')\n"
                "local c = cns.cursor\n"
                "c:movePosition(FF.MoveOperation.Start)\n"
                "c:select(FF.SelectionType.LineUnderCursor)\n"
                "assert(c.selectedCode == '[Statedef 0]')\n"
                "assert(cns.isModified and p.isModified)\n")
                .arg(def),
            QString(), m_environment);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(m_open.size(), std::size_t(1));
        Project* project = m_open.front().get();
        auto* cns = qobject_cast<CodeFile*>(project->file(dir.filePath("p.cns")));
        QVERIFY(cns);
        QVERIFY(cns->document()->toPlainText().endsWith("; added\n"));
    }

    void archives()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("sub");
        writeFile(dir.filePath("a.txt"), "alpha");
        writeFile(dir.filePath("b.def"), "beta");
        writeFile(dir.filePath("sub/c.txt"), "gamma");
        const QString root = QDir::fromNativeSeparators(dir.path());
        const Script::RunResult result = Script::runScript(
            QStringLiteral(
                "local a = FF.Archive.instance('%1', false)\n"
                "assert(a.path == '%1' and not a:isReadOnly())\n"
                "assert(a:fileExists('a.txt') and a:fileExists('%1/a.txt') and a:directoryExists('sub'))\n"
                "local files = a:entries('', FF.StringList.new(), FF.Filter.Files, FF.SortFlag.Name)\n"
                "assert(#files == 2 and files[1] == 'a.txt' and files[2] == 'b.def')\n"
                "local txt = a:entries('', {'*.txt'}, FF.Filter.Files | FF.Filter.Dirs, FF.SortFlag.Name)\n"
                "assert(#txt == 1)\n"
                "local f = a:openFile('a.txt', FF.OpenFlags.ReadOnly)\n"
                "assert(f.size == 5 and f:read() == 'alpha' and f.atEnd)\n"
                "a:closeFile(f)\n"
                "local w = a:openFile('new.txt', FF.OpenFlags.WriteOnly)\n"
                "w:write('written')\n"
                "a:closeFile(w)\n"
                "assert(a:fileExists('new.txt') and a:renameFile('new.txt', 'moved.txt'))\n"
                "assert(a:removeFile('moved.txt') and not a:fileExists('moved.txt'))\n"
                "local ro = FF.Archive.instance('%1', true)\n"
                "assert(ro:openFile('x.txt', FF.OpenFlags.WriteOnly) == nil and not ro:isWritable('a.txt'))\n")
                .arg(root),
            QString(), m_environment);
        QVERIFY2(result.ok, qPrintable(result.error));
    }

private:
    EngineManager m_engines;
    QTemporaryDir m_folder;
    Script::Environment m_environment;
    QString m_output;
    std::vector<std::unique_ptr<Project>> m_open;
};

QTEST_MAIN(ScriptApiTest)
#include "ScriptApiTest.moc"
