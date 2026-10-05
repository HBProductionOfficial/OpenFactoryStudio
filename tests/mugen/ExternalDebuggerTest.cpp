#include "engines/mugen/debugger/ExternalDebugger.h"
#include "core/debug/Breakpoints.h"
#include "core/debug/ProcessMemory.h"
#include "engines/mugen/debugger/DebugAddressMap.h"
#include "stubs/FakeEngineMemory.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QtEndian>

using namespace OFS;
using namespace OFS::Mugen;
using OFS::Test::FakeEngineMemory;

namespace {

constexpr std::uint32_t P1 = 0x03000000;
constexpr std::uint32_t P2 = 0x03100000;
constexpr std::uint32_t Helper = 0x03200000;
constexpr std::uint32_t OldHelper = 0x03300000;

DebugAddressMap shippedMap()
{
    const QList<DebugAddressMap> maps =
        DebugAddressMap::load(QStringLiteral(OFS_SOURCE_DATA_DIR) + QStringLiteral("/engines/mugen/debug.xml"));
    return maps.isEmpty() ? DebugAddressMap() : maps.front();
}

const char* const kStates = "[Data]\nlife = 1000\n"
                            "\n; Stand\n[Statedef 0]\ntype = S\n[State 0, 1]\ntype = Null\ntrigger1 = 1\n"
                            "\n; Punch\n[Statedef 200]\ntype = S\nmovetype = A\n[State 200, 1]\ntype = HitDef\n"
                            "trigger1 = AnimElem = 3\n";
const char* const kFile = "C:/chars/fighter/fighter.cns";

// P1 is the project's character with a helper, P2 is Kung Fu Man, plus a slot left by a gone
// helper.
struct Match
{
    FakeEngineMemory engine{shippedMap()};
    Debug::BreakpointList breakpoints;
    ExternalDebugger debugger{engine.map()};

    Match()
    {
        engine.addPlayer(0, P1, 32, "Test Fighter");
        engine.addPlayer(1, P2, 33, "Kung Fu Man");
        engine.addPlayer(4, Helper, 34, "Helper");
        engine.set(Helper, "player_helper_id", 1234);
        engine.set(Helper, "player_parent", 32);
        engine.set(Helper, "player_root", std::int32_t(P1));
        engine.set(Helper, "player_state_no", 7900);
        engine.addPlayer(5, OldHelper, 30, "Gone");
        engine.set(OldHelper, "player_active", 0);
        engine.set(P2, "player_facing", -1);
        engine.set(P2, "player_palette_no", 1);
        engine.setGlobal("game_time", 100);
        debugger.setMemory(&engine.image());
        debugger.setBreakpoints(&breakpoints);
        debugger.setProjectSlots({0});
        const QList<StatePlace> places = statedefs(QString::fromLatin1(kStates));
        debugger.setResolvers(
            [places](const QString& file, int line) {
                std::optional<StatePlace> place = stateOfLine(places, line);
                if (place)
                    place->file = file;
                return place;
            },
            [places](int state) -> std::optional<StatePlace> {
                for (const StatePlace& p : places) {
                    if (p.state == state)
                        return StatePlace{state, QString::fromLatin1(kFile), p.line};
                }
                return std::nullopt;
            });
    }

