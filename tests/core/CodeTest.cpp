#include "core/code/CodeBlockData.h"
#include "core/code/CodeEditor.h"
#include "core/code/CodeLine.h"
#include "core/project/ProjectFile.h"
#include "core/syntax/SyntaxDatabase.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBlock>

using namespace OFS;

namespace {

const char* kSyntax = R"(<?xml version="1.0" encoding="UTF-8"?>
<syntax>
  <file type="state">
    <block type="Data">
      <param name="life" type="int" required="true"/>
      <param name="power" type="int"/>
    </block>
    <block type="StateDef">
      <param name="type" type="keyword" list="S,C,A,L,U"/>
      <param name="velset" type="float2" minver="WinBeta"/>
    </block>
    <block type="State>Pause">
      <param name="time" type="expi" required="true"/>
      <param name="movetime" type="expi"/>
    </block>
    <block type="State>SuperPause:Pause">
      <param name="anim" type="int" char="S"/>
      <param name="unhittable" type="expb" minver="1.1"/>
    </block>
    <block type="State>VarSet">
      <param name="var" type="expi" format="indexed"/>
      <param name="v" type="expi" deprecated="1.0"/>
    </block>
  </file>
  <file type="motif">
    <block type="TitleBG>Normal:state.Pause"/>
    <block type="Title Info"><param name="cursor.move.snd" type="int2"/></block>
  </file>
  <file type="storyboard">
    <block type="Scene"><param name="layer*.anim" type="int"/></block>
  </file>
  <consts name="const"><const name="data.life"/><const name="size.xscale"/></consts>
  <trigger name="AnimElem" type="op2int2"/>
  <trigger name="StateNo"/>
  <trigger name="ProjContact*" type="opint"/>
</syntax>
)";

