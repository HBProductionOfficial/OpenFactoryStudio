#include "engines/mugen/runtime/AnimPlayer.h"
#include "engines/mugen/runtime/Character.h"
#include "engines/mugen/runtime/CharacterData.h"

#include <QTest>

using namespace OFS::Mugen::Runtime;

namespace {

// Two animations: 0 (3 frames of 2, 3 and 1 ticks, looping from the second) and 1 (one endless frame).
const char* kAir = "[Begin Action 0]\n0,0, 0,0, 2\nLoopStart\n0,1, 0,0, 3\n0,2, 0,0, 1\n\n"
                   "[Begin Action 1]\n1,0, 0,0, -1\n\n"
                   "[Begin Action 52]\n52,0, 0,0, 3\n";

} // namespace

class RuntimeStateTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void animPlayer()
    {
        AnimPlayer anim({2, 3, 1}, 1);
        QCOMPARE(anim.totalTime(), 6);
        QCOMPARE(anim.frame(), 0);
        QCOMPARE(anim.animTime(), -6);
        QCOMPARE(anim.animElemTime(1), 0);
        QCOMPARE(anim.animElemTime(2), -2);
        QList<int> frames;
        QList<int> times;
        for (int t = 0; t < 10; ++t) {
            anim.step();
            frames << anim.frame();
            times << anim.animTime();
        }
        QCOMPARE(frames, (QList<int>{0, 1, 1, 1, 2, 1, 1, 1, 2, 1}));
        QCOMPARE(times.mid(0, 7), (QList<int>{-5, -4, -3, -2, -1, 0, -3}));
        AnimPlayer again({2, 3, 1}, 1);
        QCOMPARE(again.animElemNo(0), 1);
        QCOMPARE(again.animElemNo(2), 2);
        QCOMPARE(again.animElemNo(5), 3);
        again.setElement(3);
        QCOMPARE(again.frame(), 2);
        QCOMPARE(again.animElemTime(3), 0);
        QCOMPARE(again.animElemTime(1), 5);

        AnimPlayer endless({4, -1}, 0);
        QCOMPARE(endless.totalTime(), -1);
        for (int t = 0; t < 20; ++t)
            endless.step();
        QCOMPARE(endless.frame(), 1);
    }

    void stateMachine()
    {
        CharacterData data;
        data.setAnimations(kAir);
        data.addStates("[Statedef 0]\ntype = S\nphysics = S\nanim = 0\nctrl = 1\nvelset = 0,0\n"
                       "[State 0, Count]\ntype = VarAdd\ntrigger1 = 1\nv = 1\nvalue = 1\n"
                       "[State 0, Once]\ntype = VarAdd\ntrigger1 = 1\nv = 2\nvalue = 1\npersistent = 0\n"
                       "[State 0, Every third]\ntype = VarAdd\ntrigger1 = 1\nv = 3\nvalue = 1\npersistent = 3\n"
                       "[State 0, Go]\ntype = ChangeState\ntrigger1 = time = 4\nvalue = 200\n"
                       "\n"
                       "[Statedef 200]\ntype = S\nmovetype = A\nphysics = S\nanim = 1\nctrl = 0\n"
                       "[State 200, At once]\ntype = VarSet\ntrigger1 = time = 0\nvar(10) = stateno + prevstateno\n"
                       "[State 200, Move]\ntype = VelSet\ntrigger1 = time = 0\nx = 5\n"
                       "[State 200, Back]\ntype = ChangeState\ntrigger1 = time = 3\nvalue = 0\nctrl = 1\n",
                       "test.cns");
        QVERIFY2(data.issues().isEmpty(),
                 qPrintable(data.issues().isEmpty() ? QString() : data.issues().first().message));

        Character c(data);
        c.setTracing(true);
        c.start(0);
        c.tick();
        QCOMPARE(c.stateNo(), 0);
        QCOMPARE(c.animNo(), 0);
        QVERIFY(c.ctrl());
        QCOMPARE(c.var(1), 1);
        QCOMPARE(c.var(2), 1);
        QCOMPARE(c.var(3), 1);
        for (int t = 1; t <= 3; ++t)
            c.tick();
        QCOMPARE(c.time(), 4);
        QCOMPARE(c.var(1), 4);
        QCOMPARE(c.var(2), 1); // persistent = 0: once per state
        QCOMPARE(c.var(3), 2); // persistent = 3: ticks 0 and 3

        // ChangeState at time 4: the new state's controllers run in the same tick.
        c.tick();
        QCOMPARE(c.stateNo(), 200);
        QCOMPARE(c.prevStateNo(), 0);
        QCOMPARE(c.var(10), 200);
        QCOMPARE(c.moveType(), QLatin1Char('A'));
        QVERIFY(!c.ctrl());
        QCOMPARE(c.animNo(), 1);
        // VelSet x = 5 moved the character forward this tick, then friction: 5 * 0.85.
        QCOMPARE(c.pos().x(), 5.0);
        QCOMPARE(float(c.vel().x()), 5 * 0.85f);
        QCOMPARE(c.time(), 1);

        for (int t = 0; t < 3; ++t)
            c.tick();
        QCOMPARE(c.stateNo(), 0);
        QVERIFY(c.ctrl());
        QCOMPARE(c.var(2), 2);
        QVERIFY(c.trace().contains("0: Go"));
        QVERIFY(c.messages().isEmpty());
    }

    void triggersAndPhysics()
    {
        CharacterData data;
        data.setAnimations(kAir);
        data.addConstants("[Data]\nlife = 900\n[Velocity]\njump.neu = 0,-8\n[Movement]\nyaccel = .5\n");
        data.addStates("[Statedef 40]\ntype = A\nphysics = A\nanim = 0\n"
                       "[State 40, Jump]\ntype = VelSet\ntrigger1 = time = 0\ny = const(velocity.jump.neu.y)\n"
                       "[State 40, Mark]\ntype = VarSet\ntrigger1 = AnimElem = 2\nv = 5\nvalue = time\n"
                       "[State 40, Mark2]\ntype = VarSet\ntrigger1 = AnimElem = 2, >= 1\ntrigger1 = var(6) = 0\n"
                       "v = 6\nvalue = time\n"
                       "[State 40, Life]\ntype = VarSet\ntrigger1 = 1\nv = 7\nvalue = life + lifemax\n"
                       "\n"
                       "[Statedef 52]\ntype = S\nphysics = S\nanim = 52\nvelset = 0,0\n"
                       "[State 52, Ground]\ntype = PosSet\ntrigger1 = time = 0\ny = 0\n",
                       "jump.cns");
        Character c(data);
        c.start(40);
        c.tick();
        QCOMPARE(float(c.vel().y()), -8 + 0.5f);
        QCOMPARE(c.pos().y(), -8.0);
        QCOMPARE(c.var(7), 1800);
        int ticks = 1;
        while (c.stateNo() == 40 && ticks < 100) {
            c.tick();
            ++ticks;
        }
        // Up at 8 per tick, slowing by 0.5: back on the ground after 32 ticks, into state 52.
        QCOMPARE(c.stateNo(), 52);
        QCOMPARE(ticks, 33);
        QCOMPARE(c.pos().y(), 0.0);
        QCOMPARE(c.animNo(), 52);
        QCOMPARE(c.var(5), 2);
        QCOMPARE(c.var(6), 3);
    }

    void messages()
    {
        CharacterData data;
        data.addStates("[Statedef 0]\n[State 0, x]\ntype = Frobnicate\ntrigger1 = 1\n"
                       "[State 0, y]\ntype = ChangeState\ntrigger1 = time = 1\nvalue = 77\n",
                       "bad.cns");
        Character c(data);
        c.start(0);
        c.tick();
        c.tick();
        QCOMPARE(c.stateNo(), 77);
        QCOMPARE(c.messages().size(), 2);
        QVERIFY(c.messages()[0].startsWith("unknown controller type 'frobnicate'"));
        QCOMPARE(c.messages()[1], QStringLiteral("changed to invalid state 77 (from state 0)"));
    }

    void loopsStop()
    {
        CharacterData data;
        data.addStates("[Statedef 0]\n[State 0]\ntype = ChangeState\ntrigger1 = 1\nvalue = 1\n"
                       "[Statedef 1]\n[State 1]\ntype = ChangeState\ntrigger1 = 1\nvalue = 0\n");
        Character c(data);
        c.start(0);
        c.tick();
        QVERIFY(!c.messages().isEmpty());
        QVERIFY(c.messages().first().startsWith("state machine stuck in loop"));
    }

    void pushBoundsAndTurning()
    {
        CharacterData data;
        data.setAnimations("[Begin Action 0]\n0,0, 0,0, -1\n\n[Begin Action 5]\n5,0, 0,0, 3\n\n"
                           "[Begin Action 20]\n20,0, 0,0, -1\n");
        data.addStates("[Statedef 0]\ntype = S\nphysics = S\nctrl = 1\n"
                       "[State 0]\ntype = VarSet\ntrigger1 = 1\nv = 0\nvalue = BackEdgeDist\n"
                       "[State 0, b]\ntype = VarSet\ntrigger1 = 1\nv = 1\nvalue = FrontEdgeBodyDist\n"
                       "[Statedef 20]\ntype = S\nphysics = S\nanim = 20\nctrl = 1\n"
                       "[State 20]\ntype = VelSet\ntrigger1 = 1\nx = 3\n");
        Match match;
        Character a(data, &match);
        Character b(data, &match);
        a.setPos({-5, 0});
        b.setPos({5, 0});
        b.setFacing(-1);
        a.start(0);
        b.start(0);
        match.tick();
        QVERIFY(b.pos().x() - a.pos().x() >= 31 - 0.001); // front widths 16 + 15
        QCOMPARE(a.pos().x(), -b.pos().x());

        a.start(20);
        for (int i = 0; i < 30; ++i)
            match.tick();
        QVERIFY(a.pos().x() < b.pos().x());

        // The screen keeps them within 320 units (15 from each edge).
        Match far;
        Character c(data, &far);
        Character d(data, &far);
        c.setPos({-400, 0});
        d.setPos({400, 0});
        d.setFacing(-1);
        c.start(0);
        d.start(0);
        far.tick();
        far.tick(); // the triggers see where the last tick left them
        QCOMPARE(d.pos().x() - c.pos().x(), 290.0);
        QCOMPARE(far.cameraX(), 0.f);
        QCOMPARE(c.var(0), 15);
        QCOMPARE(c.var(1), 305 - 16);

        Match turn;
        Character e(data, &turn);
        Character f(data, &turn);
        e.setPos({50, 0});
        f.setPos({-50, 0});
        e.start(0);
        f.start(0);
        turn.tick();
        QCOMPARE(e.facing(), -1);
        QCOMPARE(e.animNo(), 5);
    }

    void matchAndRedirection()
    {
        CharacterData data;
        data.addStates("[Statedef 0]\n[State 0]\ntype = VarSet\ntrigger1 = 1\nv = 0\nvalue = enemy, stateno\n"
                       "[State 0, d]\ntype = VarSet\ntrigger1 = 1\nv = 1\nvalue = p2dist x\n"
                       "[Statedef 5]\n");
        Match match(7);
        Character a(data, &match);
        Character b(data, &match);
        a.setPos({-50, 0});
        b.setPos({50, 0});
        b.setFacing(-1);
        a.start(0);
        b.start(5);
        match.tick();
        QCOMPARE(a.var(0), 5);
        QCOMPARE(a.var(1), 100);
        QCOMPARE(match.gameTime(), 1);
        QVERIFY(b.id() != a.id());
        Match one(42);
        Match two(42);
        for (int i = 0; i < 5; ++i)
            QCOMPARE(one.random(1000), two.random(1000));
    }
};

QTEST_MAIN(RuntimeStateTest)
#include "RuntimeStateTest.moc"
