#include "engines/mugen/runtime/Character.h"
#include "engines/mugen/runtime/CharacterData.h"
#include "engines/mugen/runtime/Commands.h"

#include <QTest>

using namespace OFS::Mugen::Runtime;

namespace {

// Screen directions for a character facing right.
InputState in(const char* keys)
{
    InputState s;
    for (const char* p = keys; *p; ++p) {
        switch (*p) {
        case 'U':
            s.up = true;
            break;
        case 'D':
            s.down = true;
            break;
        case 'L':
            s.left = true;
            break;
        case 'R':
            s.right = true;
            break;
        case 'a':
            s.a = true;
            break;
        case 'b':
            s.b = true;
            break;
        case 'c':
            s.c = true;
            break;
        case 'x':
            s.x = true;
            break;
        case 'y':
            s.y = true;
            break;
        case 'z':
            s.z = true;
            break;
        case 's':
            s.s = true;
            break;
        default:
            break;
        }
    }
    return s;
}

// Returns the 1-based ticks the command was active on.
QList<int> run(const QString& cmd, const QList<const char*>& inputs, int facing = 1)
{
    CommandList list;
    list.load(cmd);
    QList<int> active;
    for (int t = 0; t < inputs.size(); ++t) {
        list.update(in(inputs[t]), facing);
        if (list.active("cmd"))
            active << t + 1;
    }
    return active;
}

} // namespace

class RuntimeCommandTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void quarterCircle()
    {
        const QString qcf = "[Command]\nname = \"cmd\"\ncommand = ~D, DF, F, x\ntime = 15\n";
        QCOMPARE(run(qcf, {"D", "DR", "R", "Rx", ""}), QList<int>{4});
        QCOMPARE(run(qcf, {"D", "DL", "L", "Lx"}, -1), QList<int>{4});
        QCOMPARE(run(qcf, {"D", "DL", "L", "Lx"}, 1), QList<int>());
        // Too slow for the 15-tick window.
        QList<const char*> slow = {"D", "DR"};
        for (int i = 0; i < 20; ++i)
            slow << "R";
        slow << "Rx";
        QVERIFY(run(qcf, slow).isEmpty());
        QVERIFY(run(qcf, {"R", "DR", "D", "Dx"}).isEmpty());
    }

    void buttons()
    {
        const QString x = "[Command]\nname = \"cmd\"\ncommand = x\ntime = 1\n";
        QCOMPARE(run(x, {"", "x", "x", "", "x"}), (QList<int>{2, 5}));
        const QString both = "[Command]\nname = \"cmd\"\ncommand = a+b\ntime = 1\n";
        QCOMPARE(run(both, {"ab"}), QList<int>{1});
        QVERIFY(run(both, {"a", "ab"}).isEmpty());
        const QString held = "[Command]\nname = \"cmd\"\ncommand = /F\ntime = 1\n";
        QCOMPARE(run(held, {"R", "R", ""}), (QList<int>{1, 2}));
        const QString release = "[Command]\nname = \"cmd\"\ncommand = ~a\ntime = 1\n";
        QCOMPARE(run(release, {"a", "a", "", ""}), QList<int>{3});
    }

    void chargeAndBuffer()
    {
        const QString charge = "[Command]\nname = \"cmd\"\ncommand = ~30a\ntime = 1\n";
        QList<const char*> long_;
        for (int i = 0; i < 30; ++i)
            long_ << "a";
        long_ << "";
        QCOMPARE(run(charge, long_), QList<int>{31});
        QVERIFY(run(charge, {"a", "a", "a", ""}).isEmpty());

        const QString buffered = "[Defaults]\ncommand.buffer.time = 3\n[Command]\nname = \"cmd\"\ncommand = x\n";
        QCOMPARE(run(buffered, {"x", "", "", "", ""}), (QList<int>{1, 2, 3}));
        const QString own = "[Command]\nname = \"cmd\"\ncommand = x\nbuffer.time = 2\n";
        QCOMPARE(run(own, {"x", "", ""}), (QList<int>{1, 2}));
    }

    void greaterAndRemap()
    {
        // ">" fails when another key is pressed or released in between (releasing a too).
        const QString strict = "[Command]\nname = \"cmd\"\ncommand = a, >b\n";
        QCOMPARE(run(strict, {"a", "a", "ab"}), QList<int>{3});
        QVERIFY(run(strict, {"a", "", "b"}).isEmpty());
        QVERIFY(run(strict, {"a", "ac", "ab"}).isEmpty());
        const QString loose = "[Command]\nname = \"cmd\"\ncommand = a, b\n";
        QCOMPARE(run(loose, {"a", "c", "b"}), QList<int>{3});

        const QString remapped = "[Remap]\nx = y\n[Command]\nname = \"cmd\"\ncommand = x\n";
        QCOMPARE(run(remapped, {"y"}), QList<int>{1});
        QVERIFY(run(remapped, {"x"}).isEmpty());

        QStringList errors;
        CommandList list;
        list.load("[Command]\nname = \"bad\"\ncommand = D, q\n", &errors);
        QCOMPARE(errors.size(), qsizetype(1));
        QVERIFY(errors.first().contains("invalid symbol 'q'"));
    }

    void commandTrigger()
    {
        CharacterData data;
        data.setCommands("[Command]\nname = \"QCF_x\"\ncommand = ~D, DF, F, x\n\n[Statedef -1]\n");
        data.addStates("[Statedef -1]\n"
                       "[State -1, Fireball]\ntype = ChangeState\ntriggerall = command = \"QCF_x\"\n"
                       "trigger1 = ctrl\nvalue = 1000\n"
                       "[Statedef 0]\nctrl = 1\n[Statedef 1000]\nctrl = 0\n");
        Character c(data);
        c.start(0);
        for (const char* keys : {"", "D", "DR", "R", "Rx"}) {
            c.setInput(in(keys));
            c.tick();
            if (c.stateNo() == 1000)
                break;
        }
        QCOMPARE(c.stateNo(), 1000);
        QVERIFY(!c.ctrl());
    }
};

QTEST_GUILESS_MAIN(RuntimeCommandTest)
#include "RuntimeCommandTest.moc"
