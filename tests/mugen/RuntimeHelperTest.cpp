#include "engines/mugen/runtime/Character.h"
#include "engines/mugen/runtime/CharacterData.h"

#include <QTest>

using namespace OFS::Mugen::Runtime;

namespace {

const char* kStates = "[Statedef 0]\ntype = S\nphysics = S\n"
                      "[State 0, make]\ntype = Helper\ntrigger1 = time = 0\nid = 5\nstateno = 1000\npos = 20, -10\n"
                      "[State 0, count]\ntype = VarSet\ntrigger1 = 1\nv = 0\nvalue = NumHelper(5)\n"
                      "[State 0, read]\ntype = VarSet\ntrigger1 = NumHelper(5)\nv = 4\nvalue = helper(5), var(1)\n"
                      "[Statedef 1000]\ntype = A\nphysics = N\n"
                      "[State 1000, me]\ntype = VarSet\ntrigger1 = time = 0\nv = 1\nvalue = IsHelper(5) + IsHelper\n"
                      "[State 1000, tell]\ntype = ParentVarSet\ntrigger1 = time = 0\nv = 2\nvalue = 7\n"
                      "[State 1000, power]\ntype = PowerAdd\ntrigger1 = time = 0\nvalue = 100\n"
                      "[State 1000, dist]\ntype = VarSet\ntrigger1 = 1\nv = 3\nvalue = ParentDist X\n"
                      "[State 1000, p2]\ntype = VarSet\ntrigger1 = 1\nv = 5\nvalue = P2Dist X\n"
                      "[State 1000, root]\ntype = VarSet\ntrigger1 = 1\nv = 6\nvalue = root, var(2)\n"
                      "[State 1000, fx]\ntype = Explod\ntrigger1 = time = 0\nanim = 0\nremovetime = -1\n"
                      "[State 1000, bye]\ntype = DestroySelf\ntrigger1 = time = 3\n";

} // namespace

class RuntimeHelperTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void lifecycle()
    {
        CharacterData data;
        data.setAnimations("[Begin Action 0]\n0,0, 0,0, -1\n");
        data.addStates(kStates);
        CharacterData idle;
        idle.addStates("[Statedef 0]\n[State 0]\ntype = Null\ntrigger1 = 1\n");
        Match match;
        Character p1(data, &match);
        Character p2(idle, &match);
        p1.setPos({-50, 0});
        p2.setPos({100, 0});
        p2.setFacing(-1);
        p1.start(0);
        p2.start(0);

        match.tick();
        QCOMPARE(match.characters().size(), 3);
        Character* h = match.characters().last();
        QVERIFY(h->isHelper());
        QCOMPARE(h->helperId(), 5);
        QCOMPARE(h->parent(), &p1);
        QCOMPARE(h->root(), &p1);
        QCOMPARE(h->team(), p1.team());
        QVERIFY(p2.team() != p1.team());
        QCOMPARE(h->stateNo(), 1000);
        QCOMPARE(h->pos().x(), -30.0);
        QCOMPARE(h->pos().y(), -10.0);
        QCOMPARE(h->var(1), 2);
        QCOMPARE(p1.var(2), 7);
        QCOMPARE(p1.power(), 100);
        QCOMPARE(h->power(), 100);
        QCOMPARE(p1.var(0), 1);
        QCOMPARE(h->var(3), -20);
        QCOMPARE(h->var(5), 130);
        QCOMPARE(h->var(6), 7);
        QCOMPARE(match.enemy(&p2), &p1);
        QCOMPARE(match.numEnemies(&p2), 1);
        match.tick();
        QCOMPARE(p1.var(4), 2);

        match.tick();
        match.tick();
        QCOMPARE(match.characters().size(), 2);
        QCOMPARE(p1.explods().size(), 1);
        QCOMPARE(p1.numExplods(), 0);
        match.tick();
        QCOMPARE(p1.var(0), 0);
        QVERIFY(p1.messages().isEmpty());
    }

    void binding()
    {
        CharacterData data;
        data.addStates("[Statedef 0]\n"
                       "[State 0, make]\ntype = Helper\ntrigger1 = time = 0\nstateno = 1000\nhelpertype = player\n"
                       "[State 0, walk]\ntype = PosAdd\ntrigger1 = 1\nx = 2\n"
                       "[Statedef 1000]\n"
                       "[State 1000]\ntype = BindToRoot\ntrigger1 = 1\npos = 30, -5\nfacing = -1\n");
        Match match;
        Character p1(data, &match);
        p1.start(0);
        for (int i = 0; i < 5; ++i)
            match.tick();
        Character* h = match.characters().last();
        QVERIFY(h->isHelper());
        QVERIFY(h->isPlayerType());
        QCOMPARE(h->pos().x(), p1.pos().x() + 30);
        QCOMPARE(h->pos().y(), -5.0);
        QCOMPARE(h->facing(), -p1.facing());
    }

    void limit()
    {
        CharacterData data;
        data.addStates("[Statedef 0]\n[State 0]\ntype = Helper\ntrigger1 = 1\nstateno = 1000\n"
                       "[Statedef 1000]\n[State 1000]\ntype = Null\ntrigger1 = 1\n");
        Match match;
        Character p1(data, &match);
        p1.start(0);
        for (int i = 0; i < 60; ++i)
            match.tick();
        QCOMPARE(match.characters().size(), 57);
        QVERIFY(!p1.messages().isEmpty());
    }
};

QTEST_GUILESS_MAIN(RuntimeHelperTest)
#include "RuntimeHelperTest.moc"