void writeBytes(const QString& path, const QByteArray& data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QByteArray readBytes(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

} // namespace

class CodeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void syntaxDatabase()
    {
        SyntaxDatabase db;
        QString error;
        QVERIFY2(db.loadFromData(kSyntax, &error), qPrintable(error));
        QCOMPARE(db.fileTypes(), (QStringList{"state", "motif", "storyboard"}));

        const SyntaxBlock* data = db.findBlock("state", "Data", {});
        QVERIFY(data);
        QVERIFY(db.param("state", *data, "LIFE"));
        QVERIFY(db.param("state", *data, "life")->required);

        QVERIFY(db.findBlock("state", "Statedef 200", {}));
        QCOMPARE(db.findBlock("state", "StateDef -1", {})->name, QStringLiteral("StateDef"));
        QVERIFY(!db.findBlock("state", "Statedefx", {}));

        QVERIFY(!db.findBlock("state", "State 200, Pause it", {}));
        const SyntaxBlock* superPause = db.findBlock("state", "State 200, Pause it", "superpause");
        QVERIFY(superPause);
        QCOMPARE(superPause->typeName, QStringLiteral("SuperPause"));
        const QList<SyntaxParam> params = db.params("state", *superPause);
        QStringList names;
        for (const SyntaxParam& p : params)
            names << p.name;
        QCOMPARE(names, (QStringList{"anim", "unhittable", "time", "movetime"}));
        QCOMPARE(db.param("state", *superPause, "anim")->prefixes, QStringLiteral("S"));
        QVERIFY(db.isTypedSection("state", "State 0"));
        QCOMPARE(db.sectionTypes("state", "State 1"), (QStringList{"Pause", "SuperPause", "VarSet"}));

        const SyntaxBlock* varset = db.findBlock("state", "State 1", "VarSet");
        QVERIFY(db.param("state", *varset, "var")->indexed);

        const SyntaxBlock* titleBg = db.findBlock("motif", "TitleBG Logo", "normal");
        QVERIFY(titleBg);
        QVERIFY(db.param("motif", *titleBg, "movetime"));
        QVERIFY(db.findBlock("motif", "Title Info", {}));

        const SyntaxBlock* scene = db.findBlock("storyboard", "Scene 3", {});
        QVERIFY(scene);
        QVERIFY(db.param("storyboard", *scene, "layer2.anim"));
        QVERIFY(db.trigger("projcontact1234"));
        QCOMPARE(db.trigger("animelem")->type, QStringLiteral("op2int2"));
        QVERIFY(!db.trigger("NotATrigger"));
        QVERIFY(db.isConst("const", "Data.Life"));
        QVERIFY(!db.isConst("const", "data.nothing"));
    }

    void versions()
    {
        QVERIFY(SyntaxDatabase::availableIn("", "dosbeta"));
        QVERIFY(SyntaxDatabase::availableIn("WinBeta", "1.0"));
        QVERIFY(!SyntaxDatabase::availableIn("1.1", "1.0"));
        QVERIFY(SyntaxDatabase::deprecatedIn("1.0", "1.1"));
        QVERIFY(!SyntaxDatabase::deprecatedIn("1.0", "winbeta"));
        QVERIFY(SyntaxDatabase::versionRank("ext") > SyntaxDatabase::versionRank("1.1"));
    }

    void codeLines()
    {
        CodeLine l = CodeLine::parse("  [State 200, Punch] ; comment");
        QCOMPARE(l.kind, CodeLine::Kind::Section);
        QCOMPARE(l.header, QStringLiteral("State 200, Punch"));
        QVERIFY(l.headerClosed);
        QCOMPARE(l.commentStart, 21);

        l = CodeLine::parse("trigger1 = AnimElem = 3, >= 2 && Command = \"x;y\" ; note");
        QCOMPARE(l.kind, CodeLine::Kind::KeyValue);
        QCOMPARE(l.key, QStringLiteral("trigger1"));
        QCOMPARE(l.value, QStringLiteral("AnimElem = 3, >= 2 && Command = \"x;y\""));
        QCOMPARE(l.tokens.first().kind, CodeToken::Kind::Identifier);
        QCOMPARE(l.tokens.last().kind, CodeToken::Kind::String);

        l = CodeLine::parse("var(5) = 10");
        QCOMPARE(l.key, QStringLiteral("var"));
        QCOMPARE(l.index, QStringLiteral("5"));

        l = CodeLine::parse("Clsn2[0] = -10, 0, 10,-79");
        QCOMPARE(l.key, QStringLiteral("Clsn2"));
        QCOMPARE(l.index, QStringLiteral("0"));

        l = CodeLine::parse("Clsn2Default: 1");
        QCOMPARE(l.kind, CodeLine::Kind::KeyValue);
        QCOMPARE(l.key, QStringLiteral("Clsn2Default"));
        QCOMPARE(l.value, QStringLiteral("1"));

        l = CodeLine::parse("0,0, 0,0, 10");
        QCOMPARE(l.kind, CodeLine::Kind::Raw);
        QCOMPARE(l.tokens.size(), 9);

        QCOMPARE(CodeLine::parse("   ").kind, CodeLine::Kind::Blank);
        QCOMPARE(CodeLine::parse(" ; only").kind, CodeLine::Kind::Comment);
        QCOMPARE(CodeLine::tokenize(">= != := ** &&").size(), 5);
    }

    void codeFileRoundTrip_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        QTest::addColumn<QString>("encoding");
        QTest::newRow("ascii crlf") << QByteArray("[Data]\r\nlife = 1000\r\n") << "UTF-8";
        QTest::newRow("utf8 bom") << QByteArray("\xEF\xBB\xBFname = \"\xC3\xA9t\xC3\xA9\"\n") << "UTF-8";
        QTest::newRow("utf8 lf") << QByteArray("a = \"\xE2\x98\x85\"\nb = 1") << "UTF-8";
        QTest::newRow("latin1") << QByteArray("name = \"caf\xE9\"\r\n") << "System";
        QTest::newRow("utf16le") << QByteArray("\xFF\xFE"
                                               "a\0=\0"
                                               "1\0",
                                               8)
                                 << "UTF-16LE";
    }

    void codeFileRoundTrip()
    {
        QFETCH(QByteArray, bytes);
        QFETCH(QString, encoding);
        QTemporaryDir dir;
        const QString path = dir.filePath("f.cns");
        writeBytes(path, bytes);
        CodeFile file(path, "states");
        QString error;
        QVERIFY2(file.load(&error), qPrintable(error));
        QCOMPARE(file.encoding(), encoding);
        QVERIFY(!file.isModified());
        QVERIFY(file.save(&error));
        QCOMPARE(readBytes(path), bytes);

        QTextCursor c(file.document());
        c.movePosition(QTextCursor::End);
        c.insertText(QStringLiteral("\n; added"));
        QVERIFY(file.isModified());
        QVERIFY(file.save(&error));
        QVERIFY(!file.isModified());
        CodeFile reloaded(path, "states");
        QVERIFY(reloaded.load(&error));
        QCOMPARE(reloaded.encoding(), encoding);
        QVERIFY(reloaded.document()->toPlainText().endsWith(QStringLiteral("; added")));
        if (bytes.contains("\r\n"))
            QVERIFY(readBytes(path).contains("\r\n; added"));
    }

    void editorCommentToggle()
    {
        CodeEditor editor;
        editor.setPlainText("[Data]\nlife = 1000\n;power = 3000\n");
        QTextCursor c(editor.document()->findBlockByNumber(1));
        c.setPosition(editor.document()->findBlockByNumber(2).position() + 3, QTextCursor::KeepAnchor);
        editor.setTextCursor(c);
        editor.toggleComment();
        QCOMPARE(editor.toPlainText(), QStringLiteral("[Data]\n;life = 1000\n;;power = 3000\n"));
        editor.toggleComment();
        QCOMPARE(editor.toPlainText(), QStringLiteral("[Data]\nlife = 1000\n;power = 3000\n"));
    }

    void editorFolding()
    {
        CodeEditor editor;
        editor.setPlainText("[Data]\nlife = 1000\npower = 3000\n[Size]\nxscale = 1\n");
        QCoreApplication::processEvents(); // let the highlighter mark the sections
        QVERIFY(CodeBlockData::get(editor.document()->findBlockByNumber(0)));
        QVERIFY(CodeBlockData::get(editor.document()->findBlockByNumber(0))->isHeader);
        editor.foldAll();
        QVERIFY(editor.isFolded(0));
        QVERIFY(!editor.document()->findBlockByNumber(1).isVisible());
        QVERIFY(editor.document()->findBlockByNumber(3).isVisible());
        QVERIFY(!editor.document()->findBlockByNumber(4).isVisible());
        editor.goToLine(5);
        QVERIFY(editor.document()->findBlockByNumber(4).isVisible());
        QCOMPARE(editor.cursorLine(), 5);
        editor.unfoldAll();
        QVERIFY(editor.document()->findBlockByNumber(1).isVisible());
    }
};

QTEST_MAIN(CodeTest)
#include "CodeTest.moc"
