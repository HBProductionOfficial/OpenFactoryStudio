#include "core/script/ScriptFolders.h"
#include "core/script/ScriptTranslation.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace OFS::Script;

namespace {

void touch(const QString& path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
}

} // namespace

class ScriptFoldersTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void folders()
    {
        QCOMPARE(scriptFolder("D:/data", QString(), ScriptType::Tool), QStringLiteral("D:/data/scripts/tool"));
        QCOMPARE(scriptFolder("D:/data", "mugen", ScriptType::Common),
                 QStringLiteral("D:/data/engines/mugen/scripts/common"));
        QCOMPARE(scriptTypeFolder(ScriptType::Project), QStringLiteral("project"));
        QCOMPARE(scriptTypeFolder(ScriptType::CodeSnippet), QStringLiteral("snippet"));
    }

    void listing()
    {
        QTemporaryDir data;
        touch(data.filePath("engines/mugen/scripts/project/Template/Winane.lua"));
        touch(data.filePath("engines/mugen/scripts/project/Basic/Empty.lua"));
        touch(data.filePath("engines/mugen/scripts/project/Basic/Empty.png"));
        QCOMPARE(listScripts(scriptFolder(data.path(), "mugen", ScriptType::Project)),
                 (QStringList{"Basic/Empty", "Template/Winane"}));
        QVERIFY(listScripts(data.filePath("missing")).isEmpty());

        touch(data.filePath("scripts/tool/b tool.lua"));
        touch(data.filePath("scripts/tool/A tool.lua"));
        touch(data.filePath("scripts/tool/sub/hidden.lua"));
        touch(data.filePath("engines/mugen/scripts/tool/Engine tool.lua"));
        const QList<ToolScript> tools = toolScripts(data.path(), {"mugen"});
        QStringList captions;
        for (const ToolScript& tool : tools)
            captions << tool.caption;
        QCOMPARE(captions, (QStringList{"A tool", "b tool", "Engine tool"}));
        QVERIFY(tools[2].path.endsWith("engines/mugen/scripts/tool/Engine tool.lua"));
    }

    void translatable()
    {
        const QString code = QStringLiteral("local a = FF.translate(\"Plain\")\n"
                                            "local b = FF.translate(\"Plain\") -- the same text once\n"
                                            "local c = FF.translate('Single')\n"
                                            "local d = FF.translate(\"Say \\\"hi\\\"\\n\")\n"
                                            "local e = FF.translate(\"Joined \" .. name) -- not one text\n"
                                            "local f = FF.translate(name)\n"
                                            "local g = FF.translate([[Long\n"
                                            "text]])\n"
                                            "-- FF.translate(\"In a comment\")\n"
                                            "--[[ FF.translate(\"In a long comment\") ]]\n"
                                            "local h = FF . translate ( \"Spaced\" )\n"
                                            "local i = OFS.translate(\"Alias\")\n"
                                            "local j = other.translate(\"Other\")\n"
                                            "local k = \"FF.translate('In a string')\"\n"
                                            "local l = FF.translate(\"Ünïcödé\")\n");
        QCOMPARE(translatableStrings(code), (QStringList{"Plain", "Single", "Say \"hi\"\n", "Long\ntext", "Spaced",
                                                         "Alias", QStringLiteral("Ünïcödé")}));
    }

    void tsFile()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("Empty.ts");
        QVERIFY(writeTranslationFile(path, "Empty", {"Empty project", "A & B"}));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray ts = file.readAll();
        QVERIFY(ts.contains("<!DOCTYPE TS>"));
        QVERIFY(ts.contains("<TS version=\"2.1\">"));
        QVERIFY(ts.contains("<name>Empty</name>"));
        QVERIFY(ts.contains("<source>Empty project</source>"));
        QVERIFY(ts.contains("<source>A &amp; B</source>"));
        QVERIFY(ts.contains("<translation type=\"unfinished\"/>") ||
                ts.contains("<translation type=\"unfinished\"></translation>"));
    }
};

QTEST_GUILESS_MAIN(ScriptFoldersTest)
#include "ScriptFoldersTest.moc"
