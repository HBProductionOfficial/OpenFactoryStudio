#include "engines/mugen/runtime/Character.h"
#include "engines/mugen/runtime/CharacterData.h"

#include <QTest>

#include <cmath>
#include <numeric>

using namespace OFS::Mugen::Runtime;

namespace {

const char* kAir = "[Begin Action 0]\nClsn2Default: 1\n Clsn2[0] = -10,-60, 10,0\n0,0, 0,0, -1\n\n"
                   "[Begin Action 120]\nClsn2Default: 1\n Clsn2[0] = -10,-60, 10,0\n120,0, 0,0, -1\n\n"
                   "[Begin Action 150]\nClsn2Default: 1\n Clsn2[0] = -10,-60, 10,0\n150,0, 0,0, -1\n\n"
                   "[Begin Action 200]\nClsn2Default: 1\n Clsn2[0] = -10,-60, 10,0\n200,0, 0,0, 2\n"
                   "Clsn1: 1\n Clsn1[0] = 0,-50, 40,-40\n200,1, 0,0, 3\n200,2, 0,0, 4\n\n"
                   "[Begin Action 5000]\nClsn2Default: 1\n Clsn2[0] = -10,-60, 10,0\n5000,0, 0,0, -1\n";

QString states(const QString& hitdef)
{
    return QStringLiteral(
               "[Statedef 0]\ntype = S\nphysics = S\nanim = 0\nctrl = 1\nvelset = 0,0\n"
               "[State 0]\ntype = Null\ntrigger1 = 1\n"
               "[Statedef 120]\ntype = S\nmovetype = I\nphysics = S\nanim = 120\nctrl = 0\n"
               "[State 120]\ntype = ChangeState\ntrigger1 = time = 30\nvalue = 0\nctrl = 1\n"
               "[Statedef 150]\ntype = S\nmovetype = H\nphysics = S\nanim = 150\n"
               "[State 150]\ntype = VarSet\ntrigger1 = time = 0\nv = 3\nvalue = 1\n"
               "[State 150, b]\ntype = ChangeState\ntrigger1 = time = 10\nvalue = 0\nctrl = 1\n"
               "[Statedef 200]\ntype = S\nmovetype = A\nphysics = S\nanim = 200\nctrl = 0\n"
               "[State 200, Hit]\ntype = HitDef\ntrigger1 = time = 0\n%1\n"
               "[State 200, Contact]\ntype = VarSet\ntrigger1 = MoveHit = 1\nv = 9\nvalue = time\n"
               "[State 200, Guarded]\ntype = VarSet\ntrigger1 = MoveGuarded\nv = 8\nvalue = 1\n"
               "[State 200, End]\ntype = ChangeState\ntrigger1 = AnimTime = 0\nvalue = 0\nctrl = 1\n"
               "[Statedef 5000]\ntype = S\nmovetype = H\nphysics = N\nanim = 5000\nvelset = 0,0\n"
               "[State 5000, a]\ntype = VarSet\ntrigger1 = time = 0\nv = 1\nvalue = GetHitVar(animtype)\n"
               "[State 5000, b]\ntype = VarSet\ntrigger1 = time = 0\nv = 2\nvalue = GetHitVar(hitshaketime)\n"
               "[State 5000, c]\ntype = ChangeState\ntrigger1 = HitShakeOver\nvalue = 5001\n"
               "[Statedef 5001]\ntype = S\nmovetype = H\nphysics = S\n"
               "[State 5001, a]\ntype = HitVelSet\ntrigger1 = time = 0\nx = 1\n"
               "[State 5001, b]\ntype = VarSet\ntrigger1 = time = 0\nv = 4\nvalue = vel x\n"
               "[State 5001, c]\ntype = ChangeState\ntrigger1 = HitOver\nvalue = 0\nctrl = 1\n")
        .arg(hitdef);
}

const char* kHit = "attr = S, NA\ndamage = 50, 10\npausetime = 5, 8\nground.velocity = -4\n"
                   "ground.hittime = 12\nguardflag = MA\nanimtype = Medium";

struct Fight
{
    explicit Fight(const QString& hitdef)
        : match(3)
        , a(data, &match)
        , b(data, &match)
    {
        data.setAnimations(kAir);
        data.addStates(states(hitdef));
        a.setPos({-20, 0});
        b.setPos({20, 0});
        b.setFacing(-1);
        a.start(200);
        b.start(0);
    }
    int untilHit(int limit = 20)
    {
        for (int t = 1; t <= limit; ++t) {
            match.tick();
            if (b.moveType() == QLatin1Char('H') || b.var(3) == 1)
                return t;
        }
        return -1;
    }
    CharacterData data;
    Match match;
    Character a;
    Character b;
};

// A second HitDef in state 200, on the first tick after the first one's pause.
QString thenHit(const QString& first, const QString& second)
{
    return first +
           QStringLiteral("\n[State 200, Second]\ntype = HitDef\ntrigger1 = MoveHit = 1 && HitPauseTime = 0\n"
                          "persistent = 0\n") +
           second;
}

Value eval(Character& c, const QString& text)
{
    QString error;
    const Expr expr = Expr::compile(text, &error);
    if (!error.isEmpty())
        qWarning("%s: %s", qPrintable(text), qPrintable(error));
    return expr.evaluate(c);
}

} // namespace

class RuntimeHitTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void boxes()
    {
        Fight f(kHit);
        f.match.tick();
        QCOMPARE(f.b.boxes(2), QList<QRectF>{QRectF(10, -60, 20, 60)});
        QVERIFY(f.a.boxes(1).isEmpty());
        f.match.tick();
        f.match.tick();
        QCOMPARE(f.a.boxes(1), QList<QRectF>{QRectF(-20, -50, 40, 10)});
    }

    void hit()
    {
        Fight f(kHit);
        const int t = f.untilHit();
        QCOMPARE(t, 3);
        QCOMPARE(f.b.life(), 950);
        QCOMPARE(f.b.stateNo(), 5000);
        QVERIFY(!f.b.ctrl());
        // The power is the damage times mugen.cfg's defaults: 0.7 for the attacker, 0.6 for the one hit.
        QCOMPARE(f.a.power(), 35);
        QCOMPARE(f.b.power(), 30);
        QCOMPARE(f.a.hitPauseTime(), 4); // the hit's tick counts
        QVERIFY(!f.a.hitDef().active);

        const int timeBefore = f.a.time();
        QCOMPARE(f.b.var(1), 0);
        f.match.tick();
        QCOMPARE(f.b.var(1), int(AnimType::Medium));
        QCOMPARE(f.b.var(2), 8);

        for (int i = 0; i < 3; ++i)
            f.match.tick();
        QCOMPARE(f.a.time(), timeBefore);
        QCOMPARE(f.a.hitPauseTime(), 0);
        QCOMPARE(f.a.var(9), 0);
        f.match.tick();
        QVERIFY(f.a.var(9) > 0);

        for (int i = 0; i < 5; ++i)
            f.match.tick();
        QCOMPARE(f.b.stateNo(), 5001);
        QCOMPARE(f.b.var(4), -4);
        const qreal x = f.b.pos().x();
        f.match.tick();
        QVERIFY(f.b.pos().x() > x);
        for (int i = 0; i < 20 && f.b.stateNo() != 0; ++i)
            f.match.tick();
        QCOMPARE(f.b.stateNo(), 0);
        QVERIFY(f.b.ctrl());
    }

    void guard()
    {
        Fight f(kHit);
        InputState back;
        back.right = true;
        f.b.setInput(back);
        const int t = f.untilHit();
        QVERIFY(t > 0);
        QCOMPARE(f.b.stateNo(), 150);
        QCOMPARE(f.b.life(), 990);
        QVERIFY(f.b.getHitVars().guarded);
        for (int i = 0; i < 6; ++i)
            f.match.tick();
        QCOMPARE(f.a.var(8), 1);
        QCOMPARE(f.a.power(), 17); // half the power on a guard
    }

    void hitFlags()
    {
        Fight air(QStringLiteral("attr = S, NA\ndamage = 50\nhitflag = A"));
        QCOMPARE(air.untilHit(10), -1);
        QCOMPARE(air.b.life(), 1000);
        Fight low(QStringLiteral("attr = S, NA\ndamage = 50\npausetime = 0, 5\nguardflag = L"));
        InputState back;
        back.right = true;
        low.b.setInput(back);
        QVERIFY(low.untilHit() > 0);
        QCOMPARE(low.b.stateNo(), 5000);
        QCOMPARE(low.b.life(), 950);
    }

    void noKill()
    {
        Fight f(QStringLiteral("attr = S, NA\ndamage = 5000\nkill = 0"));
        QVERIFY(f.untilHit() > 0);
        QCOMPARE(f.b.life(), 1);
    }

    void p2StateNo()
    {
        Fight f(QStringLiteral("attr = S, NA\ndamage = 10\np2stateno = 150"));
        QVERIFY(f.untilHit() > 0);
        QCOMPARE(f.b.stateNo(), 150);
        QCOMPARE(f.b.moveType(), QLatin1Char('H'));
    }

    void throws()
    {
        CharacterData thrower;
        thrower.setAnimations("[Begin Action 0]\nClsn2Default: 1\n Clsn2[0] = -10,-60, 10,0\n0,0, 0,0, -1\n\n"
                              "[Begin Action 200]\nClsn2Default: 1\n Clsn2[0] = -10,-60, 10,0\n200,0, 0,0, 2\n"
                              "Clsn1: 1\n Clsn1[0] = 0,-50, 40,-40\n200,1, 0,0, 3\n\n"
                              "[Begin Action 820]\n820,0, 0,0, -1\n");
        thrower.addStates("[Statedef 0]\ntype = S\nphysics = S\nanim = 0\nctrl = 1\n"
                          "[State 0]\ntype = Null\ntrigger1 = 1\n"
                          "[Statedef 200]\ntype = S\nmovetype = A\nphysics = S\nanim = 200\nctrl = 0\n"
                          "[State 200]\ntype = HitDef\ntrigger1 = time = 0\nattr = S, NT\nhitflag = M-\n"
                          "p1stateno = 810\np2stateno = 820\nid = 7\n"
                          "[Statedef 810]\ntype = S\nmovetype = A\nphysics = N\nanim = 0\nvelset = 0,0\n"
                          "[State 810, count]\ntype = VarSet\ntrigger1 = time = 1\nv = 1\nvalue = NumTarget(7)\n"
                          "[State 810, life]\ntype = VarSet\ntrigger1 = time = 1\nv = 2\nvalue = target, life\n"
                          "[State 810, bind]\ntype = TargetBind\ntrigger1 = time < 5\npos = 30, -10\n"
                          "[State 810, face]\ntype = TargetFacing\ntrigger1 = time = 4\nvalue = 1\n"
                          "[State 810, hurt]\ntype = TargetLifeAdd\ntrigger1 = time = 5\nvalue = -78\n"
                          "[State 810, throw]\ntype = TargetState\ntrigger1 = time = 5\nvalue = 821\n"
                          "[State 810, end]\ntype = ChangeState\ntrigger1 = time = 6\nvalue = 0\nctrl = 1\n"
                          "[Statedef 820]\ntype = S\nmovetype = H\nphysics = N\nvelset = 0,0\n"
                          "[State 820]\ntype = ChangeAnim2\ntrigger1 = time = 0\nvalue = 820\n"
                          "[State 820, b]\ntype = VarSet\ntrigger1 = 1\nv = 6\nvalue = time\n"
                          "[Statedef 821]\ntype = A\nmovetype = H\nphysics = N\n"
                          "[State 821]\ntype = VarSet\ntrigger1 = time = 0\nv = 7\nvalue = 1\n"
                          "[State 821, b]\ntype = SelfState\ntrigger1 = time = 2\nvalue = 5050\n");
        CharacterData victim;
        victim.setAnimations("[Begin Action 0]\nClsn2Default: 1\n Clsn2[0] = -10,-60, 10,0\n0,0, 0,0, -1\n\n"
                             "[Begin Action 5050]\n5050,0, 0,0, -1\n");
        victim.addStates("[Statedef 0]\ntype = S\nphysics = S\nanim = 0\nctrl = 1\n"
                         "[State 0]\ntype = Null\ntrigger1 = 1\n"
                         "[Statedef 5050]\ntype = A\nmovetype = H\nphysics = N\nanim = 5050\n"
                         "[State 5050]\ntype = VarSet\ntrigger1 = time = 0\nv = 8\nvalue = 1\n");
        Match match;
        Character a(thrower, &match);
        Character b(victim, &match);
        a.setPos({-20, 0});
        b.setPos({20, 0});
        b.setFacing(-1);
        a.start(200);
        b.start(0);
        int t = 0;
        while (b.stateNo() != 820 && ++t < 10)
            match.tick();
        QCOMPARE(b.stateNo(), 820);
        QCOMPARE(a.stateNo(), 810);
        QCOMPARE(a.targets(), QList<Character*>{&b});
        QVERIFY(!a.ctrl());

        match.tick();
        QCOMPARE(&b.stateOwner(), &thrower);
        QCOMPARE(b.animNo(), 820);
        QCOMPARE(b.pos().x(), a.pos().x() + 30);
        QCOMPARE(b.pos().y(), -10.0);
        match.tick();
        QCOMPARE(a.var(1), 1);
        QCOMPARE(a.var(2), 1000);
        for (int i = 0; i < 3; ++i)
            match.tick();
        QCOMPARE(b.facing(), a.facing());
        match.tick();
        QCOMPARE(b.life(), 922);
        QCOMPARE(b.stateNo(), 821);
        QCOMPARE(b.var(7), 1);
        for (int i = 0; i < 3 && b.stateNo() != 5050; ++i)
            match.tick();
        QCOMPARE(b.stateNo(), 5050);
        QCOMPARE(&b.stateOwner(), &victim);
        QCOMPARE(b.var(8), 1);
        QCOMPARE(a.stateNo(), 0);
    }

    void lieDown()
    {
        CharacterData data;
        data.setAnimations(kAir);
        data.addStates(states(QStringLiteral("attr = S, NA\ndamage = 10\np2stateno = 5110\np2getp1state = 0")) +
                       QStringLiteral("[Statedef 5110]\ntype = L\nmovetype = H\nphysics = N\n"
                                      "[State 5110]\ntype = Null\ntrigger1 = 1\n"
                                      "[Statedef 5120]\ntype = L\nmovetype = H\nphysics = N\n"
                                      "[State 5120]\ntype = Null\ntrigger1 = 1\n"));
        data.setConstant(QStringLiteral("data.liedown.time"), 10);
        Match match;
        Character a(data, &match);
        Character b(data, &match);
        a.setPos({-20, 0});
        b.setPos({20, 0});
        b.setFacing(-1);
        a.start(200);
        b.start(0);
        while (b.stateNo() != 5110 && match.gameTime() < 10)
            match.tick();
        QCOMPARE(b.stateNo(), 5110);
        QCOMPARE(b.getHitVars().downRecoverTime, 10);
        int lying = 0;
        while (b.stateNo() == 5110 && lying < 30) {
            match.tick();
            ++lying;
        }
        QCOMPARE(b.stateNo(), 5120);
        QVERIFY2(lying >= 10 && lying <= 12, qPrintable(QString::number(lying)));
    }

    void cornerPushDefaults()
    {
        // 1.3 times guard.velocity (itself ground.velocity's x), none for air attacks.
        Fight ground(QStringLiteral("attr = S, NA\nground.velocity = -4"));
        ground.match.tick();
        const HitDef& g = ground.a.hitDef();
        QCOMPARE(g.cornerPush, -4 * 1.3f);
        QCOMPARE(g.airCornerPush, g.cornerPush);
        QCOMPARE(g.downCornerPush, g.cornerPush);
        QCOMPARE(g.guardCornerPush, g.cornerPush);
        QCOMPARE(g.airGuardCornerPush, g.cornerPush);
        Fight guard(QStringLiteral("attr = C, NA\nground.velocity = -4\nguard.velocity = -10\n"
                                   "guard.cornerpush.veloff = -3"));
        guard.match.tick();
        QCOMPARE(guard.a.hitDef().cornerPush, -13.f);
        QCOMPARE(guard.a.hitDef().guardCornerPush, -3.f);
        QCOMPARE(guard.a.hitDef().airGuardCornerPush, -3.f);
        Fight air(QStringLiteral("attr = A, NA\nground.velocity = -4\nair.cornerpush.veloff = -2"));
        air.match.tick();
        QCOMPARE(air.a.hitDef().cornerPush, 0.f);
        QCOMPARE(air.a.hitDef().airCornerPush, -2.f);
        QCOMPARE(air.a.hitDef().downCornerPush, 0.f);
        QVERIFY(!air.a.hitDef().downBounce);
    }

    void cornerPush()
    {
        auto cornered = [](const QString& hitdef, float bX, bool guarding, QList<float>* moves) {
            Fight f(hitdef);
            f.match.setBounds(-160, 160);
            f.a.setPos({bX - 35, 0});
            f.b.setPos({bX, 0});
            if (guarding) {
                InputState back;
                back.right = true;
                f.b.setInput(back);
            }
            if (f.untilHit() < 0)
                return false;
            float x = float(f.a.pos().x());
            for (int i = 0; i < 12; ++i) {
                f.match.tick();
                moves->append(float(f.a.pos().x()) - x);
                x = float(f.a.pos().x());
            }
            return true;
        };
        QList<float> moves;
        QVERIFY(cornered(QStringLiteral("attr = S, NA\ndamage = 10\npausetime = 5, 8\nground.velocity = -4"), 145,
                         false, &moves));
        // The pause of 5 ticks (the hit's tick counts), then 1.3 * -4 backwards, 70% of it each tick
        // while it is a unit or more.
        QList<float> expected{0, 0, 0, 0};
        for (float v = -5.2f; std::abs(v) >= 1; v *= 0.7f)
            expected << v;
        while (expected.size() < 12)
            expected << 0;
        QCOMPARE(moves.size(), expected.size());
        for (int i = 0; i < moves.size(); ++i)
            QVERIFY2(std::abs(moves[i] - expected[i]) < 0.001f,
                     qPrintable(QStringLiteral("tick %1: %2 (expected %3)").arg(i).arg(moves[i]).arg(expected[i])));

        moves.clear();
        QVERIFY(cornered(QStringLiteral("attr = S, NA\ndamage = 10\nground.velocity = -4"), 100, false, &moves));
        QCOMPARE(std::accumulate(moves.cbegin(), moves.cend(), 0.f), 0.f);
        moves.clear();
        QVERIFY(cornered(QStringLiteral("attr = S, NA\ndamage = 10\nground.velocity = -4\n"
                                        "ground.cornerpush.veloff = 0"),
                         145, false, &moves));
        QCOMPARE(std::accumulate(moves.cbegin(), moves.cend(), 0.f), 0.f);

        moves.clear();
        QVERIFY(cornered(QStringLiteral("attr = S, NA\ndamage = 10\nguardflag = MA\nground.velocity = -4\n"
                                        "guard.cornerpush.veloff = -2"),
                         142, true, &moves));
        QVERIFY(std::abs(std::accumulate(moves.cbegin(), moves.cend(), 0.f) - (-2.f - 1.4f)) < 0.001f);
    }

    void localCoord()
    {
        const QString hitdef =
            QStringLiteral("attr = S, NA\ndamage = 50\npausetime = 0, 0\nground.velocity = -8\nground.hittime = 12");
        CharacterData big;
        big.setLocalCoordWidth(640);
        big.setAnimations(kAir);
        big.addStates(states(hitdef));
        CharacterData small;
        small.setAnimations(kAir);
        small.addStates(states(hitdef));
        auto fight = [&](float targetX) {
            Match match(3);
            Character a(big, &match);
            Character b(small, &match);
            a.setPos({-20, 0}); // game -10: its attack box reaches game 10
            b.setPos({targetX, 0});
            b.setFacing(-1);
            a.start(200);
            b.start(0);
            for (int t = 0; t < 40 && b.stateNo() != 5001; ++t)
                match.tick();
            return std::pair(b.stateNo(), b.var(4));
        };
        // At 16 its body (6 to 26) meets the attack. At 26 (16 to 36) the attack falls short,
        // though in the attacker's own units its box would reach 30.
        QCOMPARE(fight(16), std::pair(5001, -4));
        QCOMPARE(fight(26).first, 0);
    }

    void downBounce()
    {
        const QString downStates = QStringLiteral(
            "[Statedef 5080]\ntype = L\nmovetype = H\nphysics = N\nvelset = 0,0\n"
            "[State 5080, a]\ntype = VarSet\ntrigger1 = time = 0\nv = 20\nvalue = GetHitVar(yvel) * 10\n"
            "[State 5080, b]\ntype = ChangeState\ntrigger1 = HitShakeOver\ntrigger1 = GetHitVar(yvel) != 0\n"
            "value = 5030\n"
            "[Statedef 5030]\ntype = A\nmovetype = H\nphysics = N\n"
            "[State 5030, a]\ntype = HitVelSet\ntrigger1 = time = 0\n"
            "[State 5030, b]\ntype = VelAdd\ntrigger1 = time > 0\ny = .5\n"
            "[State 5030, c]\ntype = ChangeState\ntrigger1 = vel y > 0\ntrigger1 = pos y >= 0\nvalue = 5100\n"
            "[Statedef 5100]\ntype = L\nmovetype = H\nphysics = N\n"
            "[State 5100, a]\ntype = VarSet\ntrigger1 = time = 0\nv = 21\nvalue = GetHitVar(fall.yvel) * 10\n"
            "[State 5100, b]\ntype = PosSet\ntrigger1 = time = 0\ny = 0\n"
            "[State 5100, c]\ntype = VelSet\ntrigger1 = time = 0\ny = 0\n"
            "[State 5100, d]\ntype = ChangeState\ntrigger1 = time = 0\ntrigger1 = GetHitVar(fall.yvel) = 0\n"
            "value = 5110\n"
            "[State 5100, e]\ntype = ChangeState\ntrigger1 = time = 1\nvalue = 5101\n"
            "[Statedef 5101]\ntype = L\nmovetype = H\nphysics = N\n"
            "[State 5101, a]\ntype = HitFallVel\ntrigger1 = time = 0\n"
            "[State 5101, b]\ntype = VarSet\ntrigger1 = time = 0\nv = 22\nvalue = vel y * 10\n"
            "[State 5101, c]\ntype = VelAdd\ntrigger1 = 1\ny = .4\n"
            "[State 5101, d]\ntype = ChangeState\ntrigger1 = vel y > 0\ntrigger1 = pos y >= 0\nvalue = 5110\n"
            "[Statedef 5110]\ntype = L\nmovetype = H\nphysics = N\nanim = 0\n"
            "[State 5110]\ntype = VarAdd\ntrigger1 = time = 0\nv = 23\nvalue = 1\n");
        struct Result
        {
            int yvel = 0; // var(20): GetHitVar(yvel) * 10 when hit
            float fallYVel = 0;
            int landingFallY = 0; // var(21): GetHitVar(fall.yvel) * 10 on landing
            int bounced = 0;      // var(22): the bounce's y velocity * 10 (0: none)
            int lowered = 0;      // how far below the floor B waited to fly
            int downs = 0;
        };
        auto knockUp = [&downStates](const QString& velocity) {
            CharacterData data;
            data.setAnimations(kAir);
            data.addStates(states(QStringLiteral("attr = S, NA\ndamage = 10\nhitflag = MAFD\npausetime = 0, 4\n"
                                                 "fall = 1\nfall.yvelocity = -3\n") +
                                  velocity) +
                           downStates);
            Match match;
            Character a(data, &match);
            Character b(data, &match);
            a.setPos({-20, 0});
            b.setPos({20, 0});
            b.setFacing(-1);
            a.start(200);
            b.start(5110);
            Result r;
            for (int t = 0; t < 120 && b.var(23) < 2; ++t) {
                match.tick();
                if (b.stateNo() == 5080) {
                    r.lowered = std::max(r.lowered, int(b.pos().y()));
                    r.fallYVel = b.getHitVars().fallYVelocity;
                }
            }
            r.yvel = b.var(20);
            r.landingFallY = b.var(21);
            r.bounced = b.var(22);
            r.downs = b.var(23);
            return r;
        };
        const Result stays = knockUp(QStringLiteral("down.velocity = -2, -6"));
        QCOMPARE(stays.yvel, -60);
        QCOMPARE(stays.lowered, 15); // the engine lowers a character knocked up off the floor
        QCOMPARE(stays.fallYVel, 0.f);
        QCOMPARE(stays.landingFallY, 0);
        QCOMPARE(stays.bounced, 0);
        QCOMPARE(stays.downs, 2);
        const Result bounces = knockUp(QStringLiteral("down.velocity = -2, -6\ndown.bounce = 1"));
        QCOMPARE(bounces.yvel, -60);
        QCOMPARE(bounces.fallYVel, -3.f);
        QCOMPARE(bounces.landingFallY, -30);
        QCOMPARE(bounces.bounced, -30);
        QCOMPARE(bounces.downs, 2);

        const Result slides = knockUp(QStringLiteral("down.velocity = -3, 0"));
        QCOMPARE(slides.yvel, 0);
        QCOMPARE(slides.lowered, 0);
        QCOMPARE(slides.fallYVel, -3.f);
    }

    void priority()
    {
        auto duel = [](const QString& first, const QString& second) {
            CharacterData a;
            CharacterData b;
            a.setAnimations(kAir);
            b.setAnimations(kAir);
            a.addStates(states(QStringLiteral("attr = S, NA\ndamage = 50\n") + first));
            b.addStates(states(QStringLiteral("attr = S, NA\ndamage = 50\n") + second));
            Match match;
            Character p1(a, &match);
            Character p2(b, &match);
            p1.setPos({-20, 0});
            p2.setPos({20, 0});
            p2.setFacing(-1);
            p1.start(200);
            p2.start(200);
            for (int t = 0; t < 4; ++t)
                match.tick();
            return std::pair(p1.life(), p2.life());
        };
        QCOMPARE(duel(QString(), QString()), std::pair(950, 950));
        QCOMPARE(duel("priority = 5", QString()), std::pair(1000, 950));
        QCOMPARE(duel("priority = 4, Miss", QString()), std::pair(950, 1000));
        QCOMPARE(duel("priority = 4, Miss", "priority = 4, Miss"), std::pair(1000, 1000));
        QCOMPARE(duel("priority = 4, Dodge", QString()), std::pair(1000, 1000));
    }

    void reversal()
    {
        CharacterData reverser;
        reverser.setAnimations(kAir);
        reverser.addStates(
            states(QStringLiteral("attr = S, NA\ndamage = 50")) +
            QStringLiteral("[Statedef 300]\ntype = S\nmovetype = A\nphysics = S\nanim = 200\n"
                           "[State 300]\ntype = ReversalDef\ntrigger1 = time = 0\n"
                           "reversal.attr = SCA, AA\npausetime = 4, 6\np1stateno = 310\np2stateno = 320\n"
                           "[Statedef 310]\ntype = S\n[State 310]\ntype = Null\ntrigger1 = 1\n"
                           "[Statedef 320]\ntype = S\nmovetype = H\nmovehitpersist = 1\n"
                           "[State 320]\ntype = VarSet\ntrigger1 = 1\nv = 5\nvalue = MoveReversed\n"));
        CharacterData attacker;
        attacker.setAnimations(kAir);
        attacker.addStates(states(QStringLiteral("attr = S, NA\ndamage = 50")));
        Match match;
        Character a(reverser, &match);
        Character b(attacker, &match);
        a.setPos({-20, 0});
        b.setPos({20, 0});
        b.setFacing(-1);
        a.start(300);
        b.start(200);
        bool spark = false;
        for (int t = 0; t < 4; ++t) {
            match.tick();
            spark = spark || (!match.spawns().isEmpty() && match.spawns().at(0).anim == 2);
        }
        QVERIFY(spark);
        QCOMPARE(a.sprPriority(), 1);
        QCOMPARE(b.sprPriority(), 0);
        QCOMPARE(a.stateNo(), 310);
        QCOMPARE(b.stateNo(), 320);
        QCOMPARE(&b.stateOwner(), &reverser);
        QCOMPARE(a.life(), 1000);
        QCOMPARE(b.life(), 1000);
        QCOMPARE(a.targets(), QList<Character*>{&b});
        QVERIFY(b.var(5) > 0);
    }

    void hitOverride()
    {
        const QString defender = QStringLiteral(
            "[Statedef 0]\ntype = S\nphysics = S\nanim = 0\nctrl = 1\n"
            "[State 0]\ntype = HitOverride\ntrigger1 = time = 0\nattr = SCA, AA, AT\nstateno = 400\ntime = -1\n"
            "[Statedef 400]\ntype = S\nmovetype = H\n[State 400]\ntype = Null\ntrigger1 = 1\n");
        auto fight = [&defender](const QString& hitdef, int* aContact) {
            CharacterData a;
            CharacterData b;
            a.setAnimations(kAir);
            b.setAnimations(kAir);
            a.addStates(states(hitdef));
            b.addStates(defender);
            Match match;
            Character p1(a, &match);
            Character p2(b, &match);
            p1.setPos({-20, 0});
            p2.setPos({20, 0});
            p2.setFacing(-1);
            p1.start(200);
            p2.start(0);
            for (int t = 0; t < 5; ++t)
                match.tick();
            *aContact = p1.var(9);
            return std::pair(p2.stateNo(), p2.life());
        };
        int contact = 0;
        QCOMPARE(fight(QStringLiteral("attr = S, NA\ndamage = 50"), &contact), std::pair(400, 1000));
        QVERIFY(contact > 0);
        QCOMPARE(fight(QStringLiteral("attr = S, NT\ndamage = 50\np2stateno = 150"), &contact), std::pair(0, 1000));
    }

    void hitEffects()
    {
        Fight f(QStringLiteral("attr = S, NA\ndamage = 10\nhitsound = 5, 1\nguardsound = S6, 0\nsparkxy = -5, -30\n"
                               "envshake.time = 7\npalfx.time = 9\npalfx.add = 50, 0, 0\np1sprpriority = 3"));
        for (int t = 0; t < 3; ++t)
            f.match.tick();
        QCOMPARE(f.b.moveType(), QLatin1Char('H'));
        QCOMPARE(f.match.spawns().size(), 1);
        const SpawnEvent spark = f.match.spawns().at(0);
        QCOMPARE(spark.anim, 2);
        QVERIFY(spark.common);
        QCOMPARE(spark.pos.x, 20.f - 16.f + 5.f); // B's front, then sparkxy towards A
        QCOMPARE(spark.pos.y, -30.f);
        QCOMPARE(f.match.sounds().size(), 1);
        QVERIFY(f.match.sounds().at(0).common);
        QCOMPARE(f.match.sounds().at(0).group, 5);
        QCOMPARE(f.match.envShake().time, 7);
        QCOMPARE(f.b.draw().palFx.add[0], 50);
        QCOMPARE(f.a.sprPriority(), 3);
        QCOMPARE(f.b.sprPriority(), 0);

        Fight g(QStringLiteral("attr = S, NA\ndamage = 10\nhitsound = 5, 1\nguardsound = S6, 0\nguardflag = MA"));
        InputState back;
        back.right = true;
        g.b.setInput(back);
        for (int t = 0; t < 3; ++t)
            g.match.tick();
        QVERIFY(g.b.getHitVars().guarded);
        QCOMPARE(g.match.spawns().at(0).anim, 40);
        QVERIFY(!g.match.sounds().at(0).common);
        QCOMPARE(g.match.sounds().at(0).group, 6);
    }

    void hitVelocityAndAttributes()
    {
        Fight f(kHit);
        QVERIFY(f.untilHit() > 0);
        // HitVel X is positive moving back.
        QCOMPARE(eval(f.b, "HitVel X").toFloat(), 4.f);
        QCOMPARE(eval(f.b, "HitVel Y").toFloat(), 0.f);
        QCOMPARE(eval(f.b, "GetHitVar(xvel)").toFloat(), -4.f);
        QVERIFY(eval(f.b, "GetHitVar(xveladd)").isFloat());
        QVERIFY(!f.a.hitDef().active);
        QCOMPARE(eval(f.a, "HitDefAttr = SC, NA").toInt(), 1);
        QCOMPARE(eval(f.a, "HitDefAttr = SCA, AA").toInt(), 1);
        QCOMPARE(eval(f.a, "HitDefAttr = C, NA").toInt(), 0);
        QCOMPARE(eval(f.a, "HitDefAttr = S, SA, HA").toInt(), 0);
        QCOMPARE(eval(f.a, "HitDefAttr != S, SA").toInt(), 1);
        for (int i = 0; i < 30 && f.a.stateNo() != 0; ++i)
            f.match.tick();
        QCOMPARE(f.a.stateNo(), 0);
        QCOMPARE(eval(f.a, "HitDefAttr = SCA, AA").toInt(), 0);
    }

    void chains()
    {
        auto lifeAfter = [](const QString& second) {
            Fight f(thenHit(QString(kHit) + QStringLiteral("\nid = 3"),
                            QStringLiteral("attr = S, NA\ndamage = 7\n") + second));
            for (int t = 0; t < 14; ++t)
                f.match.tick();
            return f.b.life();
        };
        QCOMPARE(lifeAfter(QString()), 943);
        QCOMPARE(lifeAfter(QStringLiteral("chainid = 3")), 943);
        QCOMPARE(lifeAfter(QStringLiteral("chainid = 4")), 950);
        QCOMPARE(lifeAfter(QStringLiteral("nochainid = 2, 3")), 950);
        Fight fresh(QStringLiteral("attr = S, NA\ndamage = 50\nchainid = 3"));
        QCOMPARE(fresh.untilHit(10), -1);
    }

    void hitAdd()
    {
        Fight f(QString(kHit) +
                QStringLiteral("\n[State 200, Add]\ntype = HitAdd\n"
                               "trigger1 = MoveHit = 1 && HitPauseTime = 0\npersistent = 0\nvalue = 2"));
        for (int t = 0; t < 10; ++t)
            f.match.tick();
        QCOMPARE(f.b.getHitVars().hitCount, 3);
        QCOMPARE(eval(f.a, "HitCount").toInt(), 3);
    }

    void facingAndPlacement()
    {
        Fight turned(QStringLiteral("attr = S, NA\ndamage = 10\np1getp2facing = 1"));
        QVERIFY(turned.untilHit() > 0);
        QCOMPARE(turned.a.facing(), -1);
        auto placed = [](const QString& extra) {
            Fight f(QStringLiteral("attr = S, NA\ndamage = 10\n") + extra);
            f.untilHit();
            return QPointF(f.b.pos().x() - f.a.pos().x(), f.b.pos().y() - f.a.pos().y());
        };
        QCOMPARE(placed(QStringLiteral("snap = 30, -10")), QPointF(30, -10));
        QCOMPARE(placed(QStringLiteral("mindist = 50")), QPointF(50, 0));
        QCOMPARE(placed(QStringLiteral("maxdist = 30")), QPointF(30, 0));
        QCOMPARE(placed(QStringLiteral("mindist = 10\nmaxdist = 60")), QPointF(40, 0));
    }

    void forceStand()
    {
        auto stateAfter = [](const QString& hitdef) {
            Fight f(hitdef);
            f.data.addStates(QStringLiteral("[Statedef 11]\ntype = C\nphysics = C\nanim = 0\n"
                                            "[State 11]\ntype = Null\ntrigger1 = 1\n"));
            f.b.start(11);
            f.untilHit();
            return f.b.stateNo();
        };
        QCOMPARE(stateAfter(QStringLiteral("attr = S, NA\ndamage = 10\nground.velocity = -4, -5")), 5000);
        QCOMPARE(stateAfter(QStringLiteral("attr = S, NA\ndamage = 10\nground.velocity = -4, -5\nforcestand = 0")),
                 5010);
        QCOMPARE(stateAfter(QStringLiteral("attr = S, NA\ndamage = 10\nforcestand = 1")), 5000);
        QCOMPARE(stateAfter(QStringLiteral("attr = S, NA\ndamage = 10")), 5010);
    }

    void forceNoFall()
    {
        Fight f(thenHit(QString(kHit) + QStringLiteral("\nfall = 1"),
                        QStringLiteral("attr = S, NA\ndamage = 7\nforcenofall = 1")));
        QVERIFY(f.untilHit() > 0);
        QVERIFY(f.b.getHitVars().fall);
        for (int t = 0; t < 12 && f.b.life() > 943; ++t)
            f.match.tick();
        QCOMPARE(f.b.life(), 943);
        QVERIFY(!f.b.getHitVars().fall);
    }

    void severalTargets()
    {
        auto fight = [](const QString& hitdef) {
            CharacterData a;
            a.setAnimations(kAir);
            a.addStates(states(hitdef));
            CharacterData b;
            b.setAnimations(kAir);
            b.addStates("[Statedef 0]\ntype = S\nphysics = S\nanim = 0\n"
                        "[State 0]\ntype = Helper\ntrigger1 = time = 0\nid = 1\nstateno = 1000\n"
                        "[Statedef 1000]\ntype = S\nphysics = N\nanim = 0\n[State 1000]\ntype = Null\ntrigger1 = 1\n"
                        "[Statedef 5000]\ntype = S\nmovetype = H\nphysics = N\nanim = 5000\n"
                        "[State 5000]\ntype = Null\ntrigger1 = 1\n");
            Match match;
            Character p1(a, &match);
            Character p2(b, &match);
            p1.setPos({-20, 0});
            p2.setPos({20, 0});
            p2.setFacing(-1);
            p1.start(200);
            p2.start(0);
            for (int t = 0; t < 4; ++t)
                match.tick();
            const Character* helper = match.characters().last();
            return QList<int>{p2.life(), helper->life(), eval(p1, "HitCount").toInt(), eval(p1, "UniqHitCount").toInt(),
                              eval(p1, "NumTarget").toInt()};
        };
        QCOMPARE(fight(QStringLiteral("attr = S, NA\ndamage = 50")), QList<int>({950, 950, 1, 2, 2}));
        QCOMPARE(fight(QStringLiteral("attr = S, NA\ndamage = 50\nhitonce = 1")), QList<int>({950, 1000, 1, 1, 1}));
    }

    void affectTeam()
    {
        auto lives = [](const QString& team) {
            CharacterData a;
            a.setAnimations(kAir);
            a.addStates(
                QStringLiteral("[Statedef 0]\ntype = S\nphysics = S\nanim = 0\n"
                               "[State 0]\ntype = Helper\ntrigger1 = time = 0\nstateno = 1000\n"
                               "[Statedef 1000]\ntype = S\nmovetype = A\nphysics = N\nanim = 200\n"
                               "[State 1000]\ntype = HitDef\ntrigger1 = time = 0\nattr = S, NA\ndamage = 30\n%1\n"
                               "[Statedef 5000]\ntype = S\nmovetype = H\nphysics = N\nanim = 0\n"
                               "[State 5000]\ntype = Null\ntrigger1 = 1\n")
                    .arg(team));
            CharacterData idle;
            idle.setAnimations(kAir);
            idle.addStates("[Statedef 0]\ntype = S\nanim = 0\n[State 0]\ntype = Null\ntrigger1 = 1\n");
            Match match;
            Character p1(a, &match);
            Character p2(idle, &match);
            p1.setPos({0, 0});
            p2.setPos({150, 0});
            p2.setFacing(-1);
            p1.start(0);
            p2.start(0);
            for (int t = 0; t < 8; ++t)
                match.tick();
            return QList<int>{p1.life(), p2.life(), match.characters().last()->life()};
        };
        QCOMPARE(lives(QString()), QList<int>({1000, 1000, 1000}));
        QCOMPARE(lives(QStringLiteral("affectteam = F")), QList<int>({970, 1000, 1000}));
        QCOMPARE(lives(QStringLiteral("affectteam = B")), QList<int>({970, 1000, 1000}));
    }

    void juggle()
    {
        auto fight = [](bool noCheck) {
            CharacterData a;
            a.setAnimations(kAir);
            a.addStates(
                QStringLiteral("[Statedef 200]\ntype = S\nmovetype = A\nphysics = S\nanim = 200\njuggle = 10\n"
                               "[State 200, hit]\ntype = HitDef\ntrigger1 = time = 0\nattr = S, NA\n"
                               "damage = 10\nfall = 1\n"
                               "[State 200, again]\ntype = ChangeState\ntrigger1 = AnimTime = 0\nvalue = 200\n") +
                (noCheck ? QStringLiteral("[Statedef -2]\n[State -2]\ntype = AssertSpecial\ntrigger1 = 1\n"
                                          "flag = nojugglecheck\n")
                         : QString()));
            CharacterData b;
            b.setAnimations(kAir);
            b.addStates("[Statedef 0]\ntype = S\nphysics = S\nanim = 0\n[State 0]\ntype = Null\ntrigger1 = 1\n"
                        "[Statedef 5000]\ntype = A\nmovetype = H\nphysics = N\nanim = 5000\nvelset = 0, 0\n"
                        "[State 5000]\ntype = Null\ntrigger1 = 1\n"
                        "[Statedef 5020]\ntype = A\nmovetype = H\nphysics = N\nanim = 5000\nvelset = 0, 0\n"
                        "[State 5020]\ntype = Null\ntrigger1 = 1\n");
            Match match;
            Character p1(a, &match);
            Character p2(b, &match);
            p1.setPos({-20, 0});
            p2.setPos({20, 0});
            p2.setFacing(-1);
            p1.start(200);
            p2.start(0);
            for (int t = 0; t < 30; ++t)
                match.tick();
            return std::pair(p2.getHitVars().hitCount, p2.jugglePoints());
        };
        QCOMPARE(fight(false), std::pair(1, 5));
        const auto [hits, points] = fight(true);
        QVERIFY(hits > 1);
        QVERIFY(points < 5);
    }
};

QTEST_MAIN(RuntimeHitTest)
#include "RuntimeHitTest.moc"
