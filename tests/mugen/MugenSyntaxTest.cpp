#include "engines/mugen/MugenSyntax.h"
#include "core/syntax/SyntaxDatabase.h"
#include "engines/mugen/expr/Expression.h"

#include <QTest>

using namespace OFS;
using namespace OFS::Mugen;

namespace {

const char* kSyntax = R"(<?xml version="1.0" encoding="UTF-8"?>
<syntax>
  <file type="state">
    <block type="Data">
      <param name="life" type="int" required="true"/>
      <param name="attack" type="int"/>
      <param name="IntPersistIndex" type="int" minver="1.1"/>
    </block>
    <block type="Size"><param name="xscale" type="float"/><param name="ground.back" type="int"/></block>
    <block type="Velocity"><param name="walk.fwd" type="float2"/><param name="jump.neu" type="float2"/></block>
    <block type="StateDef">
      <param name="type" type="keyword" list="S,C,A,L,U"/>
      <param name="movetype" type="keyword" list="A,I,H,U"/>
      <param name="physics" type="keyword" list="S,C,A,N,U"/>
      <param name="anim" type="expi"/>
      <param name="velset" type="exp2f"/>
      <param name="ctrl" type="expi"/>
      <param name="sprpriority" type="expi"/>
    </block>
    <block type="State>ChangeState"><param name="value" type="expi" required="true"/><param name="ctrl" type="expi"/></block>
    <block type="State>VarSet">
      <param name="v" type="expi"/><param name="value" type="expi"/>
      <param name="var" type="expi" format="indexed"/><param name="fvar" type="expf" format="indexed"/>
    </block>
    <block type="State>HitDef">
      <param name="attr" type="hitattr" required="true"/>
      <param name="hitflag" type="hitflags"/>
      <param name="guardflag" type="guardflags"/>
      <param name="damage" type="exp2i"/>
      <param name="sparkno" type="int" char="S"/>
      <param name="priority" type="expkeyword" list="Hit,Miss,Dodge"/>
      <param name="animtype" type="keyword" list="Light,Medium,Hard,Back,Up,DiagUp"/>
    </block>
    <block type="State>AssertSpecial"><param name="flag" type="keyword" list="Intro,Invisible,NoWalk"/></block>
    <block type="State>Width" deprecated="1.0"><param name="value" type="exp2i"/></block>
  </file>
  <file type="command">
    <block type="Command">
      <param name="name" type="string" required="true"/>
      <param name="command" type="keyseq" required="true"/>
      <param name="time" type="int"/>
    </block>
    <block type="StateDef:state.StateDef"/>
    <block type="State>ChangeState:state.ChangeState"/>
  </file>
  <consts name="const"><const name="data.life"/><const name="size.xscale"/></consts>
  <trigger name="AnimElem" type="op2int2"/>
  <trigger name="AnimTime"/>
  <trigger name="Time"/>
  <trigger name="Ctrl"/>
  <trigger name="StateNo"/>
  <trigger name="StateType" type="valueeq" list="S,C,A,L"/>
  <trigger name="MoveType" type="valueeq" list="A,I,H"/>
  <trigger name="Command" type="opstr"/>
  <trigger name="Var" type="exp"/>
  <trigger name="FVar" type="exp"/>
  <trigger name="Abs" type="exp"/>
  <trigger name="IfElse" type="exp3"/>
  <trigger name="Log" type="exp2"/>
  <trigger name="Pos" type="keyword" list="X,Y"/>
  <trigger name="Vel" type="keyword" list="X,Y"/>
  <trigger name="Const" type="const"/>
  <trigger name="HitDefAttr" type="hitattr"/>
  <trigger name="TimeMod" type="opint2"/>
  <trigger name="NumHelper" type="nilexp"/>
  <trigger name="IsHelper" type="expopt"/>
  <trigger name="ProjContact*" type="opint"/>
  <trigger name="Random"/>
  <trigger name="CameraPos" type="keyword" list="X,Y" minver="1.1"/>
</syntax>
)";

} // namespace

