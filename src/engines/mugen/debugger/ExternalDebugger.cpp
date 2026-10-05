#include "engines/mugen/debugger/ExternalDebugger.h"

#include "core/debug/ProcessMemory.h"
#include "engines/mugen/runtime/Expr.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QThread>
#include <QtEndian>

#include <cstring>
#include <list>

namespace OFS::Mugen {

using namespace Runtime;

namespace {

struct ExternalDebuggerTexts
{
    Q_DECLARE_TR_FUNCTIONS(OFS::Mugen::ExternalDebuggerTexts)
};

struct Row
{
    const char* name;
    const char* field;
};

constexpr Row kPlayerRows[] = {
    {"id", "player_id"},
    {"helper id", "player_helper_id"},
    {"team", "player_team"},
    {"parent", "player_parent"},
    {"root", "player_root"},
    {"life", "player_life"},
    {"power", "player_power"},
    {"power level", "player_power_level"},
    {"pos x", "player_pos_x"},
    {"pos y", "player_pos_y"},
    {"vel x", "player_vel_x"},
    {"vel y", "player_vel_y"},
    {"facing", "player_facing"},
    {"stateno", "player_state_no"},
    {"prevstateno", "player_prev_state_no"},
    {"statetype", "player_state_type"},
    {"movetype", "player_move_type"},
    {"physics", "player_physics"},
    {"time", "player_state_time"},
    {"ctrl", "player_ctrl"},
    {"alive", "player_alive"},
    {"palno", "player_palette_no"},
    {"visible", "player_visible"},
    {"sprpriority", "player_spr_priority"},
    {"attack mul", "player_attack_mul"},
    {"defence mul", "player_defence_mul"},
    {"damage", "player_damage"},
    {"fall damage", "player_fall_damage"},
    {"unhitable time", "player_unhitable_time"},
    {"move hit", "player_move_hit"},
    {"enemy hit", "player_enemy_hit"},
    {"hit attr 1", "player_hit_attr1"},
    {"hit attr 2", "player_hit_attr2"},
    {"guard", "player_guard"},
    {"pos freeze", "player_pos_freeze"},
    {"state redirect", "player_state_redit_to"},
    {"hit pause time", "player_hit_pause_time"},
    {"pause move time", "player_pause_move_time"},
    {"super pause move time", "player_spause_move_time"},
    {"constrain", "player_constrain"},
    {"camera follows x", "player_camera_follows_x"},
    {"camera follows y", "player_camera_follows_y"},
    {"push checking", "player_push_checking"},
    {"edge front", "player_edge_front"},
    {"edge back", "player_edge_back"},
    {"player front", "player_player_front"},
    {"player back", "player_player_back"},
};

constexpr Row kPlayerArrays[] = {
    {"var", "player_var"},
    {"fvar", "player_fvar"},
    {"sysvar", "player_isys_var"},
    {"sysfvar", "player_sys_fvar"},
};

constexpr Row kGameRows[] = {
    {"time", "game_time"},    {"pause", "pause"},           {"screen x", "screen_x"},
    {"screen y", "screen_y"}, {"debug mode", "debug_mode"}, {"debug target", "debug_target"},
};

constexpr const char* kGlobals[] = {"game_time", "pause",      "frame_paused", "screen_x",
                                    "screen_y",  "debug_mode", "debug_target"};

std::optional<std::uint32_t> wordAt(const QByteArray& bytes, qsizetype offset)
{
    if (offset < 0 || offset + 4 > bytes.size())
        return std::nullopt;
    return qFromLittleEndian<std::uint32_t>(bytes.constData() + offset);
}

std::optional<std::int32_t> intAt(const DebugAddress& a, const QByteArray& bytes, qsizetype offset)
{
    if (a.type == DebugAddress::Type::Byte) {
        if (offset < 0 || offset >= bytes.size())
            return std::nullopt;
        return std::int32_t(static_cast<unsigned char>(bytes[offset]));
    }
    const std::optional<std::uint32_t> word = wordAt(bytes, offset);
    if (!word)
        return std::nullopt;
    if (a.type == DebugAddress::Type::Float) {
        float f = 0;
        std::memcpy(&f, &*word, sizeof f);
        return Value::real(f).toInt();
    }
    return static_cast<std::int32_t>(*word);
}

std::optional<float> floatAt(const DebugAddress& a, const QByteArray& bytes, qsizetype offset)
{
    if (a.type != DebugAddress::Type::Float) {
        const std::optional<std::int32_t> i = intAt(a, bytes, offset);
        return i ? std::optional<float>(float(*i)) : std::nullopt;
    }
    const std::optional<std::uint32_t> word = wordAt(bytes, offset);
    if (!word)
        return std::nullopt;
    float f = 0;
    std::memcpy(&f, &*word, sizeof f);
    return f;
}

QString typeName(const DebugAddress* a)
{
    if (!a)
        return QStringLiteral("int");
    if (!a->letters.isEmpty())
        return QStringLiteral("char");
    switch (a->type) {
    case DebugAddress::Type::Float:
        return QStringLiteral("float");
    case DebugAddress::Type::String:
        return QStringLiteral("string");
    case DebugAddress::Type::Int:
    case DebugAddress::Type::Byte:
    case DebugAddress::Type::Pointer:
        break;
    }
    return QStringLiteral("int");
}

QString notAvailable()
{
    return ExternalDebuggerTexts::tr("N/A");
}

const Debug::Variable* findVariable(const QList<Debug::Variable>& list, const QString& path)
{
    for (const Debug::Variable& v : list) {
        if (v.path == path)
            return &v;
        // "32" holds "32.life" and "32.var" holds "32.var(3)". "320.life" belongs to another player.
        const QChar next = path.size() > v.path.size() ? path.at(v.path.size()) : QChar();
        if (path.startsWith(v.path) && (next == QLatin1Char('.') || next == QLatin1Char('('))) {
            if (const Debug::Variable* found = findVariable(v.children, path))
                return found;
        }
    }
    return nullptr;
}

QString playerKey(const ExternalDebugger::Player& p, std::optional<std::int32_t> id)
{
    return QStringLiteral("%1:%2").arg(p.address).arg(id.value_or(-1));
}

class PlayerContext : public Context
{
public:
    PlayerContext(const ExternalDebugger& debugger, const ExternalDebugger::Player& player)
        : m_debugger(debugger)
        , m_player(player)
    {}

