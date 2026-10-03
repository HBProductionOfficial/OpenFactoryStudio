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
        QVERIFY(c.draw().palFx.active());
        match.tick();
        QVERIFY(!c.draw().palFx.active());
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
        QCOMPARE(match.envShake().time, 5);
        QCOMPARE(match.envShake().phase, 90.f);
        QCOMPARE(match.envColor().color[1], 0);
        QCOMPARE(match.spawns().size(), 2);
        QCOMPARE(match.spawns().at(0).pos.x, 110.f);
        QCOMPARE(match.spawns().at(1).pos.x, 90.f);
        QCOMPARE(c.victoryQuote(), 3);
        QVERIFY(c.messages().isEmpty());
    }

    void superPause()
    {
        CharacterData pauser;
        pauser.addStates("[Statedef 300]\ntype = S\nmovetype = A\n"
                         "[State 300]\ntype = SuperPause\ntrigger1 = time = 0\ntime = 10\npoweradd = 500\n"
                         "p2defmul = 2\nsound = S5, 1\n");
        CharacterData counter;
        counter.addStates("[Statedef 5000]\ntype = S\nmovetype = H\n"
                          "[State 5000]\ntype = VarAdd\ntrigger1 = 1\nv = 0\nvalue = 1\n");
        Match match;
        Character a(pauser, &match);
        Character b(counter, &match);
        a.setPos({-50, 0});
        b.setPos({50, 0});
        a.start(300);
        b.start(5000);
        match.tick();
        QCOMPARE(b.var(0), 1);
        QCOMPARE(a.power(), 500);
        QCOMPARE(match.superPauseTime(), 0);
        QCOMPARE(match.spawns().size(), 1);
        QCOMPARE(match.spawns().at(0).anim, 30);
        QVERIFY(match.spawns().at(0).common);
        QCOMPARE(match.sounds().size(), 1);
        QVERIFY(!match.sounds().at(0).common);
        QCOMPARE(b.superDefenceMul(), 2.f);
        match.tick();
        QCOMPARE(match.superPauseTime(), 10);
        QVERIFY(a.frozen());
        QVERIFY(b.frozen());
        for (int i = 0; i < 9; ++i)
            match.tick();
        QCOMPARE(b.var(0), 1);
        QCOMPARE(a.time(), 1);
        match.tick();
        QVERIFY(!b.frozen());
        QCOMPARE(b.var(0), 2);
    }

    void pauseMoveTime()
    {
        CharacterData pauser;
        pauser.addStates("[Statedef 300]\n"
                         "[State 300]\ntype = Pause\ntrigger1 = time = 0\ntime = 10\nmovetime = 3\n");
        CharacterData idle;
        idle.addStates("[Statedef 0]\n[State 0]\ntype = Null\ntrigger1 = 1\n");
        Match match;
        Character a(pauser, &match);
        Character b(idle, &match);
        a.start(300);
        b.start(0);
        match.tick();
        for (int i = 0; i < 3; ++i) {
            match.tick();
            QVERIFY(!a.frozen());
            QVERIFY(b.frozen());
        }
        QCOMPARE(a.time(), 4);
        match.tick();
        QVERIFY(a.frozen());
        QCOMPARE(a.time(), 4);
        QCOMPARE(match.pauseTime(), 7);
    }

    void explods()
    {
        CharacterData data;
        data.setAnimations("[Begin Action 0]\n0,0, 0,0, -1\n\n"
                           "[Begin Action 500]\n500,0, 0,0, 2\n500,1, 0,0, 2\n\n"
                           "[Begin Action 501]\n501,0, 0,0, -1\n");
        data.addStates("[Statedef 0]\nanim = 0\n"
                       "[State 0, a]\ntype = Explod\ntrigger1 = time = 0\nanim = 500\nid = 7\npos = 30, -20\n"
                       "vel = 2, 0\n"
                       "[State 0, b]\ntype = Explod\ntrigger1 = time = 0\nanim = 501\nid = 8\npos = 10, 0\n"
                       "bindtime = -1\nremovetime = -1\n"
                       "[State 0, c]\ntype = Explod\ntrigger1 = time = 0\nanim = 501\nid = 9\npostype = left\n"
                       "pos = 5, 6\nremovetime = 3\n"
                       "[State 0, n]\ntype = VarSet\ntrigger1 = 1\nv = 0\nvalue = NumExplod\n"
                       "[State 0, n7]\ntype = VarSet\ntrigger1 = 1\nv = 1\nvalue = NumExplod(8)\n"
                       "[State 0, m]\ntype = ModifyExplod\ntrigger1 = time = 1\nid = 8\nscale = 2, 2\n"
                       "[State 0, r]\ntype = RemoveExplod\ntrigger1 = time = 6\nid = 8\n"
                       "[State 0, v]\ntype = VelSet\ntrigger1 = 1\nx = 1\n");
        Match match;
        Character c(data, &match);
        c.setPos({0, 0});
        c.start(0);
        match.tick();
        QCOMPARE(c.numExplods(), 3);
        QCOMPARE(c.var(0), 3);
        QCOMPARE(c.var(1), 1);
        const Explod& moving = c.explods().at(0);
        QCOMPARE(moving.id, 7);
        QCOMPARE(moving.drawPos().x, 30.f);
        QCOMPARE(moving.drawPos().y, -20.f);
        const Explod& screen = c.explods().at(2);
        QVERIFY(screen.screen);
        QCOMPARE(screen.drawPos().x, 5.f);
        match.tick();
        QCOMPARE(c.explods().at(0).drawPos().x, 32.f);
        QCOMPARE(c.explods().at(1).scale.x, 2.f);
        QCOMPARE(c.explods().at(1).drawPos().x, float(c.pos().x()) + 10);
        match.tick();
        QCOMPARE(c.numExplods(9), 1);
        match.tick();
        QCOMPARE(c.numExplods(9), 0);
        QCOMPARE(c.numExplods(7), 1);
        match.tick();
        QCOMPARE(c.numExplods(7), 0);
        QCOMPARE(c.numExplods(8), 1);
        for (int i = 0; i < 2; ++i)
            match.tick();
        QCOMPARE(c.numExplods(), 0);
        QVERIFY(c.messages().isEmpty());

        CharacterData v11;
        v11.setMugenVersion("1.1");
        QVERIFY(v11.isMugen11());
        v11.setAnimations("[Begin Action 501]\n501,0, 0,0, -1\n");
        v11.addStates("[Statedef 0]\n"
                      "[State 0, a]\ntype = Explod\ntrigger1 = time = 0\nanim = 501\nid = 1\npos = 30, -20\n"
                      "removetime = -1\n"
                      "[State 0, b]\ntype = Explod\ntrigger1 = time = 0\nanim = 501\nid = 2\npos = 5, 6\n"
                      "space = screen\nremovetime = -1\n"
                      "[State 0, c]\ntype = Explod\ntrigger1 = time = 0\nanim = 501\nid = 3\npos = 10, 0\n"
                      "bindid = -1\nbindtime = -1\nremovetime = -1\n"
                      "[State 0, v]\ntype = VelSet\ntrigger1 = 1\nx = 1\n");
        for (ExplodRules rules : {ExplodRules::PlayerVersion, ExplodRules::Mugen10}) {
            Match versions;
            versions.setExplodRules(rules);
            Character d(v11, &versions);
            d.setPos({50, 0});
            d.setFacing(-1);
            d.start(0);
            versions.tick();
            QCOMPARE(d.numExplods(), 3);
            if (rules == ExplodRules::PlayerVersion) {
                QVERIFY(d.explodRules11());
                QCOMPARE(d.explods().at(0).drawPos().x, 30.f);
                QVERIFY(!d.explods().at(0).screen);
                QVERIFY(d.explods().at(1).screen);
                QCOMPARE(d.explods().at(1).drawPos().x, 5.f);
                versions.tick();
                QCOMPARE(d.explods().at(2).drawPos().x, float(d.pos().x()) + 10);
            } else {
                QVERIFY(!d.explodRules11());
                QCOMPARE(d.explods().at(0).drawPos().x, 20.f);
                QVERIFY(!d.explods().at(1).screen);
            }
        }
        for (const char* version : {"1.0", "04,14,2001", "2002.04.14", ""}) {
            CharacterData old;
            old.setMugenVersion(version);
            QVERIFY(!old.isMugen11());
        }

        CharacterData fightFx;
        fightFx.setAnimations("[Begin Action 30]\n30,0, 0,0, 3\n");
        CharacterData user;
        user.addStates("[Statedef 0]\n[State 0]\ntype = Explod\ntrigger1 = time = 0\nanim = F30\n"
                       "postype = p2\npos = 0, -50\n");
        Match fx;
        fx.setFightFx(&fightFx);
        Character a(user, &fx);
        Character b(user, &fx);
        a.setPos({-40, 0});
        b.setPos({40, 0});
        b.setFacing(-1);
        a.start(0);
        b.start(0);
        fx.tick();
        QCOMPARE(a.numExplods(), 1);
        QVERIFY(a.explods().at(0).common);
        QCOMPARE(a.explods().at(0).drawPos().x, 40.f);
        QCOMPARE(a.explods().at(0).drawFacing(), -1);
    }

    void width()
    {
        CharacterData walker;
        walker.addStates("[Statedef 300]\n"
                         "[State 300, w]\ntype = Width\ntrigger1 = 1\nedge = 20, 30\nplayer = 25\n"
                         "[State 300, v]\ntype = VelSet\ntrigger1 = 1\nx = 5\n");
        CharacterData still;
        still.addStates("[Statedef 0]\n[State 0]\ntype = Null\ntrigger1 = 1\n");
        Match match;
        Character c(walker, &match);
        Character other(still, &match);
        other.setPos({-100, 0});
        c.start(300);
        other.start(0);
        for (int i = 0; i < 200; ++i)
            match.tick();
        QCOMPARE(c.frontEdgeWidth(), 20.f);
        QCOMPARE(c.backEdgeWidth(), 30.f);
        QCOMPARE(c.frontWidth(), 25.f);
        QCOMPARE(c.backWidth(), 0.f);
        // The camera between both: the right side is 160 from it, less the screen's 15 and the 20.
        QCOMPARE(float(c.pos().x()), match.cameraX() + 160 - 15 - 20);
    }

    void lifeAdd()
    {
        CharacterData data;
        data.setConstant(QStringLiteral("data.defence"), 200);
        data.addStates("[Statedef 0]\n"
                       "[State 0, a]\ntype = LifeAdd\ntrigger1 = time = 0\nvalue = -100\n"
                       "[State 0, b]\ntype = LifeAdd\ntrigger1 = time = 1\nvalue = -100\nabsolute = 1\n");
        Character c(data);
        c.start(0);
        c.tick();
        QCOMPARE(c.life(), 950);
        c.tick();
        QCOMPARE(c.life(), 850);
    }

    void afterImages()
    {
        CharacterData data;
        data.addStates("[Statedef 0]\n"
                       "[State 0, a]\ntype = AfterImageTime\ntrigger1 = time = 0\ntime = 20\n"
                       "[State 0, b]\ntype = AfterImage\ntrigger1 = time = 1\ntime = 5\n"
                       "[State 0, c]\ntype = AfterImageTime\ntrigger1 = time = 2\nvalue = 20\n");
        Character c(data);
        c.start(0);
        c.tick();
        QCOMPARE(c.draw().afterImage.time, 0);
        c.tick();
        QCOMPARE(c.draw().afterImage.palContrast, (std::array<int, 3>{120, 120, 220}));
        c.tick();
        QCOMPARE(c.draw().afterImage.time, 19); // 20, counted down once this tick
    }

    void moreEffects()
    {
        CharacterData data;
        data.setAnimations("[Begin Action 501]\n501,0, 0,0, -1\n");
        data.addStates("[Statedef 0]\n"
                       "[State 0, dust]\ntype = MakeDust\ntrigger1 = 1\npos = 0, 0\nspacing = 3\n"
                       "[State 0, anim]\ntype = GameMakeAnim\ntrigger1 = time = 0\nvalue = 7\nrandom = 40\n"
                       "[State 0, snd]\ntype = PlaySnd\ntrigger1 = time = 0\nvalue = 1, 0\nvolumescale = 50\n"
                       "[State 0, ex]\ntype = Explod\ntrigger1 = time = 0\nanim = 501\nid = 4\nangle = 30\n"
                       "velocity = 2, 0\nremovetime = -1\n"
                       "[State 0, ex2]\ntype = Explod\ntrigger1 = time = 0\nanim = 501\nid = 5\nremovetime = -1\n"
                       "bindtime = 0\n"
                       "[State 0, bind]\ntype = ExplodBindTime\ntrigger1 = time = 1\nid = 5\n"
                       "[State 0, move]\ntype = VelSet\ntrigger1 = 1\nx = 3\n"
                       "[State 0, zoom]\ntype = Zoom\ntrigger1 = time = 0\nscale = 2\n"
                       "[State 0, remap]\ntype = RemapPal\ntrigger1 = time = 0\nsource = 1, 1\ndest = 1, 2\n"
                       "[State 0, all]\ntype = RemapPal\ntrigger1 = time = 1\nsource = -1, 0\ndest = 1, 3\n"
                       "[State 0, text]\ntype = DisplayToClipboard\ntrigger1 = time = 0\n"
                       "text = \"%e %G %i \\\"q\\\" a\\\\b\"\nparams = 21.0, 0.5, 7\n");
        Match match;
        Character c(data, &match);
        c.setPos({100, 0});
        c.start(0);
        match.tick();
        QCOMPARE(match.spawns().size(), 2);
        const SpawnEvent anim = match.spawns().at(1);
        QCOMPARE(anim.anim, 7);
        QVERIFY(std::abs(anim.pos.x - 100) <= 20 && std::abs(anim.pos.y) <= 20);
        QCOMPARE(match.sounds().at(0).volumeScale, 50.f);
        QCOMPARE(c.explods().at(0).angle, 30.f);
        QCOMPARE(c.explods().at(0).vel.x, 2.f);
        QCOMPARE(c.draw().remap.value({1, 1}), qMakePair(1, 2));
        QCOMPARE(c.clipboard(), QStringLiteral("2.100000e+01 0.5 7 \"q\" a\\b"));
        QVERIFY(c.messages().isEmpty());
        match.tick();
        QCOMPARE(match.spawns().size(), 0);
        QCOMPARE(c.explods().at(1).drawPos().x, float(c.pos().x()));
        QVERIFY(c.explods().at(1).drawPos().x > 103);
        QCOMPARE(c.draw().remap.size(), 1);
        QCOMPARE(c.draw().remap.value(DrawState::AllPalettes), qMakePair(1, 3));
        match.tick();
        QCOMPARE(match.spawns().size(), 0);
        match.tick();
        QCOMPARE(match.spawns().size(), 1);
    }

    void pauseCommandBuffer()
    {
        auto pressed = [](int bufferTime) {
            CharacterData pauser;
            pauser.addStates(QStringLiteral("[Statedef 300]\n"
                                            "[State 300]\ntype = Pause\ntrigger1 = time = 0\ntime = 5\n"
                                            "endcmdbuftime = %1\n")
                                 .arg(bufferTime));
            CharacterData frozen;
            frozen.setCommands("[Defaults]\ncommand.buffer.time = 3\n[Command]\nname = \"x\"\ncommand = x\ntime = 1\n");
            frozen.addStates("[Statedef 0]\n[State 0]\ntype = VarAdd\ntrigger1 = command = \"x\"\nv = 0\nvalue = 1\n");
            Match match;
            Character a(pauser, &match);
            Character b(frozen, &match);
            a.start(300);
            b.start(0);
            match.tick();
            for (int i = 0; i < 4; ++i)
                match.tick();
            const bool frozenBefore = b.frozen();
            InputState x;
            x.x = true;
            b.setInput(x);
            match.tick();
            const bool frozenLast = b.frozen();
            b.setInput(InputState());
            match.tick();
            return frozenBefore && frozenLast && !b.frozen() ? b.var(0) : -1;
        };
        QCOMPARE(pressed(0), 0);
        QVERIFY(pressed(2) > 0);
    }
};

QTEST_GUILESS_MAIN(RuntimeEffectsTest)
#include "RuntimeEffectsTest.moc"