class MugenSyntaxTest : public QObject
{
    Q_OBJECT

private:
    SyntaxDatabase m_db;

    IssueList parse(const QString& expr, const QString& version = QStringLiteral("1.1"))
    {
        IssueList issues;
        ExpressionParser parser(&m_db, version);
        parser.parse(expr, 0, 0, issues);
        return issues;
    }

private Q_SLOTS:
    void initTestCase() { QVERIFY(m_db.loadFromData(kSyntax)); }

    void validExpressions_data()
    {
        QTest::addColumn<QString>("expr");
        const char* cases[] = {
            "1",
            "Time",
            "Time = 0",
            "AnimTime = 0",
            "Time >= 10 && Ctrl",
            "!Ctrl || StateNo = 200",
            "Var(1) + 2 * 3 ** 2 - (4 % 3)",
            "-Var(2) / ~1",
            "AnimElem = 3",
            "AnimElem = 3, >= 2",
            "AnimElem = 3, = 0 && Time > 2",
            "StateType = S",
            "StateType != A",
            "MoveType = H || MoveType = A",
            "Command = \"holdfwd\"",
            "Command != \"x\" && Command = \"y\"",
            "Pos X > 0",
            "Vel Y < 0 && Pos Y >= -10",
            "Const(data.life)",
            "Const(size.xscale) * 2",
            "HitDefAttr = SC, NA, SA",
            "HitDefAttr = A, AT",
            "TimeMod = 10, 0",
            "NumHelper",
            "NumHelper(1234) > 0",
            "IsHelper",
            "IsHelper(5)",
            "ProjContact = 1",
            "ProjContact1234 = 1",
            "Time = [0, 10]",
            "Time != (0, 10]",
            "StateNo = [200, 299)",
            "parent, Var(1) = 5",
            "root, StateNo = 0",
            "helper(1000), Time > 3",
            "enemynear(1), Pos X < 100",
            "enemynear, StateNo = 5000",
            "Var(3) := Var(3) + 1",
            "IfElse(Time > 5, 1, 0)",
            "Log(2, 8) = 3",
            "Abs(Vel X) > 2.5",
            "Random < 500",
            ".5 + 1.",
            "(((Time)))",
        };
        for (const char* c : cases)
            QTest::newRow(c) << QString::fromLatin1(c);
    }

    void validExpressions()
    {
        QFETCH(QString, expr);
        const IssueList issues = parse(expr);
        QVERIFY2(issues.isEmpty(), qPrintable(issues.isEmpty() ? QString() : issues.first().message));
    }

    void invalidExpressions_data()
    {
        QTest::addColumn<QString>("expr");
        QTest::addColumn<QString>("message");
        QTest::newRow("unknown trigger") << "Tiem > 3" << "Unknown trigger 'Tiem'";
        QTest::newRow("unknown function") << "Foo(1)" << "Unknown trigger 'Foo'";
        QTest::newRow("open paren") << "(Time > 3" << "Unbalanced parentheses";
        QTest::newRow("close paren") << "Time > 3)" << "Syntax error - unexpected ')'";
        QTest::newRow("dangling op") << "Time >" << "Syntax error - code ends before line";
        QTest::newRow("double op") << "Time > * 3" << "Syntax error - unexpected '*'";
        QTest::newRow("missing arg") << "Var" << "Trigger 'Var' needs an argument";
        QTest::newRow("wrong arg count") << "IfElse(1, 2)" << "Wrong number of arguments for trigger 'IfElse'";
        QTest::newRow("bad keyword") << "StateType = Q" << "Unknown keyword 'Q'";
        QTest::newRow("command needs string") << "Command = holdfwd" << "Syntax error - string expected";
        QTest::newRow("command compare") << "Command > \"x\"" << "Trigger 'Command' must be compared with '=' or '!='";
        QTest::newRow("pos component") << "Pos > 3" << "Trigger 'Pos' needs one of: X, Y";
        QTest::newRow("bad const") << "Const(data.nope)" << "Unknown constant 'data.nope'";
        QTest::newRow("bad hitattr") << "HitDefAttr = SC, XX" << "Unknown hit attribute/flag";
        QTest::newRow("redirect comma") << "parent Var(1)" << "Syntax error - ',' expected after redirection 'parent'";
        QTest::newRow("args on plain") << "Time(3)" << "Trigger 'Time' does not take arguments";
        QTest::newRow("unterminated string") << "Command = \"abc" << "Unterminated string";
        QTest::newRow("timemod comma") << "TimeMod = 10" << "Syntax error - ',' expected";
        QTest::newRow("version") << "CameraPos X > 0" << "";
    }

