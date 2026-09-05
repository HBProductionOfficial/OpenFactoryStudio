#include "engines/mugen/runtime/CharacterData.h"
#include "engines/mugen/runtime/IniText.h"

#include <QTest>

using namespace OFS::Mugen::Runtime;

class RuntimeCharacterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void iniText()
    {
        const IniText ini = IniText::parse("; header\n"
                                           "[Info]\n"
                                           "name = \"Kung; Fu\" ; the name\n"
                                           "[State 200, a]\n"
                                           "trigger1 = time = 0\n"
                                           "trigger1 = ctrl\n"
                                           "flag\n"
                                           "[State 200, b]\n"
                                           "value = 1\n");
        QCOMPARE(ini.sections().size(), qsizetype(3));
        QCOMPARE(ini.section("info")->value("NAME"), QStringLiteral("\"Kung; Fu\""));
        QCOMPARE(unquote(ini.section("info")->value("name")), QStringLiteral("Kung; Fu"));
        const IniSection& a = ini.sections()[1];
        QCOMPARE(a.name, QStringLiteral("State 200, a"));
        QCOMPARE(a.entries.size(), qsizetype(3));
        QCOMPARE(a.entries[1].value, QStringLiteral("ctrl"));
        QCOMPARE(a.entries[1].line, 5);
        QVERIFY(a.has("flag"));
        QCOMPARE(ini.sections()[2].name, QStringLiteral("State 200, b"));
    }

    void constants()
    {
        CharacterData data;
        QCOMPARE(data.constant("data.life"), 1000.f);
        data.addConstants("[Data]\nlife = 900\nattack = 110\n"
                          "[Size]\nhead.pos = -5, -90\nheight = 60\n"
                          "[Velocity]\nwalk.fwd = 2.4\njump.neu = 0,-8.4\nrun.back = -4.5,-3.8\n"
                          "[Movement]\nyaccel = .44\nairjump.num = 1\n"
                          "[Quotes]\nvictory1 = \"x\"\n");
        QCOMPARE(data.constant("data.life"), 900.f);
        QCOMPARE(data.constant("Data.Attack"), 110.f);
        QCOMPARE(data.constant("size.head.pos.y"), -90.f);
        QCOMPARE(data.constant("velocity.walk.fwd.x"), 2.4f);
        QCOMPARE(data.constant("velocity.jump.neu.y"), -8.4f);
        QCOMPARE(data.constant("velocity.jump.y"), -8.4f);
        QCOMPARE(data.constant("velocity.run.back.y"), -3.8f);
        QCOMPARE(data.constant("movement.yaccel"), 0.44f);
        QCOMPARE(data.constant("movement.airjump.num"), 1.f);
        QCOMPARE(data.constant("movement.stand.friction"), 0.85f);
        QVERIFY(!data.hasConstant("quotes.victory1"));
        QCOMPARE(data.constant("nothing", -1), -1.f);
    }

    void states()
    {
        CharacterData data;
        data.addStates("[Statedef 200]\n"
                       "type = S\nmovetype = A\nphysics = S\nanim = 200\nvelset = 0,0\nctrl = 0\n"
                       "\n"
                       "[State 200, Hit]\n"
                       "type = HitDef\n"
                       "triggerall = time >= 0\n"
                       "trigger1 = AnimElem = 3\n"
                       "trigger2 = time = 5\n"
                       "trigger2 = ctrl = 0\n"
                       "trigger4 = 1 ; not used: there is no trigger3\n"
                       "attr = S, NA\n"
                       "damage = 23, 0\n"
                       "persistent = 0\n"
                       "ignorehitpause = 1\n"
                       "\n"
                       "[State 200, End]\n"
                       "type = ChangeState\n"
                       "trigger1 = AnimTime = 0\n"
                       "value = 0\n"
                       "ctrl = 1\n"
                       "\n"
                       "[Statedef 201]\n"
                       "[State 201, Broken]\n"
                       "type = Null\n"
                       "trigger1 = (1 +\n"
                       "[State 201, NoTrigger]\n"
                       "type = Null\n"
                       "triggerall = 1\n",
                       "kfm.cns");
        QCOMPARE(data.states().size(), qsizetype(2));
        const StateDef* s200 = data.state(200);
        QVERIFY(s200);
        QCOMPARE(s200->param("type")->text, QStringLiteral("S"));
        QVERIFY(s200->param("anim")->isExpression());
        QCOMPARE(s200->param("velset")->values.size(), std::size_t(2));
        QCOMPARE(s200->controllers.size(), std::size_t(2));

        const Controller& hit = s200->controllers[0];
        QCOMPARE(hit.label, QStringLiteral("Hit"));
        QCOMPARE(hit.type, QStringLiteral("hitdef"));
        QCOMPARE(hit.file, QStringLiteral("kfm.cns"));
        QCOMPARE(hit.line, 8);
        QCOMPARE(hit.triggerAll.size(), std::size_t(1));
        QCOMPARE(hit.triggers.size(), std::size_t(2)); // trigger4 is not reached
        QCOMPARE(hit.triggers[1].size(), std::size_t(2));
        QCOMPARE(hit.persistent, 0);
        QVERIFY(hit.ignoreHitPause);
        QVERIFY(!hit.param("attr")->isExpression());
        QCOMPARE(hit.param("attr")->text, QStringLiteral("S, NA"));
        QCOMPARE(hit.param("damage")->values.size(), std::size_t(2));
        QCOMPARE(s200->controllers[1].type, QStringLiteral("changestate"));
        QCOMPARE(s200->controllers[1].persistent, 1);

        QCOMPARE(data.issues().size(), qsizetype(2));
        QCOMPARE(data.issues()[0].file, QStringLiteral("kfm.cns"));
        QCOMPARE(data.issues()[0].line, 29);
        QVERIFY(data.issues()[1].message.contains("trigger1"));

        data.addStates("[Statedef 200]\nanim = 999\n[Statedef 5000]\ntype = S\n", "common1.cns", false);
        QVERIFY(data.state(5000));
        QCOMPARE(data.state(200)->controllers.size(), std::size_t(2));
        data.addStates("[Statedef 200]\nanim = 7\n", "kfm2.cns", true);
        QCOMPARE(data.state(200)->controllers.size(), std::size_t(0));
    }
};

QTEST_GUILESS_MAIN(RuntimeCharacterTest)
#include "RuntimeCharacterTest.moc"