    ExternalDebugger::Look frame(bool running = true)
    {
        engine.frame({P1, P2, Helper});
        return debugger.look(running);
    }
};

QString valueOf(const QList<Debug::Variable>& list, const QString& path)
{
    for (const Debug::Variable& v : list) {
        if (v.path == path)
            return v.value;
        const QString inner = valueOf(v.children, path);
        if (!inner.isNull())
            return inner;
    }
    return QString();
}

QByteArray programFile(const QStringList& libraries)
{
    QByteArray file(0x400, '\0');
    file[0] = 'M';
    file[1] = 'Z';
    auto put32 = [&file](int at, std::uint32_t v) {
        qToLittleEndian(v, file.data() + at);
    };
    auto put16 = [&file](int at, std::uint16_t v) {
        qToLittleEndian(v, file.data() + at);
    };
    const int pe = 0x80;
    put32(0x3C, pe);
    file.replace(pe, 4, QByteArray("PE\0\0", 4));
    put16(pe + 4, 0x14C); // i386
    put16(pe + 6, 1);     // one section
    put16(pe + 20, 224);  // optional header size
    const int optional = pe + 24;
    put16(optional, 0x10B);
    put32(optional + 92, 16);         // data directories
    put32(optional + 96 + 8, 0x1000); // imports at RVA 0x1000
    const int section = optional + 224;
    file.replace(section, 6, QByteArray(".idata"));
    put32(section + 8, 0x200);   // virtual size
    put32(section + 12, 0x1000); // virtual address
    put32(section + 16, 0x200);  // raw size
    put32(section + 20, 0x200);  // raw pointer
    // Import descriptors at 0x200 (file), the names after them.
    int name = 0x200 + 20 * int(libraries.size() + 1);
    for (int i = 0; i < libraries.size(); ++i) {
        put32(0x200 + 20 * i + 12, std::uint32_t(0x1000 + name - 0x200));
        const QByteArray text = libraries[i].toLatin1();
        file.replace(name, int(text.size()), text);
        name += int(text.size()) + 1;
    }
    return file;
}

} // namespace

class ExternalDebuggerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void addressMap()
    {
        QString error;
        const QList<DebugAddressMap> maps = DebugAddressMap::parse(R"(<debug>
            <mugen version="WinBeta" imports="ALLEG40.DLL">
                <address name="game_base" value="4938572" absolute="true" type="pointer"/>
                <address name="screen_x" value="46120" type="float"/>
                <address name="player_name" value="32" type="string" count="48"/>
                <address name="player_var" value="3648" count="60"/>
                <address name="player_state_type" value="3584" enum="1:S,2:C"/>
                <address name="player_visible" value=""/>
            </mugen>
        </debug>)",
                                                                   &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(maps.size(), 1);
        const DebugAddressMap& map = maps.front();
        QCOMPARE(map.version(), QStringLiteral("winbeta"));
        QCOMPARE(map.imports(), QStringList{"alleg40.dll"});
        QVERIFY(map.known("game_base")->absolute);
        QCOMPARE(*map.known("game_base")->value, 4938572u);
        QCOMPARE(map.known("screen_x")->type, DebugAddress::Type::Float);
        QCOMPARE(map.known("player_name")->size(), 48);
        QCOMPARE(map.known("player_var")->size(), 240);
        QCOMPARE(map.known("player_state_type")->letters.value(2), QStringLiteral("C"));
        QVERIFY(map.find("player_visible"));
        QVERIFY(!map.known("player_visible"));
        QVERIFY(!map.find("player_life"));
        QCOMPARE(map.playerSize(), 3648 + 240);

        QVERIFY(DebugAddressMap::parse("<debug><mugen><address name=\"x\" value=\"12a\"/></mugen></debug>", &error)
                    .isEmpty());
        QVERIFY(error.contains("12a"));
        QVERIFY(DebugAddressMap::parse("<debug><mugen><address name=\"x\" value=\"1\" type=\"word\"/></mugen></debug>",
                                       &error)
                    .isEmpty());
        QVERIFY(error.contains("word"));
    }

    void shippedAddressMap()
    {
        const DebugAddressMap map = shippedMap();
        QCOMPARE(map.version(), QStringLiteral("winbeta"));
        QCOMPARE(map.imports(), QStringList{"alleg40.dll"});
        const QStringList names = {"game_base",
                                   "debug_target",
                                   "debug_mode",
                                   "cmd_buffer",
                                   "cmd_current",
                                   "cmd_last",
                                   "pause",
                                   "screen_x",
                                   "screen_y",
                                   "players",
                                   "player_active",
                                   "player_id",
                                   "player_helper_id",
                                   "player_team",
                                   "player_parent",
                                   "player_root",
                                   "player_name",
                                   "player_alive",
                                   "player_visible",
                                   "player_unhitable_time",
                                   "player_life",
                                   "player_power",
                                   "player_power_level",
                                   "player_facing",
                                   "player_spr_priority",
                                   "player_damage",
                                   "player_fall_damage",
                                   "player_palette_no",
                                   "player_pos_x",
                                   "player_pos_y",
                                   "player_pos_freeze",
                                   "player_vel_x",
                                   "player_vel_y",
                                   "player_state_redit_to",
                                   "player_state_no",
                                   "player_prev_state_no",
                                   "player_state_type",
                                   "player_move_type",
                                   "player_physics",
                                   "player_state_time",
                                   "player_ctrl",
                                   "player_guard",
                                   "player_hit_pause_time",
                                   "player_pause_move_time",
                                   "player_spause_move_time",
                                   "player_attack_mul",
                                   "player_defence_mul",
                                   "player_constrain",
                                   "player_camera_follows_x",
                                   "player_camera_follows_y",
                                   "player_push_checking",
                                   "player_edge_front",
                                   "player_edge_back",
                                   "player_player_front",
                                   "player_player_back",
                                   "player_move_hit",
                                   "player_enemy_hit",
                                   "player_hit_attr1",
                                   "player_hit_attr2",
                                   "player_var",
                                   "player_fvar",
                                   "player_isys_var",
                                   "player_sys_fvar",
                                   "game_time",
                                   "frame_paused"};
        for (const QString& name : names)
            QVERIFY2(map.find(name), qPrintable(name));
        QCOMPARE(map.addresses().size(), names.size());
        QVERIFY(map.known("game_base")->absolute);
        QVERIFY(map.known("pause")->absolute);
        QCOMPARE(map.known("player_var")->count, 60);
        QCOMPARE(map.known("player_fvar")->count, 40);
        QCOMPARE(map.known("player_isys_var")->count, 5);
        QCOMPARE(map.known("player_sys_fvar")->count, 5);
        for (const char* unknown : {"cmd_buffer", "player_damage", "player_guard", "player_pos_freeze",
                                    "player_state_redit_to", "player_visible"})
            QVERIFY2(!map.known(QLatin1String(unknown)), unknown);
    }

    void programImports()
    {
        QTemporaryDir dir;
        const QString exe = dir.filePath("engine.exe");
        QFile file(exe);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(programFile({"KERNEL32.DLL", "alleg40.dll", "USER32.dll"}));
        file.close();
        QCOMPARE(Debug::importedLibraries(exe), (QStringList{"kernel32.dll", "alleg40.dll", "user32.dll"}));
        const QList<DebugAddressMap> maps{shippedMap()};
        QVERIFY(DebugAddressMap::forExecutable(maps, exe).has_value());

        const QString other = dir.filePath("other.exe");
        QFile second(other);
        QVERIFY(second.open(QIODevice::WriteOnly));
        second.write(programFile({"SDL.dll", "KERNEL32.dll"}));
        second.close();
        QVERIFY(!DebugAddressMap::forExecutable(maps, other).has_value());
        QFile text(dir.filePath("readme.txt"));
        QVERIFY(text.open(QIODevice::WriteOnly));
        text.write("MZ but nothing else");
        text.close();
        QVERIFY(Debug::importedLibraries(dir.filePath("readme.txt")).isEmpty());
        QVERIFY(Debug::importedLibraries(dir.filePath("missing.exe")).isEmpty());
    }

    void commandLineSlots()
    {
        QCOMPARE(playerSlotsOf({"ofs_0", "kfm"}, "ofs_0"), QSet<int>{0});
        QCOMPARE(playerSlotsOf({"kfm", "ofs_0"}, "ofs_0"), QSet<int>{1});
        QCOMPARE(playerSlotsOf({"ofs_0", "ofs_0", "-p2.ai", "1"}, "ofs_0"), (QSet<int>{0, 1}));
        QCOMPARE(playerSlotsOf({"ofs_0", "kfm", "-p3", "ofs_0", "-p4", "kfm"}, "ofs_0"), (QSet<int>{0, 2}));
        QCOMPARE(playerSlotsOf({"-nosound", "ofs_1/ofs_1.def", "kfm"}, "ofs_1"), QSet<int>{0});
        QVERIFY(playerSlotsOf({"kfm", "kfm", "-s", "ofs_0"}, "ofs_0").isEmpty());
        QVERIFY(playerSlotsOf({"-r", "ofs_0"}, "ofs_0").isEmpty());
    }

    void statedefLines()
    {
        const QList<StatePlace> places = statedefs(QString::fromLatin1(kStates));
        QCOMPARE(places.size(), 2);
        QCOMPARE(places[0].state, 0);
        QCOMPARE(places[0].line, 5);
        QCOMPARE(places[1].state, 200);
        QCOMPARE(places[1].line, 12);
        QVERIFY(!stateOfLine(places, 2));
        QCOMPARE(stateOfLine(places, 7)->state, 0);
        QCOMPARE(stateOfLine(places, 11)->state, 0);
        QCOMPARE(stateOfLine(places, 12)->state, 200);
        QCOMPARE(stateOfLine(places, 17)->state, 200);
        QCOMPARE(statedefs("  [ StateDef -2 ]\n").front().state, -2);
    }

    void variables()
    {
        Match m;
        m.engine.set(P1, "player_var", 12345, 0);
        m.engine.set(P1, "player_var", -7, 59);
        m.engine.setFloat(P1, "player_fvar", -2.25f, 39);
        m.engine.set(P1, "player_isys_var", 44, 4);
        m.engine.setFloat(P1, "player_attack_mul", 2.0f);
        m.engine.setFloat(P1, "player_pos_x", 90.0f);
        m.engine.set(P1, "player_state_type", 3);
        m.engine.set(P1, "player_move_type", 1);
        m.engine.setGlobal("screen_x", 52);
        m.debugger.look(true);
        const QList<Debug::Variable> vars = m.debugger.variables();
        QCOMPARE(vars.size(), 4);
        QCOMPARE(vars[0].name, QStringLiteral("Game"));
        QCOMPARE(vars[1].name, QStringLiteral("P1: Test Fighter (32)"));
        QCOMPARE(vars[2].name, QStringLiteral("P2: Kung Fu Man (33)"));
        QCOMPARE(vars[3].name, QStringLiteral("Helper 1234 (34)"));
        QCOMPARE(valueOf(vars, "game.time"), QStringLiteral("100"));
        QCOMPARE(valueOf(vars, "game.pause"), QStringLiteral("0"));
        QCOMPARE(valueOf(vars, "game.screen x"), QStringLiteral("52"));
        QCOMPARE(valueOf(vars, "32.life"), QStringLiteral("1000"));
        QCOMPARE(valueOf(vars, "32.var(0)"), QStringLiteral("12345"));
        QCOMPARE(valueOf(vars, "32.var(59)"), QStringLiteral("-7"));
        QCOMPARE(valueOf(vars, "32.fvar(39)"), QStringLiteral("-2.25"));
        QCOMPARE(valueOf(vars, "32.sysvar(4)"), QStringLiteral("44"));
        QCOMPARE(valueOf(vars, "32.attack mul"), QStringLiteral("2"));
        QCOMPARE(valueOf(vars, "32.pos x"), QStringLiteral("90"));
        QCOMPARE(valueOf(vars, "32.statetype"), QStringLiteral("A"));
        QCOMPARE(valueOf(vars, "32.movetype"), QStringLiteral("A"));
        QCOMPARE(valueOf(vars, "32.physics"), QStringLiteral("S"));
        QCOMPARE(valueOf(vars, "33.palno"), QStringLiteral("2")); // counted from 1, as PalNo
        QCOMPARE(valueOf(vars, "34.helper id"), QStringLiteral("1234"));
        QCOMPARE(valueOf(vars, "34.parent"), QStringLiteral("32"));
        QCOMPARE(valueOf(vars, "34.root"), QStringLiteral("32"));
        QCOMPARE(valueOf(vars, "32.damage"), QStringLiteral("N/A"));
        QCOMPARE(valueOf(vars, "32.guard"), QStringLiteral("N/A"));
        QCOMPARE(vars[1].children.size(), vars[2].children.size());

        QCOMPARE(m.debugger.variable("32.var(59)")->value, QStringLiteral("-7"));
        QCOMPARE(m.debugger.variable("32.var")->children.size(), 60);
        QCOMPARE(m.debugger.variable("game.time")->type, QStringLiteral("int"));
        QCOMPARE(m.debugger.variable("32.statetype")->type, QStringLiteral("char"));
        QVERIFY(!m.debugger.variable("30.life"));
        QVERIFY(!m.debugger.variable("32.nothing"));

        m.engine.set(P1, "player_life", 903);
        m.frame();
        QCOMPARE(m.debugger.variable("32.life")->value, QStringLiteral("903"));
        QCOMPARE(m.debugger.variable("game.time")->value, QStringLiteral("101"));
    }

    void pauseAndStep()
    {
        Match m;
        m.debugger.look(true);
        QVERIFY(m.debugger.setPaused(true));
        QCOMPARE(m.engine.global("pause"), 1);
        QCOMPARE(m.debugger.look(true).paused, std::optional<bool>(true));
        QVERIFY(m.debugger.setPaused(false));
        QCOMPARE(m.engine.global("pause"), 0);

        m.debugger.setPaused(true);
        m.engine.playLoop({P1, P2, Helper});
        for (int i = 0; i < 3; ++i) {
            const int before = m.engine.global("game_time");
            QVERIFY(m.debugger.stepFrame());
            QCOMPARE(m.engine.global("game_time"), before + 1);
            QCOMPARE(m.engine.global("pause"), 1);
        }
        m.engine.image().onRead = nullptr;
        QVERIFY(!m.debugger.stepFrame(50));
        QCOMPARE(m.engine.global("pause"), 1);

        ExternalDebugger idle{shippedMap()};
        QVERIFY(!idle.setPaused(true));
        QVERIFY(!idle.look(true).readable);
        QVERIFY(idle.variables().size() == 1);
        QCOMPARE(valueOf(idle.variables(), "game.time"), QStringLiteral("N/A"));
    }

    void stateBreakpoints()
    {
        Match m;
        // A line inside [Statedef 200] of the project's states.
        QVERIFY(m.breakpoints.toggle(kFile, 13));
        QVERIFY(!m.debugger.look(true).hit);
        QVERIFY(!m.frame().hit);
        m.engine.changeState(P2, 200);
        QVERIFY(!m.debugger.look(true).hit);
        QCOMPARE(m.engine.global("pause"), 0);
        m.engine.changeState(P1, 200);
        const ExternalDebugger::Look look = m.debugger.look(true);
        QVERIFY(look.hit);
        QCOMPARE(m.engine.global("pause"), 1);
        QVERIFY(m.debugger.location().has_value());
        QCOMPARE(m.debugger.location()->place.state, 200);
        QCOMPARE(m.debugger.location()->place.line, 12);
        QCOMPARE(m.debugger.location()->place.file, QString::fromLatin1(kFile));
        QCOMPARE(m.debugger.location()->playerName, QStringLiteral("P1: Test Fighter (32)"));
        const QList<Debug::StackFrame> stack = m.debugger.stack();
        QCOMPARE(stack.size(), 2);
        QCOMPARE(stack[0].function, QStringLiteral("P1: Test Fighter (32): [Statedef 200]"));
        QCOMPARE(stack[0].line, 12);
        QCOMPARE(stack[1].function, QStringLiteral("Helper 1234 (34): [Statedef 7900]"));
        QVERIFY(stack[1].file.isEmpty());

        m.debugger.setPaused(false);
        m.debugger.clearLocation();
        QVERIFY(!m.frame().hit);
        QVERIFY(!m.frame().hit);
        m.engine.changeState(P1, 200);
        QVERIFY(m.debugger.look(true).hit);
        m.debugger.setPaused(false);
        m.engine.changeState(Helper, 200);
        QVERIFY(m.debugger.look(true).hit);
        QCOMPARE(m.debugger.location()->playerName, QStringLiteral("Helper 1234 (34)"));
        m.engine.changeState(P1, 0);
        m.debugger.look(false);
        m.engine.changeState(P1, 200);
        QVERIFY(!m.debugger.look(false).hit);
        m.breakpoints.removeAll();
        m.breakpoints.toggle(kFile, 2);
        m.engine.changeState(P1, 0);
        m.debugger.look(true);
        m.engine.changeState(P1, 200);
        QVERIFY(!m.debugger.look(true).hit);
    }

    void breakpointOptions()
    {
        Match m;
        Debug::Breakpoint b;
        b.file = QString::fromLatin1(kFile);
        b.line = 12;
        b.ignoreCount = 1;
        const int number = m.breakpoints.add(b);
        b.number = number;
        m.debugger.look(true);
        auto enter = [&m](std::uint32_t player) {
            m.engine.changeState(player, 0);
            m.debugger.look(true);
            m.engine.changeState(player, 200);
            return m.debugger.look(true).hit;
        };
        QVERIFY(!enter(P1));
        QVERIFY(enter(P1));
        m.breakpoints.setEnabled(number, false);
        QVERIFY(!enter(P1));
        Debug::Breakpoint c = b;
        c.ignoreCount = 0;
        c.condition = QStringLiteral("var(3) = 5 && life > 500 && statetype = S");
        m.breakpoints.update(c);
        m.breakpoints.setEnabled(number, true);
        QVERIFY(!enter(P1));
        m.engine.set(P1, "player_var", 5, 3);
        QVERIFY(enter(P1));
        c.condition = QStringLiteral("playerid(33), palno = 2");
        m.breakpoints.update(c);
        QVERIFY(enter(P1));
        c.condition = QStringLiteral("root, var(3) = 5 && parent, id = 32");
        m.breakpoints.update(c);
        QVERIFY(!enter(P1));
        QVERIFY(enter(Helper));
        c.condition = QStringLiteral("enemy, life = 1000");
        m.breakpoints.update(c);
        QVERIFY(!enter(P1));
        c.condition = QStringLiteral("var(3) = ");
        m.breakpoints.update(c);
        QVERIFY(!enter(P1));
        c.condition.clear();
        c.oneShot = true;
        m.breakpoints.update(c);
        QVERIFY(enter(P1));
        QVERIFY(!m.breakpoints.find(number)->enabled);
        QVERIFY(!enter(P1));
    }

    void frameAndDataBreakpoints()
    {
        Match m;
        Debug::Breakpoint frame;
        frame.type = Debug::BreakpointType::Frame;
        frame.ignoreCount = 2;
        const int number = m.breakpoints.add(frame);
        m.debugger.look(true);
        QVERIFY(!m.frame().hit);
        QVERIFY(!m.frame().hit);
        QVERIFY(m.frame().hit);
        QCOMPARE(m.engine.global("pause"), 1);
        m.debugger.setPaused(false);
        QVERIFY(!m.debugger.look(true).hit);
        m.breakpoints.remove(number);

        Debug::Breakpoint life;
        life.type = Debug::BreakpointType::Memory;
        life.address = QStringLiteral("32.life");
        m.breakpoints.add(life);
        QVERIFY(!m.frame().hit);
        QVERIFY(!m.frame().hit);
        m.engine.set(P1, "player_life", 950);
        QVERIFY(m.frame().hit);
        m.engine.set(P1, "player_life", 900);
        m.frame(false);
        m.debugger.setPaused(false);
        QVERIFY(!m.frame().hit);
        m.breakpoints.removeAll();

        Debug::Breakpoint raw;
        raw.type = Debug::BreakpointType::Memory;
        raw.address = QStringLiteral("0x%1").arg(P2 + *m.engine.map().known("player_power")->value, 0, 16);
        m.breakpoints.add(raw);
        QVERIFY(!m.frame().hit);
        m.engine.set(P2, "player_power", 500);
        QVERIFY(m.frame().hit);
        QCOMPARE(m.debugger.location()->playerName, QStringLiteral("P1: Test Fighter (32)"));
    }

    void enginePause()
    {
        // While the engine starts, its memory may hold anything.
        Match m;
        m.engine.setGlobal("pause", 1);
        QCOMPARE(m.debugger.look(true).paused, std::optional<bool>(true));
        m.engine.image().setInt(*m.engine.map().known("game_base")->value, 0);
        const ExternalDebugger::Look early = m.debugger.look(true);
        QVERIFY(!early.paused.has_value());
        QVERIFY(m.debugger.players().isEmpty());
        QVERIFY(!m.debugger.setPaused(false));
        QCOMPARE(m.engine.global("pause"), 1);
    }
};

QTEST_GUILESS_MAIN(ExternalDebuggerTest)
#include "ExternalDebuggerTest.moc"