    void invalidExpressions()
    {
        QFETCH(QString, expr);
        QFETCH(QString, message);
        const IssueList issues = parse(expr, message.isEmpty() ? QStringLiteral("1.0") : QStringLiteral("1.1"));
        QVERIFY(!issues.isEmpty());
        if (!message.isEmpty())
            QCOMPARE(issues.first().message, message);
        else
            QVERIFY(issues.first().message.contains(QStringLiteral("current project version")));
    }

    void issuePositions()
    {
        const IssueList issues = parse(QStringLiteral("Time > 3 && Tiem"));
        QCOMPARE(issues.size(), 1);
        QCOMPARE(issues.first().column, 12);
        QCOMPARE(issues.first().length, 4);
    }

    void keySequences()
    {
        for (const char* ok : {"~D, DF, F, x", "/$B, a+b", "~30$D, >F, x+y", "s", "~F, F", "UB, U, UF"})
            QVERIFY2(MugenExpressionChecker::isValidKeySequence(QString::fromLatin1(ok)), ok);
        for (const char* bad : {"", "D, Q", "x++y", "~D, DF, F, w"})
            QVERIFY2(!MugenExpressionChecker::isValidKeySequence(QString::fromLatin1(bad)), bad);
    }

    void validateCnsFile()
    {
        const QString cns = QStringLiteral("[Data]\n"
                                           "life = 1000\n"
                                           "attack = 100\n"
                                           "power = 3000 ; unknown in this database\n"
                                           "IntPersistIndex = 60\n"
                                           "[Velocity]\n"
                                           "walk.fwd = 2.4, 0\n"
                                           "jump.neu = 0,-8.4, 9\n"
                                           "[Statedef 200]\n"
                                           "type = S\n"
                                           "movetype = Q\n"
                                           "velset = 0, 0\n"
                                           "[State 200, 1]\n"
                                           "type = ChangeState\n"
                                           "trigger1 = AnimTime = 0\n"
                                           "value = 0\n"
                                           "ctrl = 1\n"
                                           "[State 200, 2]\n"
                                           "type = ChangeStat\n"
                                           "trigger1 = 1\n"
                                           "[State 200, 3]\n"
                                           "type = HitDef\n"
                                           "trigger1 = Time = 0\n"
                                           "attr = S, NA\n"
                                           "hitflag = MAF\n"
                                           "guardflag = MA\n"
                                           "damage = 30, 5\n"
                                           "sparkno = S2\n"
                                           "priority = 4, Hit\n"
                                           "animtype = Med\n"
                                           "[State 200, 4]\n"
                                           "type = VarSet\n"
                                           "triggerall = Var(1) > 0\n"
                                           "trigger1 = Time > 1\n"
                                           "trigger2 = 1\n"
                                           "var(5) = Var(5) + 1\n"
                                           "[State 200, 5]\n"
                                           "type = ChangeState\n"
                                           "value = 0\n"
                                           "[State 200, 6]\n"
                                           "type = Width\n"
                                           "trigger1 = 1\n"
                                           "value = 10, 10\n"
                                           "[Unknown Section]\n"
                                           "x = 1\n");
        const IssueList issues = validateMugenCode(m_db, QStringLiteral("state"), QStringLiteral("1.0"), cns);
        QStringList messages;
        for (const Issue& i : issues)
            messages << QStringLiteral("%1: %2").arg(i.line + 1).arg(i.message);
        const QStringList expected = {
            "4: Unknown parameter 'power' for 'Data' block",
            "5: Invalid parameter for current project version",
            "8: Too many values for parameter 'jump.neu' (2 at most)",
            "11: Unknown keyword 'Q'",
            "19: Unknown type 'ChangeStat'",
            "37: Missing parameter 'trigger1'",
            "40: Block deprecated in 1.0",
            "44: Unknown block header",
        };
        QStringList sortedMessages = messages;
        QStringList sortedExpected = expected;
        sortedMessages.sort();
        sortedExpected.sort();
        QVERIFY2(sortedMessages == sortedExpected, qPrintable(sortedMessages.join(QStringLiteral(" | "))));
    }

