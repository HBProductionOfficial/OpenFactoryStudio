#include "engines/mugen/emulator/Training.h"
#include "engines/mugen/runtime/Character.h"
#include "engines/mugen/runtime/CharacterData.h"

#include <QTest>

using namespace OFS::Mugen;
using namespace OFS::Mugen::Runtime;

namespace {

// 0 stands with control. 200 attacks (movetype A) with a HitDef guarded low only. 5000 is hit.
// var(0) holds AILevel.
const char* kStates = "[Statedef 0]\ntype = S\nmovetype = I\nphysics = S\nctrl = 1\n"
                      "[State 0]\ntype = VarSet\ntrigger1 = 1\nv = 0\nvalue = AILevel\n"
                      "[Statedef 200]\ntype = S\nmovetype = A\nphysics = S\nctrl = 0\n"
                      "[State 200]\ntype = HitDef\ntrigger1 = 1\nattr = S, NA\nguardflag = L\n"
                      "[Statedef 5000]\ntype = S\nmovetype = H\nphysics = S\nctrl = 0\n";

struct Fixture
{
    CharacterData data;
    Match match;
    std::unique_ptr<Character> player;
    std::unique_ptr<Character> dummy;

    Fixture()
    {
        data.addStates(QString::fromLatin1(kStates));
        player = std::make_unique<Character>(data, &match);
        dummy = std::make_unique<Character>(data, &match);
        player->setPos({-20, 0});
        dummy->setPos({20, 0});
        player->setFacing(1);
        dummy->setFacing(-1);
        match.tick();
    }
};

TrainingOptions options(TrainingOptions::Mode mode = TrainingOptions::Mode::Stand)
{
    TrainingOptions o;
    o.mode = mode;
    return o;
}

} // namespace

class TrainingTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void stances()
    {
        Fixture f;
        Training training;
        const InputState none;
        training.setOptions(options(TrainingOptions::Mode::Stand));
        InputState in = training.dummyInput(f.match, *f.dummy, none);
        QVERIFY(!in.up && !in.down && !in.left && !in.right);
        training.setOptions(options(TrainingOptions::Mode::Crouch));
        QVERIFY(training.dummyInput(f.match, *f.dummy, none).down);
        training.setOptions(options(TrainingOptions::Mode::Jump));
        QVERIFY(training.dummyInput(f.match, *f.dummy, none).up);
        training.setOptions(options(TrainingOptions::Mode::WJump));
        QVERIFY(training.dummyInput(f.match, *f.dummy, none).up);

        InputState keys;
        keys.right = true;
        keys.b = true;
        training.setOptions(options(TrainingOptions::Mode::Crouch));
        in = training.dummyInput(f.match, *f.dummy, keys);
        QVERIFY(in.right && !in.down && in.b);
        TrainingOptions manual = options(TrainingOptions::Mode::Crouch);
        manual.control = TrainingOptions::Control::Manual;
        training.setOptions(manual);
        in = training.dummyInput(f.match, *f.dummy, none);
        QVERIFY(!in.down);
    }

    void buttonJam()
    {
        Fixture f;
        Training training;
        TrainingOptions o;
        o.buttonJam = 3; // C
        training.setOptions(o);
        QList<int> pressed;
        for (int t = 0; t < 30; ++t) {
            if (training.dummyInput(f.match, *f.dummy, {}).c)
                pressed << t;
        }
        QCOMPARE(pressed, QList<int>({0, 12, 24}));
    }

    void distance()
    {
        Fixture f;
        Training training;
        TrainingOptions o;
        f.player->setPos({-150, 0});
        f.dummy->setPos({150, 0});
        o.distance = TrainingOptions::Distance::Close;
        training.setOptions(o);
        InputState in = training.dummyInput(f.match, *f.dummy, {});
        QVERIFY(in.left && !in.right);
        o.distance = TrainingOptions::Distance::Far;
        training.setOptions(o);
        in = training.dummyInput(f.match, *f.dummy, {});
        QVERIFY(!in.left && !in.right);
        f.player->setPos({-20, 0});
        f.dummy->setPos({20, 0});
        o.distance = TrainingOptions::Distance::Medium;
        training.setOptions(o);
        in = training.dummyInput(f.match, *f.dummy, {});
        QVERIFY(in.right && !in.left);
        f.dummy->setPos({155, 0});
        f.player->setPos({120, 0});
        in = training.dummyInput(f.match, *f.dummy, {});
        QVERIFY(!in.right);
    }

    void autoGuard()
    {
        Fixture f;
        Training training;
        TrainingOptions o;
        o.guard = TrainingOptions::Guard::Auto;
        training.setOptions(o);
        InputState in = training.dummyInput(f.match, *f.dummy, {});
        QVERIFY(!in.right);
        f.player->changeState(200);
        f.match.tick();
        QVERIFY(f.match.attackerNear(f.dummy.get()) == f.player.get());
        in = training.dummyInput(f.match, *f.dummy, {});
        QVERIFY(in.right && !in.left);
        QVERIFY(in.down);
        o.guard = TrainingOptions::Guard::None;
        training.setOptions(o);
        QVERIFY(!training.dummyInput(f.match, *f.dummy, {}).right);
    }

    void computer()
    {
        Fixture f;
        Training training;
        TrainingOptions o;
        o.control = TrainingOptions::Control::AI;
        o.aiLevel = 8;
        training.setOptions(o);
        training.reset(7);
        int moves = 0;
        int buttons = 0;
        for (int t = 0; t < 300; ++t) {
            const InputState in = training.dummyInput(f.match, *f.dummy, {});
            moves += in.left || in.right || in.up || in.down;
            buttons += in.a || in.b || in.c || in.x || in.y || in.z;
        }
        QVERIFY(moves > 50);
        QVERIFY(buttons > 20);
        QCOMPARE(f.dummy->aiLevel(), 0);
        f.dummy->setAiLevel(8);
        f.match.tick();
        QCOMPARE(f.dummy->var(0), 8);
        QCOMPARE(f.player->var(0), 0);
    }

    void refills()
    {
        Fixture f;
        Training training;
        f.dummy->setLife(300);
        f.dummy->setPower(0);
        f.dummy->changeState(5000);
        for (int t = 0; t < 100; ++t) {
            f.match.tick();
            training.refill(f.match, *f.dummy, 1);
        }
        QCOMPARE(f.dummy->life(), 300);
        QCOMPARE(f.dummy->power(), 0);
        f.dummy->changeState(0);
        for (int t = 0; t < 59; ++t) {
            f.match.tick();
            training.refill(f.match, *f.dummy, 1);
        }
        QCOMPARE(f.dummy->life(), 300);
        f.match.tick();
        training.refill(f.match, *f.dummy, 1);
        QCOMPARE(f.dummy->life(), 1000);
        QCOMPARE(f.dummy->power(), 3000);

        f.match.setNoKo(true);
        f.player->setLife(0);
        f.match.tick();
        QVERIFY(f.player->alive());
        QCOMPARE(f.player->stateNo(), 0);
    }
};

QTEST_GUILESS_MAIN(TrainingTest)
#include "TrainingTest.moc"
