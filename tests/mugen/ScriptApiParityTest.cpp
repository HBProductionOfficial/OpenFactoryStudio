#include "core/engine/EngineManager.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/project/ZipArchive.h"
#include "core/script/ScriptHost.h"
#include "engines/mugen/MugenEngine.h"

#include <QCoreApplication>
#include <QFile>
#include <QLocale>
#include <QTemporaryDir>
#include <QTest>
#include <QTextDocument>
#include <QtEndian>

#include <algorithm>
#include <memory>

using namespace OFS;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(data);
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// A .qm file with (context, source, translation) messages, so the test needs no translation tools.
QByteArray qmFile(const QList<std::array<QString, 3>>& messages)
{
    auto be32 = [](QByteArray& out, quint32 value) {
        char bytes[4];
        qToBigEndian(value, bytes);
        out.append(bytes, 4);
    };
    auto elfHash = [](const QByteArray& text) {
        quint32 h = 0;
        for (const char c : text) {
            h = (h << 4) + quint8(c);
            if (const quint32 g = h & 0xf0000000)
                h ^= g >> 24;
            h &= ~(h & 0xf0000000);
        }
        return h ? h : 1u;
    };
    QByteArray records;
    QList<std::pair<quint32, quint32>> hashes;
    for (const auto& [context, source, translation] : messages) {
        hashes.append({elfHash(source.toUtf8()), quint32(records.size())});
        QByteArray utf16;
        for (const QChar c : translation) {
            utf16.append(char(c.unicode() >> 8));
            utf16.append(char(c.unicode() & 0xff));
        }
        records.append(char(3)); // translation
        be32(records, quint32(utf16.size()));
        records.append(utf16);
        records.append(char(6)); // source text
        be32(records, quint32(source.toUtf8().size()));
        records.append(source.toUtf8());
        records.append(char(7)); // context
        be32(records, quint32(context.toUtf8().size()));
        records.append(context.toUtf8());
        records.append(char(1)); // end
    }
    std::sort(hashes.begin(), hashes.end());
    QByteArray table;
    for (const auto& [hash, offset] : hashes) {
        be32(table, hash);
        be32(table, offset);
    }
    static const unsigned char magic[] = {0x3c, 0xb8, 0x64, 0x18, 0xca, 0xef, 0x9c, 0x95,
                                          0xcd, 0x21, 0x1c, 0xbf, 0x60, 0xa1, 0xbd, 0xdd};
    QByteArray qm(reinterpret_cast<const char*>(magic), sizeof(magic));
    qm.append(char(0x42)); // hashes
    be32(qm, quint32(table.size()));
    qm.append(table);
    qm.append(char(0x69)); // messages
    be32(qm, quint32(records.size()));
    qm.append(records);
    return qm;
}

} // namespace

