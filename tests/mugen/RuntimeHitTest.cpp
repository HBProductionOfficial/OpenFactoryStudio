#include "engines/mugen/runtime/Character.h"
#include "engines/mugen/runtime/CharacterData.h"

#include <QTest>

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
        QCOMPARE(f.a.power(), 50);
        QCOMPARE(f.b.power(), 50);
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
        QCOMPARE(f.a.power(), 25); // half the power on a guard
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
        for (int t = 0; t < 4; ++t)
            match.tick();
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
        QCOMPARE(f.match.envShake().time, 6);
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
};

QTEST_MAIN(RuntimeHitTest)
#include "RuntimeHitTest.moc"
