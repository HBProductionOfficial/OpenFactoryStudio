#include "engines/mugen/runtime/Character.h"
#include "engines/mugen/runtime/CharacterData.h"

#include <QTest>

using namespace OFS::Mugen::Runtime;

namespace {

const char* kAir = "[Begin Action 0]\nClsn2Default: 1\n Clsn2[0] = -10,-60, 10,0\n0,0, 0,0, -1\n\n"
                   "[Begin Action 1000]\nClsn2Default: 1\n Clsn2[0] = -10,-50, 10,-30\n"
                   "Clsn1Default: 1\n Clsn1[0] = -10,-50, 10,-30\n1000,0, 0,0, 2\n1000,1, 0,0, 2\n\n"
                   "[Begin Action 1001]\n1001,0, 0,0, 3\n";

QString shooter(const QString& projectile)
{
    return QStringLiteral("[Statedef 0]\ntype = S\nphysics = S\nanim = 0\nctrl = 1\n"
                          "[State 0]\ntype = Null\ntrigger1 = 1\n"
                          "[Statedef 300]\ntype = S\nmovetype = A\nphysics = S\nanim = 0\n"
                          "[State 300, fire]\ntype = Projectile\ntrigger1 = time = 0\n%1\n"
                          "[State 300, count]\ntype = VarSet\ntrigger1 = 1\nv = 1\nvalue = NumProjID(5)\n"
                          "[State 300, hit]\ntype = VarSet\ntrigger1 = 1\nv = 2\nvalue = ProjHit5 = 1\n"
                          "[State 300, since]\ntype = VarSet\ntrigger1 = 1\nv = 3\nvalue = ProjContactTime(5)\n"
                          "[State 300, cancel]\ntype = VarSet\ntrigger1 = 1\nv = 4\nvalue = ProjCancelTime(0)\n"
                          "[Statedef 5000]\ntype = S\nmovetype = H\nphysics = S\n"
                          "[State 5000]\ntype = Null\ntrigger1 = 1\n")
        .arg(projectile);
}

} // namespace

class RuntimeProjectileTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void hit()
    {
        CharacterData data;
        data.setAnimations(kAir);
        data.addStates(shooter("projid = 5\nprojanim = 1000\nprojhitanim = 1001\nvelocity = 10, 0\n"
                               "offset = 20, 0\nattr = S, SP\ndamage = 40\npausetime = 2, 5"));
        Match match;
        Character a(data, &match);
        Character b(data, &match);
        a.setPos({-50, 0});
        b.setPos({150, 0});
        b.setFacing(-1);
        a.start(300);
        b.start(0);
        match.tick();
        QCOMPARE(a.projectiles().size(), 1);
        QCOMPARE(a.projectiles().at(0).pos.x, -20.f); // made at -30, moved once
        QCOMPARE(a.var(1), 1);
        int t = 1;
        while (b.life() == 1000 && t < 30) {
            match.tick();
            ++t;
        }
        QCOMPARE(t, 16);
        QCOMPARE(b.life(), 960);
        QCOMPARE(b.stateNo(), 5000);
        QCOMPARE(a.hitPauseTime(), 0);
        match.tick();
        QCOMPARE(a.var(2), 1);
        QCOMPARE(a.var(3), 0);
        match.tick();
        QCOMPARE(a.var(2), 0);
        QCOMPARE(a.var(3), 1);
        QCOMPARE(a.var(4), -1);
        QCOMPARE(a.projectiles().at(0).status, Projectile::Status::Hit);
        QCOMPARE(a.projectiles().at(0).shown.number, 1001);
        for (int i = 0; i < 10 && !a.projectiles().isEmpty(); ++i)
            match.tick();
        QVERIFY(a.projectiles().isEmpty());
        match.tick();
        QCOMPARE(a.var(1), 0);
    }

    void removeTime()
    {
        CharacterData data;
        data.setAnimations(kAir);
        data.addStates(shooter("projid = 5\nprojanim = 1000\nprojremovetime = 3"));
        Match match;
        Character a(data, &match);
        a.start(300);
        for (int i = 0; i < 3; ++i)
            match.tick();
        QCOMPARE(a.projectiles().size(), 1);
        match.tick();
        QVERIFY(a.projectiles().isEmpty());
    }

    void clash()
    {
        CharacterData data;
        data.setAnimations(kAir);
        data.addStates(shooter("projid = 5\nprojanim = 1000\nvelocity = 5, 0\nprojcancelanim = 1001"));
        Match match;
        Character a(data, &match);
        Character b(data, &match);
        a.setPos({-100, 0});
        b.setPos({100, 0});
        b.setFacing(-1);
        a.start(300);
        b.start(300);
        match.tick();
        for (int i = 0;
             i < 30 && !a.projectiles().isEmpty() && a.projectiles().at(0).status == Projectile::Status::Active; ++i)
            match.tick();
        QCOMPARE(a.projectiles().at(0).status, Projectile::Status::Cancelled);
        QCOMPARE(b.projectiles().at(0).status, Projectile::Status::Cancelled);
        match.tick();
        QCOMPARE(a.var(4), 0);
        QCOMPARE(b.life(), 1000);
    }

    void helperFires()
    {
        CharacterData data;
        data.setAnimations(kAir);
        data.addStates("[Statedef 0]\n[State 0]\ntype = Helper\ntrigger1 = time = 0\nstateno = 400\n"
                       "[Statedef 400]\n[State 400]\ntype = Projectile\ntrigger1 = time = 0\nprojanim = 1000\n");
        Match match;
        Character a(data, &match);
        a.start(0);
        match.tick();
        QCOMPARE(a.numProjectiles(), 1);
    }
};

QTEST_GUILESS_MAIN(RuntimeProjectileTest)
#include "RuntimeProjectileTest.moc"
