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
        QCOMPARE(f.b.var(1), int(AnimType::Medium));
        QCOMPARE(f.b.var(2), 8);
        QVERIFY(!f.b.ctrl());
        QCOMPARE(f.a.power(), 50);
        QCOMPARE(f.b.power(), 50);
        QCOMPARE(f.a.hitPauseTime(), 5);
        QVERIFY(!f.a.hitDef().active);

        const int timeBefore = f.a.time();
        for (int i = 0; i < 5; ++i)
            f.match.tick();
        QCOMPARE(f.a.time(), timeBefore);
        QCOMPARE(f.a.var(9), 0);
        f.match.tick();
        QVERIFY(f.a.var(9) > 0);

        for (int i = 0; i < 4; ++i)
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
};

QTEST_MAIN(RuntimeHitTest)
#include "RuntimeHitTest.moc"
