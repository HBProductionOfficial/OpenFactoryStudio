#include "core/code/CodeBlockData.h"
#include "core/code/CodeEditor.h"
#include "core/code/CodeLine.h"
#include "core/code/CompletionModels.h"
#include "core/code/Snippet.h"
#include "core/code/TextSearch.h"
#include "core/project/ProjectFile.h"
#include "core/project/ProjectSearch.h"
#include "core/syntax/SyntaxDatabase.h"
#include "core/text/Encodings.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QDir>
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
#ifdef Q_OS_WIN
        // windows-1252 on the western Windows systems we test on. Elsewhere UTF-8 cannot hold this byte.
        QTest::newRow("latin1") << QByteArray("name = \"caf\xE9\"\r\n") << "System";
#endif
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

    void encodings()
    {
#ifdef Q_OS_WIN
        // Qt with ICU lacks ISCII, TSCII and WINSAMI2, so this only holds on Windows.
        for (const TextEncoding& e : TextEncoding::all())
            QVERIFY2(TextEncoding::isAvailable(e.codecName()), qPrintable(e.codecName()));
#endif
        for (const char* name : {"System", "UTF-8", "UTF-16LE", "windows-1252", "Shift_JIS", "GBK", "Big5", "EUC-KR",
                                 "KOI8-R", "ISO-8859-2"})
            QVERIFY2(TextEncoding::isAvailable(QLatin1String(name)), name);
        QCOMPARE(TextEncoding::indexOf("System"), 0);
        QCOMPARE(TextEncoding::indexOf("cp1252"), TextEncoding::indexOf("windows-1252"));
        QCOMPARE(TextEncoding::indexOf("nonsense"), -1);

        bool ok = false;
        const QByteArray sjis("\x83\x65\x83\x58\x83\x67", 6); // "テスト"
        QCOMPARE(TextEncoding::decode(sjis, "Shift_JIS", &ok), QStringLiteral(u"テスト"));
        QVERIFY(ok);
        QCOMPARE(TextEncoding::encode(QStringLiteral(u"テスト"), "SJIS"), sjis);
        QCOMPARE(TextEncoding::decode("caf\xE9", "windows-1252"), QStringLiteral(u"café"));
        QCOMPARE(TextEncoding::encode(QStringLiteral(u"привет"), "KOI8-R"), QByteArray("\xD0\xD2\xC9\xD7\xC5\xD4"));
        TextEncoding::decode("x", "no-such-codec", &ok);
        QVERIFY(!ok);
    }

    void encodingDetection()
    {
        const QString japanese =
            QStringLiteral(u"; 立ちパンチ（弱）\n[State 200, 1]\ntype = ChangeState ; 状態を変える\n");
        const QByteArray sjis = TextEncoding::encode(japanese, "Shift_JIS");
        QString encoding;
        QByteArray bom;
        QCOMPARE(CodeFile::decode(sjis, &encoding, &bom), japanese);
        QCOMPARE(encoding, QStringLiteral("Shift_JIS"));

        const QString french = QStringLiteral(u"; Été, à côté du château, où l'élève préféré déçu reçoit "
                                              u"des crêpes à la crème brûlée près de la forêt\nname = \"Hélène\"\n");
        const QByteArray latin = TextEncoding::encode(french, "windows-1252");
        QCOMPARE(CodeFile::decode(latin, &encoding, &bom), french);
        QVERIFY2(encoding == QLatin1String("windows-1252") || encoding == QLatin1String("ISO-8859-1") ||
                     encoding == QLatin1String("ISO-8859-15"),
                 qPrintable(encoding));

        CodeFile::setFallbackEncoding("windows-1252");
        CodeFile::decode("name = \"caf\xE9\"", &encoding, &bom);
        QCOMPARE(encoding, QStringLiteral("windows-1252"));
        CodeFile::setFallbackEncoding("System");
    }

    void codeFileEncodingChanges()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("f.def");
        const QByteArray sjis("name = \"\x83\x65\x83\x58\x83\x67\"\r\n", 16);
        writeBytes(path, sjis);

        CodeFile::setFallbackEncoding("Shift_JIS");
        CodeFile file(path, "definitions");
        QString error;
        QVERIFY(file.load(&error));
        CodeFile::setFallbackEncoding("System");
        QCOMPARE(file.encoding(), QStringLiteral("Shift_JIS"));
        QCOMPARE(file.document()->toPlainText(), QStringLiteral(u"name = \"テスト\"\n"));

        QVERIFY(file.reloadWithEncoding("windows-1252", &error));
        QCOMPARE(file.encoding(), QStringLiteral("windows-1252"));
        QVERIFY(!file.isModified());
        QVERIFY(file.document()->toPlainText().startsWith(QStringLiteral(u"name = \"ƒeƒXƒg\"")));
        QVERIFY(file.reloadWithEncoding("Shift_JIS", &error));
        QCOMPARE(file.document()->toPlainText(), QStringLiteral(u"name = \"テスト\"\n"));

        QVERIFY(file.setEncoding("UTF-8", &error));
        QVERIFY(file.isModified());
        QVERIFY(file.save(&error));
        QCOMPARE(readBytes(path), QByteArray("name = \"\xE3\x83\x86\xE3\x82\xB9\xE3\x83\x88\"\r\n"));
        QVERIFY(!file.setEncoding("no-such-codec", &error));
        QVERIFY(!file.reloadWithEncoding("no-such-codec", &error));
    }

    void textSearch()
    {
        const QString text =
            QStringLiteral("[State 200]\ntype = HitDef\nTrigger1 = AnimElem = 3\ntrigger2 = time = 0\n");
        SearchOptions plain;
        TextSearch search("trigger", plain);
        QCOMPARE(search.findAll(text).size(), 2);
        QCOMPARE(search.find(text, 0).start, text.indexOf("Trigger1"));
        QCOMPARE(search.find(text, text.indexOf("Trigger1") + 1).start, text.indexOf("trigger2"));
        QVERIFY(!search.find(text, text.indexOf("trigger2") + 1).isValid());
        QCOMPARE(search.find(text, text.size(), true).start, text.indexOf("trigger2"));
        QCOMPARE(search.find(text, text.indexOf("trigger2") + 6, true).start, text.indexOf("Trigger1"));
        QVERIFY(!search.find(text, 5, true).isValid());

        SearchOptions sensitive;
        sensitive.caseSensitive = true;
        QCOMPARE(TextSearch("trigger", sensitive).findAll(text).size(), 1);

        SearchOptions words;
        words.wholeWords = true;
        QCOMPARE(TextSearch("time", words).findAll(text).size(), 1);
        QCOMPARE(TextSearch("tim", words).findAll(text).size(), 0);
        QCOMPARE(TextSearch("[State", words).findAll(text).size(), 1);

        QCOMPARE(TextSearch("HitDef\ntrigger1", plain).find(text, 0).start, text.indexOf("HitDef"));
        QCOMPARE(TextSearch("[State 200]", plain).find(text, 0).start, 0);

        SearchOptions regex;
        regex.regExp = true;
        const TextSearch captures("(\\w+) = (\\d+)$", regex);
        const TextSearch::Match m = captures.find(text, 0);
        QVERIFY(m.isValid());
        QCOMPARE(captures.replacement(m, "%2 = %1 (%0)"), QStringLiteral("3 = AnimElem (AnimElem = 3)"));
        QCOMPARE(TextSearch("a(", regex).isValid(), false);
        QVERIFY(TextSearch("x*", regex).findAll(text).isEmpty());
        QVERIFY(captures.matchAt(text, m.start, m.length).isValid());
        QVERIFY(!captures.matchAt(text, m.start, m.length - 1).isValid());
        QVERIFY(!captures.matchAt(text, m.start - 1, m.length + 1).isValid());
        QCOMPARE(TextSearch("a", plain).replacement(m, "%1"), QStringLiteral("%1"));
    }

    void textSearchOps()
    {
        CodeEditor editor;
        editor.setPlainText("[Data]\nlife = 1000\n[Size]\nxscale = 1\nyscale = 1\n");
        QCoreApplication::processEvents();
        SearchOptions plain;
        const TextSearch scale("scale", plain);

        editor.foldAll();
        QVERIFY(TextSearchOps::find(&editor, scale, false, false));
        QCOMPARE(editor.textCursor().selectedText(), QStringLiteral("scale"));
        QCOMPARE(editor.cursorLine(), 4);
        QVERIFY(editor.document()->findBlockByNumber(3).isVisible());
        QVERIFY(TextSearchOps::find(&editor, scale, false, false));
        QCOMPARE(editor.cursorLine(), 5);
        QVERIFY(!TextSearchOps::find(&editor, scale, false, false));
        QVERIFY(TextSearchOps::find(&editor, scale, false, true));
        QCOMPARE(editor.cursorLine(), 4);
        QVERIFY(TextSearchOps::find(&editor, scale, true, true));
        QCOMPARE(editor.cursorLine(), 5);

        editor.moveCursor(QTextCursor::Start);
        QVERIFY(TextSearchOps::replace(&editor, scale, "size", false, false));
        QCOMPARE(editor.cursorLine(), 4);
        QVERIFY(editor.toPlainText().contains("xscale"));
        QVERIFY(TextSearchOps::replace(&editor, scale, "size", false, false));
        QVERIFY(editor.toPlainText().contains("xsize = 1"));
        QCOMPARE(editor.cursorLine(), 5);

        SearchOptions regex;
        regex.regExp = true;
        QCOMPARE(TextSearchOps::replaceAll(&editor, TextSearch("(\\w+) = (\\d+)", regex), "%1: %2"), 3);
        QCOMPARE(editor.toPlainText(), QStringLiteral("[Data]\nlife: 1000\n[Size]\nxsize: 1\nyscale: 1\n"));
        editor.undo();
        QCOMPARE(editor.toPlainText(), QStringLiteral("[Data]\nlife = 1000\n[Size]\nxsize = 1\nyscale = 1\n"));

        editor.setReadOnly(true);
        QCOMPARE(TextSearchOps::replaceAll(&editor, scale, "x"), 0);
    }

    void snippets()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.filePath("shipped"));
        QDir().mkpath(dir.filePath("user"));
        writeBytes(dir.filePath("shipped/Generic state.xml"),
                   "<snippet>\n"
                   "  <field name=\"sdef\" title=\"StateDef\" type=\"int\"/>\n"
                   "  <field name=\"sdefname\" title=\"Name\" type=\"string\"/>\n"
                   "  <field name=\"kind\" title=\"Type\" type=\"items\" value=\"S,C,A\" "
                   "value_title=\"Standing,Crouching,Air\"/>\n"
                   "  <code>\n"
                   "    ; @sdefname\n"
                   "    [Statedef @sdef]\n"
                   "    type = @kind\n"
                   "\n"
                   "    [State @sdef, end]\n"
                   "      type = ChangeState\n"
                   "      value = CURSOR\n"
                   "  </code>\n"
                   "</snippet>\n");
        writeBytes(dir.filePath("shipped/Command.xml"), "<snippet><code>[Command]</code></snippet>");
        writeBytes(dir.filePath("shipped/Command defaults.xml"), "<snippet><code>[Defaults]</code></snippet>");
        writeBytes(dir.filePath("shipped/broken.xml"), "<snippet><code>");
        writeBytes(dir.filePath("user/command.xml"), "<snippet><code>[Command] ; mine</code></snippet>");

        SnippetLibrary library;
        library.load({dir.filePath("shipped"), dir.filePath("user")});
        QCOMPARE(library.names(), (QStringList{"Command defaults", "command", "Generic state"}));
        QCOMPARE(library.find("COMMAND")->code, QStringLiteral("[Command] ; mine"));

        const Snippet* state = library.find("Generic state");
        QVERIFY(state);
        QCOMPARE(state->fields.size(), 3);
        QCOMPARE(state->fields[2].type, SnippetField::Type::Items);
        QCOMPARE(state->fields[2].valueTitles, (QStringList{"Standing", "Crouching", "Air"}));
        int cursor = -2;
        const QString text = state->expand({{"sdef", "200"}, {"sdefname", "Punch"}, {"kind", "S"}}, &cursor);
        QCOMPARE(text, QStringLiteral("; Punch\n[Statedef 200]\ntype = S\n\n[State 200, end]\ntype = "
                                      "ChangeState\nvalue = \n"));
        QCOMPARE(cursor, int(text.size()) - 1);
        library.find("Command defaults")->expand({}, &cursor);
        QCOMPARE(cursor, -1);

        Snippet copy = *state;
        copy.fields << SnippetField{"ratio", "Ratio", SnippetField::Type::Float, {}, {}};
        QVERIFY(copy.save(dir.filePath("user/Copy.xml")));
        const QByteArray bytes = readBytes(dir.filePath("user/Copy.xml"));
        QVERIFY(
            bytes.startsWith("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<snippet>\r\n\r\n\t<field name=\"sdef\""));
        QVERIFY(bytes.contains("\t<code>\r\n\t\t    ; @sdefname\r\n"));
        QVERIFY(bytes.contains("value = CURSOR\r\n\t</code>\r\n\r\n</snippet>\r\n"));
        QVERIFY(bytes.contains("type=\"float\""));
        Snippet reloaded;
        QVERIFY(reloaded.load(dir.filePath("user/Copy.xml")));
        QCOMPARE(reloaded.name, QStringLiteral("Copy"));
        QCOMPARE(reloaded.fields.size(), 4);
        QCOMPARE(reloaded.fields[2].values, (QStringList{"S", "C", "A"}));
        QCOMPARE(reloaded.fields[0].type, SnippetField::Type::Int);
        QCOMPARE(reloaded.fields[3].type, SnippetField::Type::Float);
        int reloadedCursor = -2;
        QCOMPARE(reloaded.expand({{"sdef", "200"}, {"sdefname", "Punch"}, {"kind", "S"}}, &reloadedCursor), text);
        QCOMPARE(reloadedCursor, int(text.size()) - 1);

        QVERIFY(copy.saveTranslationSource(dir.filePath("copy.ts")));
        QVERIFY(readBytes(dir.filePath("copy.ts")).contains("<source>Generic state</source>"));
    }

    void completion()
    {
        QTemporaryDir dir;
        writeBytes(dir.filePath("completer.xml"), "<completion>\n"
                                                  "  <model prefix=\"type=\">\n"
                                                  "    <word value=\" ChangeState\"/>\n"
                                                  "    <word value=\" ChangeAnim\"/>\n"
                                                  "    <word value=\" VelSet\"/>\n"
                                                  "  </model>\n"
                                                  "  <model prefix=\"const(\"><word value=\"data.life)\"/></model>\n"
                                                  "</completion>\n");
        CompletionModels models;
        QVERIFY(models.load(dir.filePath("completer.xml")));
        QCOMPARE(models.models().size(), 2);
        QVERIFY(models.match("type="));
        QVERIFY(models.match("  Type = "));
        QVERIFY(models.match("trigger1 = Const("));
        QVERIFY(!models.match("type=x"));

        CodeEditor editor;
        editor.setCompletionModels(&models);
        editor.show();
        QVERIFY(QTest::qWaitForWindowExposed(&editor));
        QTest::keyClicks(&editor, "type =");
        QVERIFY(editor.isCompleting());
        QCOMPARE(editor.completer()->completionCount(), 3);
        QTest::keyClicks(&editor, " Cha");
        QCOMPARE(editor.completer()->completionCount(), 2);
        QTest::keyClick(editor.completer()->popup(), Qt::Key_Down);
        QTest::keyClick(editor.completer()->popup(), Qt::Key_Return);
        QVERIFY(!editor.isCompleting());
        QCOMPARE(editor.toPlainText(), QStringLiteral("type = ChangeAnim"));

        editor.clear();
        QTest::keyClicks(&editor, "type=");
        QVERIFY(editor.isCompleting());
        QTest::keyClick(editor.completer()->popup(), Qt::Key_Tab);
        QCOMPARE(editor.toPlainText(), QStringLiteral("type= ChangeState"));

        editor.clear();
        QTest::keyClicks(&editor, "x = const(da");
        QVERIFY(editor.isCompleting());
        QTest::keyClick(editor.completer()->popup(), Qt::Key_Escape);
        QVERIFY(!editor.isCompleting());
        QCOMPARE(editor.toPlainText(), QStringLiteral("x = const(da"));
        QTest::keyClicks(&editor, ")");
        QVERIFY(!editor.isCompleting());
    }

    void projectSearch()
    {
        QCOMPARE(ProjectSearch::splitPatterns("*.def, *.cmd, *.cns, *.st"),
                 (QStringList{"*.def", "*.cmd", "*.cns", "*.st"}));
        QCOMPARE(ProjectSearch::splitPatterns("*.def *.sff;*.air"), (QStringList{"*.def", "*.sff", "*.air"}));
        QVERIFY(ProjectSearch::splitPatterns("  ").isEmpty());
        QVERIFY(ProjectSearch::matchesPatterns("C:/x/KFM.CNS", {"*.def", "*.cns"}, {}));
        QVERIFY(!ProjectSearch::matchesPatterns("C:/x/kfm.sff", {"*.def", "*.cns"}, {}));
        QVERIFY(!ProjectSearch::matchesPatterns("C:/x/common1.cns", {"*.cns"}, {"common*"}));
        QVERIFY(ProjectSearch::matchesPatterns("C:/x/any.bin", {}, {}));

        const QString text = QStringLiteral("[Data]\nlife = 1000\n; life points\npower = 3000");
        const QList<SearchHit> hits = ProjectSearch::find("f.cns", text, TextSearch("life", {}));
        QCOMPARE(hits.size(), 2);
        QCOMPARE(hits[0].line, 1);
        QCOMPARE(hits[0].column, 0);
        QCOMPARE(hits[0].lineText, QStringLiteral("life = 1000"));
        QCOMPARE(hits[1].line, 2);
        QCOMPARE(hits[1].column, 2);
        QCOMPARE(hits[1].length, 4);

        QTemporaryDir dir;
        QDir().mkpath(dir.filePath("sub"));
        writeBytes(dir.filePath("a.cns"), "[Data]\r\nlife = 1000\r\n");
        writeBytes(dir.filePath("sub/b.cmd"), "life");
        writeBytes(dir.filePath("sub/c.sff"), "life");
        QCOMPARE(ProjectSearch::directoryFiles(dir.path(), {"*.cns", "*.cmd"}, {}).size(), 2);
        QCOMPARE(ProjectSearch::directoryFiles(dir.path(), {}, {"*.sff"}).size(), 2);

        SearchOptions regex;
        regex.regExp = true;
        QString error;
        QCOMPARE(ProjectSearch::replaceAll({}, dir.filePath("a.cns"), TextSearch("(\\w+) = (\\d+)", regex), "%1 = 2%2",
                                           &error),
                 1);
        QCOMPARE(readBytes(dir.filePath("a.cns")), QByteArray("[Data]\r\nlife = 21000\r\n"));
        QCOMPARE(ProjectSearch::replaceAll({}, dir.filePath("a.cns"), TextSearch("absent", {}), "x", &error), 0);
        bool ok = false;
        QCOMPARE(ProjectSearch::fileText({}, dir.filePath("a.cns"), &ok), QStringLiteral("[Data]\nlife = 21000\n"));
        QVERIFY(ok);
        ProjectSearch::fileText({}, dir.filePath("missing.cns"), &ok);
        QVERIFY(!ok);
    }

    void tabConversion()
    {
        QCOMPARE(CodeFile::expandTabs("a\tb\n\tc\n12345\td", 4), QStringLiteral("a   b\n    c\n12345   d"));
        QCOMPARE(CodeFile::tabifyIndentation("    x = 1\n      y\n  z\n\t  w = \"a    b\"", 4),
                 QStringLiteral("\tx = 1\n\t  y\n  z\n\t  w = \"a    b\""));

        QTemporaryDir dir;
        const QString path = dir.filePath("t.cns");
        writeBytes(path, "[Data]\r\n\tlife = 1000\r\n");
        CodeFile::setTabConversion(true, true);
        CodeFile file(path, "states");
        QString error;
        QVERIFY(file.load(&error));
        QCOMPARE(file.document()->toPlainText(), QStringLiteral("[Data]\n    life = 1000\n"));
        QVERIFY(file.save(&error));
        QCOMPARE(readBytes(path), QByteArray("[Data]\r\n\tlife = 1000\r\n"));
        QTextCursor c(file.document());
        c.movePosition(QTextCursor::End);
        c.insertText("    power = 3000\n");
        QVERIFY(file.save(&error));
        QCOMPARE(readBytes(path), QByteArray("[Data]\r\n\tlife = 1000\r\n\tpower = 3000\r\n"));
        CodeFile::setTabConversion(false, false);
    }

    void blockHighlight()
    {
        CodeEditor editor;
        CodeEditor::Options options;
        options.highlightBlocks = true;
        options.highlightLine = false;
        options.parenMatching = false;
        editor.setOptions(options);
        editor.setPlainText("[Data]\nlife = 1000\npower = 3000\n[Size]\nxscale = 1\n");
        QCoreApplication::processEvents();
        editor.goToLine(2);
        const auto selections = editor.extraSelections();
        QCOMPARE(selections.size(), 1);
        QCOMPARE(selections.first().cursor.selectionStart(), 0);
        QCOMPARE(selections.first().cursor.selectedText(), QString("[Data]") + QChar::ParagraphSeparator +
                                                               "life = 1000" + QChar::ParagraphSeparator +
                                                               "power = 3000");
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