    Value trigger(const TriggerCall& call) override
    {
        auto arg = [&call](int i) {
            return i < int(call.args.size()) ? call.args[std::size_t(i)] : Value::undefined();
        };
        auto special = [&call](bool match) {
            return Value::boolean(match != call.negate);
        };
        auto integer = [this](const char* field, int index = 0) {
            const std::optional<std::int32_t> v = m_debugger.intField(m_player, QLatin1String(field), index);
            return v ? Value::integer(*v) : Value::undefined();
        };
        auto real = [this](const char* field, int index = 0) {
            const std::optional<float> v = m_debugger.floatField(m_player, QLatin1String(field), index);
            return v ? Value::real(*v) : Value::undefined();
        };
        auto letter = [&](const char* field) {
            const DebugAddress* a = m_debugger.map().known(QLatin1String(field));
            const std::optional<std::int32_t> code = m_debugger.intField(m_player, QLatin1String(field));
            if (!a || !code || !a->letters.contains(*code))
                return Value::undefined();
            return special(call.keyword.startsWith(a->letters.value(*code), Qt::CaseInsensitive));
        };
        switch (call.trigger) {
        case Trigger::Time:
            return integer("player_state_time");
        case Trigger::StateNo:
            return integer("player_state_no");
        case Trigger::PrevStateNo:
            return integer("player_prev_state_no");
        case Trigger::Life:
            return integer("player_life");
        case Trigger::Power:
            return integer("player_power");
        case Trigger::Facing:
            return integer("player_facing");
        case Trigger::Alive:
            return integer("player_alive");
        case Trigger::Ctrl: {
            const Value ctrl = integer("player_ctrl");
            return ctrl.isUndefined() ? ctrl : Value::boolean(ctrl.toBool());
        }
        case Trigger::Var:
            return integer("player_var", arg(0).toInt());
        case Trigger::FVar:
            return real("player_fvar", arg(0).toInt());
        case Trigger::SysVar:
            return integer("player_isys_var", arg(0).toInt());
        case Trigger::SysFVar:
            return real("player_sys_fvar", arg(0).toInt());
        case Trigger::PalNo: {
            // The engine counts palettes from 0, the trigger from 1.
            const Value palette = integer("player_palette_no");
            return palette.isUndefined() ? palette : Value::integer(palette.i + 1);
        }
        case Trigger::ID:
            return integer("player_id");
        case Trigger::TeamSide:
            return integer("player_team");
        case Trigger::IsHelper: {
            const bool helper = m_player.slot >= 4;
            if (call.args.empty())
                return Value::boolean(helper);
            const Value id = integer("player_helper_id");
            return Value::boolean(helper && !id.isUndefined() && id.i == arg(0).toInt());
        }
        case Trigger::GameTime: {
            const std::optional<std::int32_t> time = m_debugger.gameInt(QStringLiteral("game_time"));
            return time ? Value::integer(*time) : Value::undefined();
        }
        case Trigger::StateType:
            return letter("player_state_type");
        case Trigger::MoveType:
            return letter("player_move_type");
        case Trigger::Vel: {
            if (call.keyword == QLatin1String("y"))
                return real("player_vel_y");
            // The engine stores the speed in screen direction. The trigger gives it toward the player's front.
            const Value x = real("player_vel_x");
            const Value facing = integer("player_facing");
            if (x.isUndefined() || facing.isUndefined())
                return Value::undefined();
            return Value::real(facing.i < 0 ? -x.f : x.f);
        }
        case Trigger::Pos:
            // Pos X is measured from the screen's centre, which the map does not give.
            return call.keyword == QLatin1String("y") ? real("player_pos_y") : Value::undefined();
        default:
            break;
        }
        return Value::undefined();
    }

