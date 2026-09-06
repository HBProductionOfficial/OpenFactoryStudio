#include "engines/mugen/runtime/Character.h"
#include "engines/mugen/runtime/CharacterData.h"

#include <QTest>

using namespace OFS::Mugen::Runtime;

namespace {

const char* kBasics = "[Statedef 0]\ntype = S\nphysics = S\nctrl = 1\n[State 0]\ntype = Null\ntrigger1 = 1\n"
                      "[Statedef 10]\ntype = C\nphysics = C\nctrl = 0\n"
                      "[State 10]\ntype = ChangeState\ntrigger1 = time = 2\nvalue = 11\nctrl = 1\n"
                      "[Statedef 11]\ntype = C\nphysics = C\nctrl = 1\n[State 11]\ntype = Null\ntrigger1 = 1\n"
                      "[Statedef 12]\ntype = S\nphysics = S\nctrl = 0\n"
                      "[State 12]\ntype = ChangeState\ntrigger1 = time = 2\nvalue = 0\nctrl = 1\n"
                      "[Statedef 20]\ntype = S\nphysics = S\nctrl = 1\n[State 20]\ntype = Null\ntrigger1 = 1\n"
                      "[Statedef 40]\ntype = S\nphysics = S\nctrl = 0\n[State 40]\ntype = Null\ntrigger1 = 1\n";

const char* kAir = "[Begin Action 0]\nClsn2Default: 1\n Clsn2[0] = -10,-60, 10,0\n0,0, 0,0, -1\n\n"
                   "[Begin Action 200]\nClsn2Default: 1\n Clsn2[0] = -10,-60, 10,0\n200,0, 0,0, 2\n"
                   "Clsn1: 1\n Clsn1[0] = 0,-50, 40,-40\n200,1, 0,0, 3\n200,2, 0,0, 4\n";

struct Duel
{
    Duel(const QString& hitdef, const QString& defender = QString(), const QString& attacker = QString())
        : a(attackerData, &match)
        , b(defenderData, &match)
    {
        attackerData.setAnimations(kAir);
        attackerData.addStates(QStringLiteral("[Statedef 0]\ntype = S\nphysics = S\nanim = 0\nctrl = 1\n"
                                              "[State 0]\ntype = Null\ntrigger1 = 1\n"
                                              "[Statedef 200]\ntype = S\nmovetype = A\nphysics = S\nanim = 200\n"
                                              "[State 200]\ntype = HitDef\ntrigger1 = time = 0\n%1\n%2")
                                   .arg(hitdef, attacker));
        defenderData.setAnimations(kAir);
        defenderData.addStates(QStringLiteral("[Statedef 0]\ntype = S\nphysics = S\nanim = 0\nctrl = 1\n"
                                              "[State 0]\ntype = Null\ntrigger1 = 1\n%1"
                                              "[Statedef 5000]\ntype = S\nmovetype = H\nphysics = S\n"
                                              "[State 5000]\ntype = Null\ntrigger1 = 1\n")
                                   .arg(defender));
        a.setPos({-20, 0});
        b.setPos({20, 0});
        b.setFacing(-1);
        a.start(200);
        b.start(0);
        for (int t = 0; t < 8; ++t)
            match.tick();
    }
    CharacterData attackerData;
    CharacterData defenderData;
    Match match;
    Character a;
    Character b;
};

} // namespace

class RuntimeEffectsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void engineMoves()
    {
        CharacterData data;
        data.addStates(kBasics);
        Match match;
        Character p(data, &match);
        p.start(0);
        auto hold = [&](bool up, bool down, bool right, bool left) {
            InputState in;
            in.up = up;
            in.down = down;
            in.right = right;
            in.left = left;
            p.setInput(in);
            match.tick();
        };
        hold(false, true, false, false);
        QCOMPARE(p.stateNo(), 10);
        for (int i = 0; i < 3; ++i)
            hold(false, true, false, false);
        QCOMPARE(p.stateNo(), 11);
        hold(false, false, false, false);
        QCOMPARE(p.stateNo(), 12);
        for (int i = 0; i < 3; ++i)
            hold(false, false, false, false);
        QCOMPARE(p.stateNo(), 0);
        hold(false, false, true, false);
        QCOMPARE(p.stateNo(), 20);
        hold(false, false, true, true);
        QCOMPARE(p.stateNo(), 0);
        hold(false, false, false, true);
        QCOMPARE(p.stateNo(), 20);
        hold(false, false, false, false);
        QCOMPARE(p.stateNo(), 0);
        hold(true, false, false, false);
        QCOMPARE(p.stateNo(), 40);
        QVERIFY(!p.ctrl());

        // Flags count from the next tick, so NoWalk is asserted the tick before.
        CharacterData lazy;
        lazy.addStates(
            QStringLiteral("[Statedef -2]\n[State -2]\ntype = AssertSpecial\ntrigger1 = 1\nflag = nowalk\n") +
            QLatin1String(kBasics));
        Match other;
        Character q(lazy, &other);
        q.start(0);
        other.tick();
        InputState forward;
        forward.right = true;
        q.setInput(forward);
        for (int i = 0; i < 3; ++i)
            other.tick();
        QCOMPARE(q.stateNo(), 0);
        QVERIFY(q.isAsserted(QStringLiteral("nowalk")));
    }

    void noAutoTurn()
    {
        CharacterData data;
        data.addStates(QStringLiteral("[Statedef -2]\n[State -2]\ntype = AssertSpecial\ntrigger1 = 1\n"
                                      "flag = noautoturn\n") +
                       QLatin1String(kBasics));
        Match match;
        Character a(data, &match);
        Character b(data, &match);
        a.setPos({-50, 0});
        b.setPos({50, 0});
        b.setFacing(-1);
        a.start(0);
        b.start(0);
        match.tick();
        a.setPos({100, 0});
        for (int i = 0; i < 3; ++i)
            match.tick();
        QCOMPARE(a.facing(), 1);
    }

    void hitBy()
    {
        Duel shielded(QStringLiteral("attr = S, NA\ndamage = 50"),
                      QStringLiteral("[State 0, b]\ntype = NotHitBy\ntrigger1 = 1\nvalue = S\n"));
        QCOMPARE(shielded.b.life(), 1000);
        Duel special(QStringLiteral("attr = S, NA\ndamage = 50"),
                     QStringLiteral("[State 0, b]\ntype = HitBy\ntrigger1 = 1\nvalue = SCA, SA\n"));
        QCOMPARE(special.b.life(), 1000);
        Duel all(QStringLiteral("attr = S, NA\ndamage = 50"),
                 QStringLiteral("[State 0, b]\ntype = HitBy\ntrigger1 = 1\nvalue = SCA, AA\n"));
        QCOMPARE(all.b.life(), 950);
        Duel expired(QStringLiteral("attr = S, NA\ndamage = 50"),
                     QStringLiteral("[State 0, b]\ntype = NotHitBy\ntrigger1 = time = 0\nvalue = SCA\ntime = 1\n"));
        QCOMPARE(expired.b.life(), 950);
    }

    void multipliers()
    {
        Duel doubled(QStringLiteral("attr = S, NA\ndamage = 50"), QString(),
                     QStringLiteral("[State 200, mul]\ntype = AttackMulSet\ntrigger1 = time = 0\nvalue = 2\n"));
        QCOMPARE(doubled.b.life(), 900);
        Duel tough(QStringLiteral("attr = S, NA\ndamage = 50"),
                   QStringLiteral("[State 0, b]\ntype = DefenceMulSet\ntrigger1 = 1\nvalue = 2\n"));
        QCOMPARE(tough.b.life(), 975);
    }

    void unguardable()
    {
        CharacterData data;
        data.addStates(QStringLiteral("[Statedef -2]\n[State -2]\ntype = AssertSpecial\ntrigger1 = 1\n"
                                      "flag = unguardable\nflag2 = NoShadow\n"));
        Match match;
        Character c(data, &match);
        c.start(0);
        match.tick();
        QVERIFY(c.isAsserted(QStringLiteral("unguardable")));
        QVERIFY(c.isAsserted(QStringLiteral("noshadow")));
        QVERIFY(match.isAsserted(QStringLiteral("noshadow")));
        QVERIFY(!match.isAsserted(QStringLiteral("intro")));
    }

    void clipboard()
    {
        CharacterData data;
        data.addStates("[Statedef 0]\n"
                       "[State 0, a]\ntype = VarSet\ntrigger1 = 1\nv = 3\nvalue = 42\n"
                       "[State 0, b]\ntype = DisplayToClipboard\ntrigger1 = 1\n"
                       "text = \"var(3) = %d, half %f, 100%%\"\nparams = var(3), var(3) / 2.0\n"
                       "[State 0, c]\ntype = AppendToClipboard\ntrigger1 = time = 1\ntext = \"tick %d\"\n"
                       "params = time\n"
                       "[State 0, d]\ntype = ClearClipboard\ntrigger1 = time = 3\n");
        Character c(data);
        c.start(0);
        c.tick();
        QCOMPARE(c.clipboard(), QStringLiteral("var(3) = 42, half 21.000000, 100%"));
        c.tick();
        QCOMPARE(c.clipboard(), QStringLiteral("var(3) = 42, half 21.000000, 100%\ntick 1"));
        c.tick();
        c.tick();
        QCOMPARE(c.clipboard(), QString());
    }

    void sounds()
    {
        CharacterData data;
        data.addStates("[Statedef 0]\n"
                       "[State 0, a]\ntype = PlaySnd\ntrigger1 = time = 0\nvalue = F7, 1\nchannel = 2\n"
                       "[State 0, b]\ntype = PlaySnd\ntrigger1 = time = 0\nvalue = 3, 4\nvolume = 50\nloop = 1\n"
                       "[State 0, c]\ntype = StopSnd\ntrigger1 = time = 1\nchannel = 2\n"
                       "[State 0, d]\ntype = SndPan\ntrigger1 = time = 2\nchannel = 0\nabspan = -40\n");
        Match match;
        Character c(data, &match);
        c.start(0);
        match.tick();
        QCOMPARE(match.sounds().size(), 2);
        const SoundEvent common = match.sounds().at(0);
        QVERIFY(common.common);
        QCOMPARE(common.group, 7);
        QCOMPARE(common.index, 1);
        QCOMPARE(common.channel, 2);
        QCOMPARE(common.playerId, c.id());
        const SoundEvent own = match.sounds().at(1);
        QVERIFY(!own.common);
        QCOMPARE(own.group, 3);
        QCOMPARE(own.volume, 50);
        QVERIFY(own.loop);
        match.tick();
        QCOMPARE(match.sounds().size(), 1);
        QCOMPARE(match.sounds().at(0).kind, SoundEvent::Kind::Stop);
        match.tick();
        QCOMPARE(match.sounds().at(0).kind, SoundEvent::Kind::Pan);
        QVERIFY(match.sounds().at(0).absolutePan);
        QCOMPARE(match.sounds().at(0).pan, -40.f);
    }

    void drawing()
    {
        CharacterData data;
        data.addStates("[Statedef 0]\n"
                       "[State 0, a]\ntype = Trans\ntrigger1 = time < 2\ntrans = addalpha\nalpha = 128, 128\n"
                       "[State 0, b]\ntype = Offset\ntrigger1 = time < 2\nx = 5\ny = -3\n"
                       "[State 0, c]\ntype = AngleSet\ntrigger1 = time = 0\nvalue = 30\n"
                       "[State 0, d]\ntype = AngleAdd\ntrigger1 = time = 1\nvalue = 15\n"
                       "[State 0, e]\ntype = AngleDraw\ntrigger1 = time < 2\nscale = 2, 1\n"
                       "[State 0, f]\ntype = PalFX\ntrigger1 = time = 0\ntime = 3\nadd = 10, 20, 30\n"
                       "sinadd = 5, 5, 5, 20\n"
                       "[State 0, g]\ntype = Trans\ntrigger1 = time = 3\ntrans = add1\n");
        Match match;
        Character c(data, &match);
        c.start(0);
        match.tick();
        QCOMPARE(c.draw().trans, QStringLiteral("addalpha"));
        QCOMPARE(c.draw().alpha[0], 128);
        QCOMPARE(c.draw().offset.x, 5.f);
        QVERIFY(c.draw().angleDraw);
        QCOMPARE(c.draw().angle, 30.f);
        QCOMPARE(c.draw().angleScale.x, 2.f);
        QCOMPARE(c.draw().palFx.add[1], 20);
        QCOMPARE(c.draw().palFx.sinPeriod, 20);
        match.tick();
        QCOMPARE(c.draw().angle, 45.f);
        match.tick();
        QCOMPARE(c.draw().trans, QStringLiteral("default"));
        QCOMPARE(c.draw().offset.x, 0.f);
        QVERIFY(!c.draw().angleDraw);
        QCOMPARE(c.draw().angle, 45.f);
        QVERIFY(!c.draw().palFx.active());
        match.tick();
        QCOMPARE(c.draw().trans, QStringLiteral("add1"));
        QCOMPARE(c.draw().alpha[1], 128);
    }

    void screenBound()
    {
        CharacterData data;
        data.addStates("[Statedef 300]\n"
                       "[State 300, a]\ntype = ScreenBound\ntrigger1 = 1\nvalue = 0\n"
                       "[State 300, b]\ntype = VelSet\ntrigger1 = 1\nx = 10\n");
        CharacterData still;
        still.addStates("[Statedef 0]\n[State 0]\ntype = Null\ntrigger1 = 1\n");
        Match match;
        Character runner(data, &match);
        Character other(still, &match);
        runner.setPos({0, 0});
        other.setPos({0, 0});
        runner.start(300);
        other.start(0);
        for (int i = 0; i < 40; ++i)
            match.tick();
        QVERIFY(runner.pos().x() > 300);
        QCOMPARE(match.cameraX(), float(other.pos().x()));
    }

    void envEffects()
    {
        CharacterData data;
        data.addStates("[Statedef 0]\n"
                       "[State 0, a]\ntype = EnvShake\ntrigger1 = time = 0\ntime = 5\nampl = 6\nfreq = 100\n"
                       "[State 0, b]\ntype = EnvColor\ntrigger1 = time = 0\nvalue = 255, 0, 0\ntime = 2\n"
                       "[State 0, c]\ntype = MakeDust\ntrigger1 = time = 0\npos = 10, 0\npos2 = -10, 0\n"
                       "[State 0, d]\ntype = VictoryQuote\ntrigger1 = 1\nvalue = 3\n");
        Match match;
        Character c(data, &match);
        c.setPos({100, 0});
        c.start(0);
        match.tick();
        QCOMPARE(match.envShake().time, 4);
        QCOMPARE(match.envShake().phase, 90.f);
        QCOMPARE(match.envColor().color[1], 0);
        QCOMPARE(match.spawns().size(), 2);
        QCOMPARE(match.spawns().at(0).pos.x, 110.f);
        QCOMPARE(match.spawns().at(1).pos.x, 90.f);
        QCOMPARE(c.victoryQuote(), 3);
        QVERIFY(c.messages().isEmpty());
    }
};

QTEST_GUILESS_MAIN(RuntimeEffectsTest)
#include "RuntimeEffectsTest.moc"
