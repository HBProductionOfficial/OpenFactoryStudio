#include "engines/mugen/runtime/Character.h"
#include "engines/mugen/runtime/CharacterData.h"

#include <QTest>

using namespace OFS::Mugen::Runtime;

namespace {

// 5900 starts the round (190 for the first one's intro), 0 is idle, 180/170/175 note the pose.
// Var 1 asks for a KO: the character sets its life to 0.
const char* kStates = "[Statedef 5900]\ntype = S\n"
                      "[State 5900]\ntype = ChangeState\ntrigger1 = roundno = 1\nvalue = 190\n"
                      "[State 5900, b]\ntype = ChangeState\ntrigger1 = 1\nvalue = 0\n"
                      "[Statedef 190]\ntype = S\nctrl = 0\n"
                      "[State 190]\ntype = AssertSpecial\ntrigger1 = time < 3\nflag = intro\n"
                      "[State 190, b]\ntype = ChangeState\ntrigger1 = time = 3\nvalue = 0\n"
                      "[Statedef 0]\ntype = S\nphysics = S\n"
                      "[State 0, ko]\ntype = LifeSet\ntrigger1 = var(1) = roundno\nvalue = 0\n"
                      "[State 0, rs]\ntype = VarSet\ntrigger1 = 1\nv = 2\nvalue = RoundState\n"
                      "[Statedef 5030]\ntype = A\nmovetype = H\nphysics = N\n"
                      "[State 5030]\ntype = Null\ntrigger1 = 1\n"
                      "[Statedef 180]\ntype = S\n[State 180]\ntype = VarSet\ntrigger1 = 1\nv = 3\nvalue = 180\n"
                      "[State 180, b]\ntype = VarSet\ntrigger1 = 1\nv = 4\nvalue = WinKO + 2 * WinPerfect\n"
                      "[Statedef 170]\ntype = S\n[State 170]\ntype = VarSet\ntrigger1 = 1\nv = 3\nvalue = 170\n"
                      "[Statedef 175]\ntype = S\n[State 175]\ntype = VarSet\ntrigger1 = 1\nv = 3\nvalue = 175\n"
                      "[State 175, b]\ntype = VarSet\ntrigger1 = 1\nv = 4\nvalue = DrawGame\n";

RoundSettings quick()
{
    RoundSettings s;
    s.wins = 2;
    s.time = 2;
    s.framesPerCount = 10;
    s.startWait = 4;
    s.ctrlTime = 3;
    s.overHitTime = 2;
    s.overWaitTime = 5;
    s.overWinTime = 3;
    s.overTime = 10;
    return s;
}

} // namespace

class RuntimeRoundTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void phases()
    {
        CharacterData data;
        data.addStates(kStates);
        Match match;
        Character a(data, &match);
        Character b(data, &match);
        match.startMatch(quick());
        QCOMPARE(match.roundNo(), 1);
        QCOMPARE(match.roundState(), 0);
        QCOMPARE(a.stateNo(), 5900);
        QVERIFY(!a.ctrl());
        QCOMPARE(a.pos().x(), -70.0);
        QCOMPARE(b.pos().x(), 70.0);
        QCOMPARE(b.facing(), -1);

        QList<int> states;
        int fightTick = -1;
        for (int t = 1; t <= 30 && fightTick < 0; ++t) {
            match.tick();
            states << match.roundState();
            if (match.roundState() == 2)
                fightTick = t;
        }
        QVERIFY(states.mid(0, 3) == (QList<int>{0, 0, 0}));
        QVERIFY(states.contains(1));
        QVERIFY(fightTick > 0);
        QCOMPARE(a.stateNo(), 0);
        QVERIFY(a.ctrl());
        QCOMPARE(match.roundTime(), 19);
        match.tick();
        QCOMPARE(a.var(2), 2);
        QCOMPARE(match.roundTime(), 18);
    }

    void timeOverDraw()
    {
        CharacterData data;
        data.addStates(kStates);
        Match match;
        Character a(data, &match);
        Character b(data, &match);
        match.startMatch(quick());
        while (match.roundState() != 3 && match.gameTime() < 200)
            match.tick();
        QCOMPARE(match.roundEnd(), RoundEnd::TimeOver);
        QCOMPARE(match.roundWinner(), 2);
        QCOMPARE(match.roundTime(), 0);
        while (a.var(3) == 0 && match.gameTime() < 300)
            match.tick();
        QCOMPARE(a.var(3), 175);
        QCOMPARE(b.var(3), 175);
        QCOMPARE(a.var(4), 1);
        QCOMPARE(match.roundState(), 4);
        while (match.roundNo() == 1 && match.gameTime() < 400)
            match.tick();
        QCOMPARE(match.roundNo(), 2);
        QCOMPARE(match.wins(0), 0);
        QCOMPARE(match.wins(1), 0);
    }

    void knockOut()
    {
        CharacterData data;
        data.addStates(kStates);
        Match match;
        Character a(data, &match);
        Character b(data, &match);
        match.startMatch(quick());
        while (match.roundState() != 2)
            match.tick();
        b.setVar(1, 1);
        while (b.alive() && match.gameTime() < 100)
            match.tick();
        QVERIFY(!b.alive());
        QVERIFY(!b.ctrl());
        QCOMPARE(b.stateNo(), 5030);
        match.tick();
        QCOMPARE(match.roundEnd(), RoundEnd::KO);
        QCOMPARE(match.roundWinner(), 0);
        QVERIFY(match.perfect());
        while (a.var(3) == 0 && match.gameTime() < 300)
            match.tick();
        QCOMPARE(a.var(3), 180);
        QCOMPARE(a.var(4), 3); // WinKO and WinPerfect
        QCOMPARE(b.var(3), 0);
        QCOMPARE(match.wins(0), 1);

        while (match.roundNo() == 1 && match.gameTime() < 500)
            match.tick();
        QCOMPARE(match.roundNo(), 2);
        QVERIFY(b.alive());
        QCOMPARE(b.life(), 1000);
        while (match.roundState() != 2 && match.gameTime() < 600)
            match.tick();
        QCOMPARE(a.stateNo(), 0);
        b.setVar(1, 2);
        while (!match.matchOver() && match.gameTime() < 900)
            match.tick();
        QVERIFY(match.matchOver());
        QCOMPARE(match.wins(0), 2);
    }
};

QTEST_GUILESS_MAIN(RuntimeRoundTest)
#include "RuntimeRoundTest.moc"