    Context* redirect(Redirect kind, const Value& argument) override
    {
        const ExternalDebugger::Player* target = nullptr;
        switch (kind) {
        case Redirect::Parent:
            if (const std::optional<std::int32_t> id = m_debugger.intField(m_player, QStringLiteral("player_parent")))
                target = m_debugger.playerById(*id);
            break;
        case Redirect::Root:
            if (const std::optional<std::int32_t> root = m_debugger.intField(m_player, QStringLiteral("player_root")))
                target = m_debugger.playerAt(std::uint32_t(*root));
            break;
        case Redirect::PlayerID:
            target = m_debugger.playerById(argument.toInt());
            break;
        default:
            break;
        }
        if (!target)
            return nullptr;
        m_redirects.emplace_back(m_debugger, *target);
        return &m_redirects.back();
    }

    Value assign(Trigger kind, std::int32_t index, const Value& value) override
    {
        Q_UNUSED(kind)
        Q_UNUSED(index)
        Q_UNUSED(value)
        return Value::undefined();
    }

private:
    const ExternalDebugger& m_debugger;
    const ExternalDebugger::Player& m_player;
    std::list<PlayerContext> m_redirects;
};

} // namespace

QList<StatePlace> statedefs(const QString& text)
{
    static const QRegularExpression header(QStringLiteral("^\\s*\\[\\s*statedef\\s+(-?\\d+)"),
                                           QRegularExpression::CaseInsensitiveOption);
    QList<StatePlace> out;
    int number = 0;
    for (QStringView row : QStringView(text).split(u'\n')) {
        ++number;
        if (!row.contains(u'['))
            continue;
        const QRegularExpressionMatch m = header.matchView(row);
        if (m.hasMatch())
            out << StatePlace{m.captured(1).toInt(), QString(), number};
    }
    return out;
}

std::optional<StatePlace> stateOfLine(const QList<StatePlace>& statedefs, int line)
{
    std::optional<StatePlace> found;
    for (const StatePlace& place : statedefs) {
        if (place.line > line)
            break;
        found = place;
    }
    return found;
}

QSet<int> playerSlotsOf(const QStringList& arguments, const QString& name)
{
    static const QStringList flags = {QStringLiteral("-h"),        QStringLiteral("-?"),
                                      QStringLiteral("-help"),     QStringLiteral("-nosound"),
                                      QStringLiteral("-nomusic"),  QStringLiteral("-nojoy"),
                                      QStringLiteral("-windowed"), QStringLiteral("-fullscreen")};
    static const QStringList players = {QStringLiteral("-p1"), QStringLiteral("-p2"), QStringLiteral("-p3"),
                                        QStringLiteral("-p4")};
    auto isProject = [&name](const QString& argument) {
        return !name.isEmpty() && QString(argument)
                                          .replace(QLatin1Char('\\'), QLatin1Char('/'))
                                          .section(QLatin1Char('/'), 0, 0)
                                          .compare(name, Qt::CaseInsensitive) == 0;
    };
    QSet<int> slots;
    int plain = 0;
    for (qsizetype i = 0; i < arguments.size(); ++i) {
        const QString& argument = arguments[i];
        if (argument.startsWith(QLatin1Char('-'))) {
            const QString option = argument.toLower();
            if (const qsizetype p = players.indexOf(option); p >= 0 && i + 1 < arguments.size()) {
                if (isProject(arguments[i + 1]))
                    slots.insert(int(p));
                ++i;
            } else if (!flags.contains(option)) {
                ++i;
            }
            continue;
        }
        if (plain < 2 && isProject(argument))
            slots.insert(plain);
        ++plain;
    }
    return slots;
}

ExternalDebugger::ExternalDebugger(DebugAddressMap map)
    : m_map(std::move(map))
{}

ExternalDebugger::~ExternalDebugger() = default;

void ExternalDebugger::setResolvers(LineResolver lines, StateResolver states)
{
    m_lines = std::move(lines);
    m_states = std::move(states);
}

void ExternalDebugger::reset()
{
    m_game = 0;
    m_globals.clear();
    m_players.clear();
    m_location.reset();
    m_lastTime.reset();
    m_seen.clear();
    m_hits.clear();
    m_dataValues.clear();
}

void ExternalDebugger::read()
{
    m_game = 0;
    m_globals.clear();
    m_players.clear();
    if (!m_memory)
        return;
    if (const DebugAddress* base = m_map.known(QStringLiteral("game_base")))
        m_game = m_memory->readUInt(*base->value).value_or(0);
    // While the engine starts up the pointer may hold anything, so it only counts once its table reads.
    const DebugAddress* table = m_map.known(QStringLiteral("players"));
    const QByteArray pointers =
        m_game && table ? m_memory->bytes(m_game + *table->value, std::size_t(table->size())) : QByteArray();
    if (pointers.isEmpty())
        m_game = 0;
    for (const char* name : kGlobals) {
        const DebugAddress* a = m_map.known(QLatin1String(name));
        if (!a || (!a->absolute && !m_game))
            continue;
        const QByteArray bytes = m_memory->bytes(a->absolute ? *a->value : m_game + *a->value, std::size_t(a->size()));
        if (!bytes.isEmpty())
            m_globals.insert(a->name, bytes);
    }
    if (!m_game)
        return;
    const DebugAddress* active = m_map.known(QStringLiteral("player_active"));
    const int size = m_map.playerSize();
    for (int slot = 0; slot < table->count; ++slot) {
        const std::uint32_t address = wordAt(pointers, slot * 4).value_or(0);
        if (address == 0)
            continue;
        // Helper slots stay allocated after their helper is gone.
        if (active && m_memory->readInt(std::uint64_t(address) + *active->value) != std::optional<std::int32_t>(1))
            continue;
        const QByteArray bytes = m_memory->bytes(address, std::size_t(size));
        if (!bytes.isEmpty())
            m_players << Player{address, slot, bytes};
    }
}

std::optional<std::int32_t> ExternalDebugger::intField(const Player& player, const QString& name, int index) const
{
    const DebugAddress* a = m_map.known(name);
    if (!a || index < 0 || index >= a->count || a->absolute)
        return std::nullopt;
    return intAt(*a, player.bytes, qsizetype(*a->value) + qsizetype(index) * a->elementSize());
}

std::optional<float> ExternalDebugger::floatField(const Player& player, const QString& name, int index) const
{
    const DebugAddress* a = m_map.known(name);
    if (!a || index < 0 || index >= a->count || a->absolute)
        return std::nullopt;
    return floatAt(*a, player.bytes, qsizetype(*a->value) + qsizetype(index) * a->elementSize());
}

std::optional<std::int32_t> ExternalDebugger::gameInt(const QString& name) const
{
    const DebugAddress* a = m_map.known(name);
    const auto it = m_globals.constFind(name);
    if (!a || it == m_globals.cend())
        return std::nullopt;
    return intAt(*a, *it, 0);
}

const ExternalDebugger::Player* ExternalDebugger::playerById(int id) const
{
    for (const Player& p : m_players) {
        if (intField(p, QStringLiteral("player_id")) == std::optional<std::int32_t>(id))
            return &p;
    }
    return nullptr;
}

const ExternalDebugger::Player* ExternalDebugger::playerAt(std::uint64_t address) const
{
    for (const Player& p : m_players) {
        if (p.address == address)
            return &p;
    }
    return nullptr;
}

QString ExternalDebugger::label(const Player& player) const
{
    const QString id = QString::number(intField(player, QStringLiteral("player_id")).value_or(0));
    if (player.slot >= 4)
        return QStringLiteral("Helper %1 (%2)")
            .arg(intField(player, QStringLiteral("player_helper_id")).value_or(0))
            .arg(id);
    QString name;
    if (const DebugAddress* a = m_map.known(QStringLiteral("player_name"))) {
        const QByteArray raw = player.bytes.mid(qsizetype(*a->value), a->size());
        name = QString::fromLatin1(raw.left(raw.indexOf('\0') < 0 ? raw.size() : raw.indexOf('\0')));
    }
    return QStringLiteral("P%1: %2 (%3)").arg(player.slot + 1).arg(name, id);
}

bool ExternalDebugger::isProject(const Player& player) const
{
    if (player.slot < 4)
        return m_projectSlots.contains(player.slot);
    const std::optional<std::int32_t> root = intField(player, QStringLiteral("player_root"));
    const Player* owner = root ? playerAt(std::uint32_t(*root)) : nullptr;
    return owner && owner->slot < 4 && m_projectSlots.contains(owner->slot);
}

QList<const ExternalDebugger::Player*> ExternalDebugger::projectPlayers() const
{
    QList<const Player*> out;
    for (const Player& p : m_players) {
        if (isProject(p))
            out << &p;
    }
    return out;
}

ExternalDebugger::Look ExternalDebugger::look(bool checkBreakpoints)
{
    Look out;
    read();
    // Before the game structure exists the engine may still be unpacking itself.
    const std::optional<std::int32_t> pause = m_game ? gameInt(QStringLiteral("pause")) : std::nullopt;
    if (pause && (*pause == 0 || *pause == 1)) {
        out.readable = true;
        out.paused = *pause != 0;
    }
    const std::optional<std::int32_t> now = gameInt(QStringLiteral("game_time"));
    const bool newFrame = now && m_lastTime && *now != *m_lastTime;
    const QList<const Player*> mine = projectPlayers();
    const Player* first = !mine.isEmpty() ? mine.front() : !m_players.isEmpty() ? &m_players.front() : nullptr;
    auto entered = [this](const Player& p, int state) {
        const std::optional<std::int32_t> stateNo = intField(p, QStringLiteral("player_state_no"));
        const std::optional<std::int32_t> time = intField(p, QStringLiteral("player_state_time"));
        if (stateNo != std::optional<std::int32_t>(state))
            return false;
        const auto it = m_seen.constFind(playerKey(p, intField(p, QStringLiteral("player_id"))));
        if (it == m_seen.cend())
            return time.value_or(0) <= 1; // a new helper, made in the state
        return it->state != state || time.value_or(0) < it->time;
    };
    bool stopped = false;
    std::optional<Location> where;
    const QList<Debug::Breakpoint> list = m_breakpoints ? m_breakpoints->breakpoints() : QList<Debug::Breakpoint>();
    for (const Debug::Breakpoint& b : list) {
        if (!b.enabled)
            continue;
        switch (b.type) {
        case Debug::BreakpointType::FileLine: {
            if (!checkBreakpoints || stopped || !m_lines)
                break;
            const std::optional<StatePlace> place = m_lines(b.file, b.line);
            if (!place || place->state < 0) // [Statedef -1] to -3 are never entered
                break;
            for (const Player* p : mine) {
                if (entered(*p, place->state) && hit(b, p)) {
                    stopped = true;
                    where = Location{intField(*p, QStringLiteral("player_id")).value_or(0), label(*p), *place};
                    break;
                }
            }
            break;
        }
        case Debug::BreakpointType::Frame:
            if (checkBreakpoints && !stopped && newFrame && hit(b, first)) {
                stopped = true;
                where = currentLocation();
            }
            break;
        case Debug::BreakpointType::Memory: {
            const QString key = b.address.trimmed();
            const QString value = dataValue(key);
            const bool known = m_dataValues.contains(key);
            const QString before = m_dataValues.value(key);
            m_dataValues.insert(key, value);
            if (checkBreakpoints && !stopped && known && before != value && hit(b, first)) {
                stopped = true;
                where = currentLocation();
            }
            break;
        }
        case Debug::BreakpointType::Function:
            break;
        }
    }
    m_seen.clear();
    for (const Player& p : m_players) {
        m_seen.insert(playerKey(p, intField(p, QStringLiteral("player_id"))),
                      Seen{intField(p, QStringLiteral("player_state_no")).value_or(0),
                           intField(p, QStringLiteral("player_state_time")).value_or(0)});
    }
    if (now)
        m_lastTime = now;
    if (stopped) {
        setPaused(true);
        m_location = where;
        out.hit = true;
        out.paused = true;
    }
    return out;
}

bool ExternalDebugger::hit(const Debug::Breakpoint& breakpoint, const Player* player)
{
    if (!breakpoint.condition.trimmed().isEmpty() && !conditionHolds(breakpoint.condition, player))
        return false;
    const int times = ++m_hits[breakpoint.number];
    if (times <= breakpoint.ignoreCount)
        return false;
    if (breakpoint.oneShot && m_breakpoints)
        m_breakpoints->setEnabled(breakpoint.number, false);
    return true;
}

bool ExternalDebugger::conditionHolds(const QString& condition, const Player* player) const
{
    if (!player)
        return false;
    QString error;
    const Expr expr = Expr::compile(condition, &error);
    if (!error.isEmpty() || expr.isEmpty())
        return false;
    PlayerContext context(*this, *player);
    return expr.evaluate(context).toBool();
}

bool ExternalDebugger::setPaused(bool paused)
{
    const DebugAddress* pause = m_map.known(QStringLiteral("pause"));
    if (!m_memory || !pause || !m_game)
        return false;
    const std::uint64_t at = pause->absolute ? *pause->value : m_game + *pause->value;
    const bool written = pause->type == DebugAddress::Type::Byte ? m_memory->writeByte(at, paused ? 1 : 0)
                                                                 : m_memory->writeInt(at, paused ? 1 : 0);
    if (written)
        m_globals.insert(pause->name, m_memory->bytes(at, std::size_t(pause->size())));
    return written;
}

bool ExternalDebugger::stepFrame(int timeoutMs)
{
    const DebugAddress* time = m_map.known(QStringLiteral("game_time"));
    if (!m_memory || !time || !m_game)
        return false;
    auto gameTime = [&] {
        return m_memory->readInt(m_game + *time->value);
    };
    const std::optional<std::int32_t> start = gameTime();
    if (!start || !setPaused(false))
        return false;
    QElapsedTimer clock;
    clock.start();
    auto waiting = [&] {
        QThread::yieldCurrentThread();
        return clock.elapsed() < timeoutMs;
    };
    if (const DebugAddress* copy = m_map.known(QStringLiteral("frame_paused"))) {
        // The engine copies the pause flag once per pass of its main loop and runs that pass's frame when the
        // copy is clear, so raising the flag right after the copy stops the next pass.
        while (m_memory->bytes(m_game + *copy->value, 1) != QByteArray(1, '\0') && waiting()) {}
    } else {
        // Without the copy the flag goes up after the first frame, and one more may run.
        while (gameTime() == start && waiting()) {}
    }
    setPaused(true);
    while (gameTime() == start && waiting()) {}
    return gameTime() != start;
}

std::optional<ExternalDebugger::Location> ExternalDebugger::currentLocation() const
{
    std::optional<Location> first;
    for (const Player* p : projectPlayers()) {
        const std::optional<std::int32_t> state = intField(*p, QStringLiteral("player_state_no"));
        if (!state)
            continue;
        Location here{intField(*p, QStringLiteral("player_id")).value_or(0), label(*p), StatePlace{*state, {}, 0}};
        if (m_states) {
            if (const std::optional<StatePlace> place = m_states(*state))
                here.place = *place;
        }
        if (!here.place.file.isEmpty())
            return here;
        if (!first)
            first = here;
    }
    return first;
}

QString ExternalDebugger::formatField(const Player* player, const QString& name, int index) const
{
    const DebugAddress* a = m_map.known(name);
    if (!a)
        return notAvailable();
    QByteArray bytes;
    qsizetype offset = qsizetype(index) * a->elementSize();
    if (player) {
        bytes = player->bytes;
        offset += qsizetype(*a->value);
    } else {
        bytes = m_globals.value(name);
    }
    if (a->type == DebugAddress::Type::String) {
        const QByteArray raw = bytes.mid(offset, a->size());
        if (raw.isEmpty())
            return notAvailable();
        return QString::fromLatin1(raw.left(raw.indexOf('\0') < 0 ? raw.size() : raw.indexOf('\0')));
    }
    if (a->type == DebugAddress::Type::Float) {
        const std::optional<float> f = floatAt(*a, bytes, offset);
        return f ? QString::number(*f) : notAvailable();
    }
    const std::optional<std::int32_t> i = intAt(*a, bytes, offset);
    if (!i)
        return notAvailable();
    if (a->letters.contains(*i))
        return a->letters.value(*i);
    if (name == QLatin1String("player_palette_no"))
        return QString::number(*i + 1);
    if (name == QLatin1String("player_root")) {
        const Player* root = playerAt(std::uint32_t(*i));
        return root ? QString::number(intField(*root, QStringLiteral("player_id")).value_or(0)) : QStringLiteral("0");
    }
    return QString::number(*i);
}

Debug::Variable ExternalDebugger::playerVariables(const Player& player) const
{
    const std::optional<std::int32_t> id = intField(player, QStringLiteral("player_id"));
    const QString path = id ? QString::number(*id) : QStringLiteral("slot%1").arg(player.slot);
    Debug::Variable v{path, label(player), QString(), QString(), {}};
    for (const Row& row : kPlayerRows) {
        const QString name = QLatin1String(row.name);
        const QString field = QLatin1String(row.field);
        v.children << Debug::Variable{
            path + QLatin1Char('.') + name, name, formatField(&player, field), typeName(m_map.find(field)), {}};
    }
    for (const Row& array : kPlayerArrays) {
        const QString name = QLatin1String(array.name);
        const QString field = QLatin1String(array.field);
        const DebugAddress* a = m_map.find(field);
        Debug::Variable group{path + QLatin1Char('.') + name, name, QString(), QString(), {}};
        const int count = a ? a->count : 0;
        for (int i = 0; i < count; ++i) {
            const QString item = QStringLiteral("%1(%2)").arg(name).arg(i);
            group.children << Debug::Variable{
                path + QLatin1Char('.') + item, item, formatField(&player, field, i), typeName(a), {}};
        }
        v.children << group;
    }
    return v;
}

QList<Debug::Variable> ExternalDebugger::variables() const
{
    Debug::Variable game{QStringLiteral("game"), QStringLiteral("Game"), QString(), QString(), {}};
    for (const Row& row : kGameRows) {
        const QString name = QLatin1String(row.name);
        const QString field = QLatin1String(row.field);
        game.children << Debug::Variable{
            QStringLiteral("game.") + name, name, formatField(nullptr, field), typeName(m_map.find(field)), {}};
    }
    QList<Debug::Variable> out{game};
    for (const Player& p : m_players)
        out << playerVariables(p);
    return out;
}

std::optional<Debug::Variable> ExternalDebugger::variable(const QString& path) const
{
    const QString top = path.section(QLatin1Char('.'), 0, 0);
    QList<Debug::Variable> candidates;
    if (top == QLatin1String("game")) {
        candidates << variables().front();
    } else {
        bool number = false;
        const int id = top.toInt(&number);
        for (const Player& p : m_players) {
            const std::optional<std::int32_t> pid = intField(p, QStringLiteral("player_id"));
            if ((number && pid == std::optional<std::int32_t>(id)) ||
                (!pid && top == QStringLiteral("slot%1").arg(p.slot)))
                candidates << playerVariables(p);
        }
    }
    if (const Debug::Variable* found = findVariable(candidates, path))
        return *found;
    return std::nullopt;
}

QString ExternalDebugger::dataValue(const QString& address) const
{
    if (const std::optional<Debug::Variable> v = variable(address))
        return v->value;
    // A number is an address in the engine's memory. Read the 32-bit value there.
    bool ok = false;
    const qulonglong at = address.toULongLong(&ok, 0);
    if (!ok || !m_memory)
        return QString();
    const std::optional<std::int32_t> value = m_memory->readInt(at);
    return value ? QString::number(*value) : notAvailable();
}

QList<Debug::StackFrame> ExternalDebugger::stack() const
{
    QList<const Player*> list = projectPlayers();
    if (m_location) {
        for (qsizetype i = 0; i < list.size(); ++i) {
            if (intField(*list[i], QStringLiteral("player_id")) == std::optional<std::int32_t>(m_location->playerId)) {
                list.move(i, 0);
                break;
            }
        }
    }
    QList<Debug::StackFrame> out;
    for (const Player* p : list) {
        const std::optional<std::int32_t> state = intField(*p, QStringLiteral("player_state_no"));
        if (!state)
            continue;
        StatePlace place{*state, {}, 0};
        if (m_states) {
            if (const std::optional<StatePlace> found = m_states(*state))
                place = *found;
        }
        out << Debug::StackFrame{QStringLiteral("%1: [Statedef %2]").arg(label(*p)).arg(*state), place.file,
                                 place.line};
    }
    return out;
}

} // namespace OFS::Mugen