    void validateCmdFile()
    {
        const QString cmd = QStringLiteral("[Command]\nname = \"QCF_x\"\ncommand = ~D, DF, F, x\ntime = 15\n"
                                           "[Command]\nname = \"bad\"\ncommand = D, Q\n"
                                           "[Statedef -1]\n"
                                           "[State -1, Combo]\ntype = ChangeState\nvalue = 1000\n"
                                           "triggerall = Command = \"QCF_x\"\ntrigger1 = StateType = S && Ctrl\n");
        const IssueList issues = validateMugenCode(m_db, QStringLiteral("command"), QStringLiteral("1.1"), cmd);
        QCOMPARE(issues.size(), 1);
        QCOMPARE(issues.first().line, 6);
        QVERIFY(issues.first().message.startsWith(QStringLiteral("Invalid command element")));
    }

    void matchingRules()
    {
        SyntaxDatabase db;
        QVERIFY(db.loadFromData(R"(<syntax>
  <file type="stage">
    <block type="Info"><param name="name" type="string"/><param name="displayname" type="string" minver="1.0"/></block>
    <block type="BGDef"><param name="spr" type="file"/></block>
    <block type="BG>Normal"><param name="spriteno" type="int2"/><param name="start" type="float2"/></block>
    <block type="BG>Parallax:Normal"><param name="xscale" type="float2"/></block>
    <block type="BGCtrlDef"><param name="looptime" type="int"/></block>
    <block type="BGCtrl>Visible"><param name="value" type="int"/></block>
  </file>
  <file type="storyboard">
    <block type="SceneDef"><param name="spr" type="file"/></block>
    <block type="Scene"><param name="end.time" type="int"/><param name="bg.name" type="string"/></block>
    <block type="BG*Def:stage.BGDef"/>
    <block type="BG*>Normal:stage.BG.Normal"/>
    <block type="BG*CtrlDef:stage.BGCtrlDef"/>
    <block type="BG*Ctrl>Visible:stage.BGCtrlDef.Visible"/>
  </file>
  <file type="font">
    <block type="Def"><param name="type" type="keyword" list="bitmap"/><param name="size" type="int2"/></block>
    <block type="Def>TrueType"><param name="file" type="file"/></block>
    <block type="Credits" raw="true"/>
  </file>
  <file type="state">
    <block type="Quotes"><param name="victory*" type="string"/></block>
    <block type="State>HitDef"><param name="priority" type="expkeyword" list="Hit,Miss,Dodge"/>
      <param name="p2getp1state" type="keyword" list="Yes,No"/><param name="ground.type" type="keyword" list="High,Low,Trip"/>
      <param name="posttype" type="keyword" list="P1,P2,Front"/></block>
    <block type="State>HitOverride"><param name="attr" type="hitattr"/></block>
  </file>
</syntax>)"));
        auto messages = [&db](const QString& fileType, const QString& version, const QString& text) {
            QStringList list;
            for (const Issue& i : validateMugenCode(db, fileType, version, text))
                list << QStringLiteral("%1: %2").arg(i.line + 1).arg(i.message);
            return list;
        };

        // The most literal characters win, so [BG0Def] is a BG*Def.
        QCOMPARE(db.sectionFor("storyboard", "BG0Def"), QStringLiteral("BG*Def"));
        QCOMPARE(db.sectionFor("storyboard", "BG0CtrlDef"), QStringLiteral("BG*CtrlDef"));
        QCOMPARE(db.sectionFor("storyboard", "BG0 sky"), QStringLiteral("BG*"));
        QVERIFY(!db.isTypedSection("storyboard", "BG0Def"));
        const SyntaxBlock* visible = db.findBlock("storyboard", "BG0Ctrl blink", "visible");
        QVERIFY(visible);
        QVERIFY(db.param("storyboard", *visible, "value"));
        QVERIFY(db.param("storyboard", *db.findBlock("storyboard", "BG0 sky", "normal"), "spriteno"));
        QVERIFY(db.param("stage", *db.findBlock("stage", "BG far", "parallax"), "spriteno"));

        QCOMPARE(messages("stage", "1.0", "[BGDef]\nspr = a.sff\n[BG a]\ntype = norm\nspriteno = 0,0\n"),
                 QStringList());
        QCOMPARE(messages("font", "1.0", "[Def]\ntype = truetype font\nfile = arial.ttf\n"), QStringList());
        QCOMPARE(messages("font", "1.0", "[Def]\ntype = bitmap\nsize = 8,8\n"), QStringList());
        QCOMPARE(messages("font", "1.0", "[Def]\nsize = 8,8\n"), QStringList());
        QCOMPARE(messages("stage", "1.0", "[BG a]\ntype = n\n"), QStringList());
        QCOMPARE(messages("stage", "1.0", "[BG a]\ntype = x\n"), QStringList{"2: Unknown type 'x'"});
        QCOMPARE(messages("stage", "1.0", "[BG a]\ntype = normal\ntype = normal\n"),
                 QStringList{"3: Duplicate parameter 'type' on group 'BG a'"});
        QCOMPARE(messages("state", "1.0", "[State 0]\ntype = Hit\ntrigger1 = 1\n"),
                 QStringList{"2: Unknown type 'Hit'"});

        QCOMPARE(messages("font", "1.0", "[Credits]\nAnything goes here\nkey = value\n"), QStringList());
        QCOMPARE(messages("state", "1.0", "[ja.Quotes]\nvictory1 = \"x\"\n"), QStringList());

        QCOMPARE(messages("state", "1.0",
                          "[State 0]\ntype = HitDef\ntrigger1 = 1\npriority = 3\n[State 0]\ntype = HitDef\n"
                          "trigger1 = 1\npriority = 3, Miss\np2getp1state = Y\nground.type = Hi\n"),
                 QStringList());
        QCOMPARE(messages("state", "1.0", "[State 0]\ntype = HitDef\ntrigger1 = 1\nposttype = P\npriority = 1, Q\n"),
                 (QStringList{"4: Unknown keyword 'P'", "5: Unknown keyword 'Q'"}));

        const QString story = QStringLiteral("[SceneDef]\nspr = s.sff\n[Scene 0]\nbg.name = Intro\nend.time = 10\n"
                                             "[IntroDef]\nspr = s.sff\n[Intro sky]\ntype = normal\nspriteno = 1,0\n"
                                             "[IntroCtrlDef]\nlooptime = 10\n[IntroCtrl 1]\ntype = visible\n"
                                             "value = 0\n");
        QCOMPARE(messages("storyboard", "1.0", story), QStringList());

        const QString info = QStringLiteral("[Info]\nname = \"x\"\ndisplayname = \"y\"\n");
        QCOMPARE(messages("stage", "winbeta", info), QStringList{"3: Invalid parameter for current project version"});
        QCOMPARE(validateMugenCode(db, "stage", "winbeta", info).first().severity, Issue::Severity::Warning);
        QCOMPARE(messages("stage", QString(), info), QStringList());
    }
};

QTEST_GUILESS_MAIN(MugenSyntaxTest)
#include "MugenSyntaxTest.moc"
