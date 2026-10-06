#include "engines/mugen/runtime/Character.h"
#include "engines/mugen/runtime/CharacterData.h"

#include <QTest>

using namespace OFS::Mugen::Runtime;

namespace {

const char* kIdle = "[Statedef 0]\ntype = S\nphysics = S\n[State 0]\ntype = Null\ntrigger1 = 1\n";

Value eval(Character& c, const QString& text)
{
    QString error;
    const Expr expr = Expr::compile(text, &error);
    if (!error.isEmpty())
        qWarning("%s: %s", qPrintable(text), qPrintable(error));
    return expr.evaluate(c);
}

bool isInt(const Value& v, int expected)
{
    return v.type == Value::Type::Int && v.i == expected;
}

bool isFloat(const Value& v, float expected)
{
    return v.type == Value::Type::Float && qFuzzyCompare(v.f + 1, expected + 1);
}

} // namespace

class RuntimeTriggerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void localCoord()
    {
        // Each character reads positions and the screen in its own units, also through a redirection.
        CharacterData big;
        big.setLocalCoordWidth(640);
        big.addStates(kIdle);
        CharacterData small;
        small.addStates(kIdle);
        Match match;
        Character a(big, &match);
        Character b(small, &match);
        a.start(0);
        b.start(0);
        a.setPos({-100, 0}); // its own units: 50 game units left of the middle
        b.setPos({50, 0});
        b.setFacing(-1);
        match.tick();
        QCOMPARE(a.gamePos().x(), -50.0);
        QVERIFY(isFloat(eval(a, "Pos X"), -100));
        QVERIFY(isFloat(eval(b, "Pos X"), 50));
        QVERIFY(isFloat(eval(a, "P2Dist X"), 200));
        QVERIFY(isFloat(eval(b, "P2Dist X"), 100));
        QVERIFY(isFloat(eval(a, "enemy, Pos X"), 100));
        QVERIFY(isFloat(eval(b, "enemy, Pos X"), -50));
        QVERIFY(isFloat(eval(a, "ScreenWidth"), 640));
        QVERIFY(isFloat(eval(a, "GameWidth"), 640));
        QVERIFY(isFloat(eval(b, "GameWidth"), 320));
        QVERIFY(isFloat(eval(a, "RightEdge"), 320));
        QVERIFY(isFloat(eval(a, "Const240p(10)"), 20));
        QVERIFY(isFloat(eval(a, "Const720p(40)"), 20));
        QVERIFY(isFloat(eval(b, "Const720p(40)"), 10));
        QVERIFY(isInt(eval(a, "enemy, Const(size.ground.front)"), 32)); // 16 of the smaller one
        QVERIFY(isFloat(eval(a, "enemy, Const(size.xscale)"), 1));
    }

    void names()
    {
        CharacterData kfm;
        kfm.setName(QStringLiteral("Kung Fu Man"));
        kfm.setAuthor(QStringLiteral("Elecbyte"));
        kfm.addStates("[Statedef 0]\n"
                      "[State 0, a]\ntype = Helper\ntrigger1 = time = 0\nid = 1\nname = \"Shadow\"\nstateno = 1000\n"
                      "[State 0, b]\ntype = Helper\ntrigger1 = time = 0\nid = 2\nstateno = 1000\n"
                      "[Statedef 1000]\n[State 1000]\ntype = Null\ntrigger1 = 1\n");
        CharacterData other;
        other.setName(QStringLiteral("Suave Dude"));
        other.addStates(kIdle);
        Match match;
        Character a(kfm, &match);
        Character b(other, &match);
        a.start(0);
        b.start(0);
        match.tick();
        QVERIFY(isInt(eval(a, "Name = \"kung fu man\""), 1));
        QVERIFY(isInt(eval(a, "P1Name != \"Suave Dude\""), 1));
        QVERIFY(isInt(eval(a, "P2Name = \"Suave Dude\""), 1));
        QVERIFY(isInt(eval(b, "P2Name = \"Kung Fu Man\""), 1));
        QVERIFY(isInt(eval(a, "AuthorName = \"Elecbyte\""), 1));
        QVERIFY(isInt(eval(a, "enemy, Name = \"Suave Dude\""), 1));
        QVERIFY(isInt(eval(a, "P3Name = \"Kung Fu Man\""), 0));
        QVERIFY(isInt(eval(a, "P4Name != \"Suave Dude\""), 1));
        QVERIFY(isInt(eval(a, "helper(1), Name = \"Shadow\""), 1));
        QVERIFY(isInt(eval(a, "helper(2), Name = \"Kung Fu Man's helper\""), 1));
        QCOMPARE(match.characters().last()->name(), QStringLiteral("Kung Fu Man's helper"));
    }

    void screenAndCamera()
    {
        CharacterData data;
        data.addStates(kIdle);
        Match match;
        match.setBounds(-400, 400);
        match.setScreenHeight(240, 190);
        Character c(data, &match);
        c.setPos({100, -30});
        c.start(0);
        match.tick();
        QCOMPARE(match.cameraX(), 100.f);
        QVERIFY(isFloat(eval(c, "ScreenPos X"), 160));
        QVERIFY(isFloat(eval(c, "ScreenPos Y"), 160)); // 30 above the ground line, 190 down
        QVERIFY(isFloat(eval(c, "CameraPos X"), 100));
        QVERIFY(isFloat(eval(c, "CameraPos Y"), 0));
        QVERIFY(isFloat(eval(c, "CameraZoom"), 1));
        QVERIFY(isFloat(eval(c, "ScreenWidth"), 320));
        QVERIFY(isFloat(eval(c, "ScreenHeight"), 240));
        QVERIFY(isFloat(eval(c, "GameWidth"), 320));
        QVERIFY(isFloat(eval(c, "GameHeight"), 240));
        QVERIFY(isFloat(eval(c, "LeftEdge"), -60));
        QVERIFY(isFloat(eval(c, "RightEdge"), 260));
        QVERIFY(isFloat(eval(c, "BackEdge"), -60));
        QVERIFY(isFloat(eval(c, "FrontEdge"), 260));
        QVERIFY(isFloat(eval(c, "TopEdge"), -190));
        QVERIFY(isFloat(eval(c, "BottomEdge"), 50));
        // TopEdge is Pos Y - ScreenPos Y, as the engine defines it.
        QVERIFY(isFloat(eval(c, "Pos Y - ScreenPos Y"), -190));
        c.setFacing(-1);
        QVERIFY(isFloat(eval(c, "BackEdge"), 260));
        QVERIFY(isFloat(eval(c, "FrontEdge"), -60));
    }

    void playersAndTeams()
    {
        CharacterData data;
        data.addStates(kIdle);
        Match match;
        Character a(data, &match);
        Character b(data, &match);
        a.start(0);
        b.start(0);
        match.tick();
        QVERIFY(isInt(eval(a, QStringLiteral("PlayerIDExist(%1)").arg(b.id())), 1));
        QVERIFY(isInt(eval(a, "PlayerIDExist(id + 100000)"), 0));
        QVERIFY(isInt(eval(a, "IsHomeTeam"), 1));
        QVERIFY(isInt(eval(b, "IsHomeTeam"), 0));
        QVERIFY(isInt(eval(a, "TeamMode = Single"), 1));
        QVERIFY(isInt(eval(a, "TeamMode = Simul"), 0));
        QVERIFY(isInt(eval(a, "TeamMode != Turns"), 1));
    }

    void stageVar()
    {
        CharacterData data;
        data.addStates(kIdle);
        Match match;
        match.setStageInfo(QStringLiteral("info.name"), QStringLiteral("Training Room"));
        match.setStageInfo(QStringLiteral("info.author"), QStringLiteral("Elecbyte"));
        Character c(data, &match);
        c.start(0);
        match.tick();
        QVERIFY(isInt(eval(c, "StageVar(info.name) = \"training room\""), 1));
        QVERIFY(isInt(eval(c, "StageVar(Info.Author) = \"Elecbyte\""), 1));
        QVERIFY(isInt(eval(c, "StageVar(info.authorname) != \"Elecbyte\""), 0));
        QVERIFY(isInt(eval(c, "StageVar(info.displayname) = \"Training Room\""), 0));
    }

    void constants()
    {
        CharacterData data;
        data.addStates("[Statedef 0]\n"
                       "[State 0]\ntype = Helper\ntrigger1 = time = 0\nid = 1\nstateno = 1000\n"
                       "size.ground.front = 30\n"
                       "[Statedef 1000]\n[State 1000]\ntype = Null\ntrigger1 = 1\n");
        Match match;
        Character c(data, &match);
        c.start(0);
        match.tick();
        QVERIFY(isInt(eval(c, "Const(size.ground.front)"), 16));
        QVERIFY(isInt(eval(c, "Const(size.ground.front) / 3"), 5));
        QVERIFY(isFloat(eval(c, "Const(size.xscale)"), 1));
        QVERIFY(isInt(eval(c, "Const(data.life)"), 1000));
        QVERIFY(isInt(eval(c, "Const(movement.airjump.height)"), 35));
        QVERIFY(isFloat(eval(c, "Const(movement.yaccel)"), 0.44f));
        // The fall defence multiplier comes from fall.defence_up (50): 100 / 150.
        QVERIFY(isFloat(eval(c, "Const(data.fall.defence_mul)"), 100.f / 150.f));
        QVERIFY(eval(c, "Const(size.nothing)").isUndefined());
        QVERIFY(isInt(eval(c, "helper(1), Const(size.ground.front)"), 30));
    }

    void variablesAndAnimation()
    {
        CharacterData data;
        data.setAnimations("[Begin Action 0]\n0,0, 0,0, 3\n0,1, 0,0, 3\n");
        data.addStates("[Statedef 0]\nanim = 0\n[State 0]\ntype = VarSet\ntrigger1 = 1\nv = 59\nvalue = 4\n");
        Match match;
        Character c(data, &match);
        c.start(0);
        for (int i = 0; i < 5; ++i)
            match.tick();
        QVERIFY(isInt(eval(c, "var(59)"), 4));
        QVERIFY(eval(c, "var(60)").isUndefined());
        QVERIFY(eval(c, "fvar(40)").isUndefined());
        QVERIFY(eval(c, "sysvar(5)").isUndefined());
        QVERIFY(eval(c, "sysfvar(-1)").isUndefined());
        QVERIFY(eval(c, "AnimElemTime(3)").isUndefined());
        QVERIFY(eval(c, "AnimElemTime(0)").isUndefined());
        QVERIFY(isInt(eval(c, "AnimElemTime(1)"), 5));
        QVERIFY(isInt(eval(c, "AnimElemTime(2)"), 2));
        QVERIFY(isInt(eval(c, "Time"), 5));
        QVERIFY(isInt(eval(c, "TimeMod > 4, 0"), 1));
        QVERIFY(isInt(eval(c, "TimeMod < 4, 1"), 0));
        QVERIFY(isInt(eval(c, "TimeMod = 4, 1"), 1));
        QVERIFY(isInt(eval(c, "TimeMod != 4, 1"), 0));
    }

    void animElemNo()
    {
        CharacterData data;
        data.setAnimations("[Begin Action 1]\n0,0, 0,0, 3\nLoopStart\n0,1, 0,0, 3\n");
        data.addStates("[Statedef 0]\nanim = 1\n[State 0]\ntype = Null\ntrigger1 = 1\n");
        Character c(data);
        c.start(0);
        c.tick();
        c.tick();
        QVERIFY(eval(c, "AnimElemNo(-3)").isUndefined());
        QVERIFY(isInt(eval(c, "AnimElemNo(-2)"), 1));
        QVERIFY(isInt(eval(c, "AnimElemNo(0)"), 1));
        QVERIFY(isInt(eval(c, "AnimElemNo(1)"), 2));
        for (int i = 0; i < 3; ++i)
            c.tick();
        QVERIFY(isInt(eval(c, "AnimElemNo(0)"), 2));
        QVERIFY(!eval(c, "AnimElemNo(-30)").isUndefined());
    }

    void numHelper()
    {
        CharacterData data;
        data.addStates("[Statedef 0]\n"
                       "[State 0, a]\ntype = Helper\ntrigger1 = time = 0\nid = 3\nstateno = 1000\n"
                       "[State 0, b]\ntype = Helper\ntrigger1 = time = 0\nid = 4\nstateno = 1000\n"
                       "[Statedef 1000]\n[State 1000]\ntype = Null\ntrigger1 = 1\n");
        Match match;
        Character c(data, &match);
        c.start(0);
        match.tick();
        QVERIFY(isInt(eval(c, "NumHelper"), 2));
        QVERIFY(isInt(eval(c, "NumHelper(0)"), 2));
        QVERIFY(isInt(eval(c, "NumHelper(-1)"), 2));
        QVERIFY(isInt(eval(c, "NumHelper(4)"), 1));
        QVERIFY(eval(c, "helper(1 / 0), id").isUndefined());
        QVERIFY(eval(c, "helper(4), id").toInt() > 0);
    }
};

QTEST_GUILESS_MAIN(RuntimeTriggerTest)
#include "RuntimeTriggerTest.moc"