class ScriptApiParityTest : public QObject
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

    void mainAndResults()
    {
        Script::RunResult result = Script::runScript("debugOutput('chunk;')\nmain = 5\n", QString(), m_environment);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(m_output, QStringLiteral("chunk;"));
        for (const char* code : {"return 42", "return 'text', nil, {}", "function main() return 7 end\nreturn false",
                                 "function main() return nil, 'x' end"}) {
            result = Script::runScript(QString::fromLatin1(code), QString(), m_environment);
            QVERIFY2(result.ok, code);
            QVERIFY(result.error.isEmpty());
        }
        result = Script::runScript("function main() FF.emptyProject('mugen', MUGEN.ProjectType.Font) end", QString(),
                                   m_environment);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(m_open.size(), std::size_t(1));
    }

    void editorGlobals()
    {
        Script::RunResult result =
            Script::runScript("debugOutput(tostring(ScriptPath) .. ' ' .. tostring(arg) .. ' ' .. select('#', ...))",
                              QString(), m_environment);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(m_output, QStringLiteral("nil nil 0"));
        QTemporaryDir dir;
        writeFile(dir.filePath("t.lua"), "debugOutput(ScriptPath)");
        m_output.clear();
        result = Script::runScriptFile(dir.filePath("t.lua"), m_environment);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(m_output, QDir::fromNativeSeparators(dir.path()));
    }

    void unsuitableArguments()
    {
        const Script::RunResult result = Script::runScript("local ok, message = pcall(FF.RGB.new, 1, 2)\n"
                                                           "assert(not ok)\n"
                                                           "debugOutput(message)\n"
                                                           "assert(not pcall(FF.Margins.new, 'a', 'b'))\n",
                                                           QString(), m_environment);
        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY2(m_output.contains("no matching function call takes this number of arguments and the specified types"),
                 qPrintable(m_output));
        const Script::RunResult failed = Script::runScript("local c = FF.RGB.new('x')", QString(), m_environment);
        QVERIFY(!failed.ok);
        QVERIFY2(failed.error.startsWith("sol: runtime error:"), qPrintable(failed.error));
        QVERIFY(failed.error.contains("no matching function call"));
    }

    void flagTypes()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("b.txt"), "b");
        writeFile(dir.filePath("a.txt"), "a");
        QDir(dir.path()).mkpath("sub");
        const Script::RunResult result = Script::runScript(
            QStringLiteral("debugOutput(tostring(FF.Filters.new()) .. ' ')\n"
                           "debugOutput(tostring(FF.Filters.new() | FF.Filter.Files) .. ' ')\n"
                           "debugOutput(tostring(FF.SortFlags.new()) .. ' ')\n"
                           "debugOutput(tostring(FF.SortFlags.new() | FF.SortFlag.Reversed))\n"
                           "local a = FF.Archive.instance('%1', true)\n"
                           "local files = a:entries('', FF.StringList.new(), FF.Filters.new() | FF.Filter.Files,"
                           " FF.SortFlags.new())\n"
                           "assert(#files == 2 and files[1] == 'a.txt' and files[2] == 'b.txt')\n")
                .arg(QDir::fromNativeSeparators(dir.path())),
            QString(), m_environment);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(m_output, QStringLiteral("0 2 0 8"));
    }

    // These fields crashed Fighter Factory, so they are left out.
    void codeNodeFields()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("p.air"), "; Standing\n[Begin Action 0]\n0,0, 0,0, 5\n");
        writeFile(dir.filePath("p.def"), "[Info]\nname = \"Nodes\"\nmugenversion = 1.0\n[Files]\nanim = p.air\n");
        const Script::RunResult result =
            Script::runScript(QStringLiteral("local p = FF.projectFromFile('%1', false)\n"
                                             "local a = p:fileByType(FF.FileType.Animations):asAnimations():at(0)\n"
                                             "assert(a.document == nil and a.parent == nil and a.type == nil)\n"
                                             "local f = a:frameAt(0)\n"
                                             "assert(f.code == nil and f.document == nil)\n"
                                             "assert(a.code:find('%[Begin Action 0%]') ~= nil)\n")
                                  .arg(QDir::fromNativeSeparators(dir.filePath("p.def"))),
                              QString(), m_environment);
        QVERIFY2(result.ok, qPrintable(result.error));
    }

    void zipArchives()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("src/a.txt"), "alpha");
        writeFile(dir.filePath("src/b.def"), "beta");
        writeFile(dir.filePath("src/sub/c.txt"), "gamma");
        const QString zip = dir.filePath("test.zip");
        QVERIFY(Zip::create(zip, dir.filePath("src"), {"a.txt", "b.def", "sub/c.txt"}));
        const Script::RunResult result = Script::runScript(
            QStringLiteral("local a = FF.Archive.instance('%1', true)\n"
                           "assert(a.name == 'test')\n"
                           "assert(a.path == '%2')\n"
                           "assert(a:fileExists('a.txt') and a:fileExists('sub/c.txt') and not a:fileExists('c.txt'))\n"
                           "assert(a:directoryExists('sub') and not a:directoryExists('other'))\n"
                           "local all = a:entries('', FF.StringList.new(), FF.Filters.new() | FF.Filter.AllEntries,"
                           " FF.SortFlags.new())\n"
                           "assert(#all == 3 and all[1] == 'a.txt' and all[2] == 'b.def' and all[3] == 'sub')\n"
                           "local f = a:openFile('sub/c.txt', FF.OpenFlags.ReadOnly)\n"
                           "assert(f:read() == 'gamma')\n")
                .arg(QDir::fromNativeSeparators(zip), QDir::fromNativeSeparators(dir.path())),
            QString(), m_environment);
        QVERIFY2(result.ok, qPrintable(result.error));
    }

    // FF.translate uses <name>_<language>.qm beside the script.
    void scriptTranslation()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("Greet.lua"), "debugOutput(FF.translate('Hello') .. '|' .. FF.translate('Other'))");
        writeFile(dir.filePath("Greet_fr.qm"), qmFile({{"Greet", "Hello", QStringLiteral("Bonjour")}}));
        const QLocale before;
        QLocale::setDefault(QLocale(QLocale::French, QLocale::France));
        const Script::RunResult result = Script::runScriptFile(dir.filePath("Greet.lua"), m_environment);
        const Script::RunResult text =
            Script::runScript("debugOutput('/' .. FF.translate('Hello'))", QString(), m_environment);
        QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
        const Script::RunResult german = Script::runScriptFile(dir.filePath("Greet.lua"), m_environment);
        QLocale::setDefault(before);
        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY2(text.ok, qPrintable(text.error));
        QVERIFY2(german.ok, qPrintable(german.error));
        QCOMPARE(m_output, QStringLiteral("Bonjour|Other/HelloHello|Other"));
        QCOMPARE(QCoreApplication::translate("Greet", "Hello"), QStringLiteral("Hello"));
    }

    void parentSave()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("q.cns"), "[Statedef 0]\ntype = S\n");
        writeFile(dir.filePath("q.def"), "[Info]\nname = \"Q\"\nmugenversion = 1.0\n[Files]\nst = q.cns\n");
        const Script::RunResult result =
            Script::runScript(QStringLiteral("local p = FF.projectFromFile('%1', false)\n"
                                             "local cns = p:fileByType(FF.FileType.Scripts):asSourceCode()\n"
                                             "cns:insertCode('; saved\\n')\n"
                                             "assert(p.isModified)\n"
                                             "assert(p.parent:save() == true)\n"
                                             "assert(not p.isModified and not cns.isModified)\n")
                                  .arg(QDir::fromNativeSeparators(dir.filePath("q.def"))),
                              QString(), m_environment);
        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(readFile(dir.filePath("q.cns")).contains("; saved"));
    }

private:
    EngineManager m_engines;
    QTemporaryDir m_folder;
    Script::Environment m_environment;
    QString m_output;
    std::vector<std::unique_ptr<Project>> m_open;
};

QTEST_MAIN(ScriptApiParityTest)
#include "ScriptApiParityTest.moc"
