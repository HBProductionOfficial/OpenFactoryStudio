#include "engines/mugen/emulator/EmulatorDebugger.h"

#include "engines/mugen/emulator/EmulatorSession.h"
#include "engines/mugen/runtime/Character.h"

#include <QDir>

namespace OFS::Mugen {

using namespace Runtime;

namespace {

bool sameFile(const QString& a, const QString& b)
{
#ifdef Q_OS_WIN
    return QDir::cleanPath(a).compare(QDir::cleanPath(b), Qt::CaseInsensitive) == 0;
#else
    return QDir::cleanPath(a) == QDir::cleanPath(b);
#endif
}

Debug::Variable value(const QString& path, const QString& name, const QString& text, const QString& type)
{
    return Debug::Variable{path, name, text, type, {}};
}

QString letter(QChar c)
{
    return c.isNull() ? QString() : QString(c);
}

QString nameOf(const EmulatorSession& session, const Character& c)
{
    if (c.isHelper())
        return QStringLiteral("Helper %1 (%2)").arg(c.helperId()).arg(c.id());
    const auto& players = session.players();
    for (std::size_t i = 0; i < players.size(); ++i) {
        if (players[i].character.get() == &c)
            return QStringLiteral("P%1: %2 (%3)").arg(i + 1).arg(players[i].name).arg(c.id());
    }
    return QStringLiteral("%1").arg(c.id());
}

Debug::Variable characterVariables(const EmulatorSession& session, const Character& c)
{
    const QString id = QString::number(c.id());
    Debug::Variable v{id, nameOf(session, c), QString(), QString(), {}};
    auto add = [&](const QString& field, const QString& text, const char* type) {
        v.children << value(id + QLatin1Char('.') + field, field, text, QLatin1String(type));
    };
    add(QStringLiteral("life"), QString::number(c.life()), "int");
    add(QStringLiteral("power"), QString::number(c.power()), "int");
    add(QStringLiteral("pos x"), QString::number(c.pos().x()), "float");
    add(QStringLiteral("pos y"), QString::number(c.pos().y()), "float");
    add(QStringLiteral("vel x"), QString::number(c.vel().x()), "float");
    add(QStringLiteral("vel y"), QString::number(c.vel().y()), "float");
    add(QStringLiteral("facing"), QString::number(c.facing()), "int");
    add(QStringLiteral("stateno"), QString::number(c.stateNo()), "int");
    add(QStringLiteral("prevstateno"), QString::number(c.prevStateNo()), "int");
    add(QStringLiteral("statetype"), letter(c.stateType()), "char");
    add(QStringLiteral("movetype"), letter(c.moveType()), "char");
    add(QStringLiteral("physics"), letter(c.physics()), "char");
    add(QStringLiteral("time"), QString::number(c.time()), "int");
    add(QStringLiteral("ctrl"), QString::number(c.ctrl() ? 1 : 0), "int");
    add(QStringLiteral("anim"), QString::number(c.animNo()), "int");
    add(QStringLiteral("alive"), QString::number(c.alive() ? 1 : 0), "int");
    add(QStringLiteral("sprpriority"), QString::number(c.sprPriority()), "int");
    auto array = [&](const QString& name, int count, const char* type, auto read) {
        Debug::Variable group{id + QLatin1Char('.') + name, name, QString(), QString(), {}};
        for (int i = 0; i < count; ++i) {
            const QString item = QStringLiteral("%1(%2)").arg(name).arg(i);
            group.children << value(id + QLatin1Char('.') + item, item, read(i), QLatin1String(type));
        }
        v.children << group;
    };
    array(QStringLiteral("var"), 60, "int", [&c](int i) { return QString::number(c.var(i)); });
    array(QStringLiteral("fvar"), 40, "float", [&c](int i) { return QString::number(c.fvar(i)); });
    array(QStringLiteral("sysvar"), 5, "int", [&c](int i) { return QString::number(c.sysVar(i)); });
    array(QStringLiteral("sysfvar"), 5, "float", [&c](int i) { return QString::number(c.sysFVar(i)); });
    return v;
}

const Debug::Variable* find(const QList<Debug::Variable>& list, const QString& path)
{
    for (const Debug::Variable& v : list) {
        if (v.path == path)
            return &v;
        if (path.startsWith(v.path)) {
            if (const Debug::Variable* found = find(v.children, path))
                return found;
        }
    }
    return nullptr;
}

} // namespace

void EmulatorDebugger::reset()
{
    m_step = Step::None;
    m_location.reset();
    m_from = Location();
    m_interrupt = false;
    m_hits.clear();
    m_dataValues.clear();
}

void EmulatorDebugger::resume(Step step)
{
    m_from = m_location.value_or(Location());
    // Stopped between frames, Over and Out go to the first line of the next frame.
    if (!m_location && (step == Step::Over || step == Step::Out))
        step = Step::Into;
    m_step = step;
    m_location.reset();
    m_frames = 0;
}

bool EmulatorDebugger::hit(const Debug::Breakpoint& breakpoint, const Character* character)
{
    if (!breakpoint.condition.trimmed().isEmpty() && character) {
        QString error;
        const Expr condition = Expr::compile(breakpoint.condition, &error);
        if (!error.isEmpty() || !condition.evaluate(const_cast<Character&>(*character)).toBool())
            return false;
    }
    const int times = ++m_hits[breakpoint.number];
    if (times <= breakpoint.ignoreCount)
        return false;
    if (breakpoint.oneShot && m_breakpoints)
        m_breakpoints->setEnabled(breakpoint.number, false);
    return true;
}

bool EmulatorDebugger::dataChanged(const EmulatorSession& session)
{
    if (!m_breakpoints)
        return false;
    bool changed = false;
    const QList<Debug::Breakpoint> list = m_breakpoints->breakpoints();
    for (const Debug::Breakpoint& b : list) {
        if (!b.enabled || b.type != Debug::BreakpointType::Memory)
            continue;
        const std::optional<Debug::Variable> v = variable(session, b.address.trimmed());
        const QString now = v ? v->value : QString();
        const bool known = m_dataValues.contains(b.address);
        const QString before = m_dataValues.value(b.address);
        m_dataValues.insert(b.address, now);
        // It stops after the value changes. The first look only notes the value.
        if (known && before != now && !changed) {
            const Character* who =
                session.isStarted() && !session.players().empty() ? session.players().front().character.get() : nullptr;
            changed = hit(b, who);
        }
    }
    return changed;
}

bool EmulatorDebugger::atLine(const EmulatorSession& session, const Character& character, const Controller& controller,
                              int line)
{
    Location here;
    here.characterId = character.id();
    here.characterName = nameOf(session, character);
    here.stateNo = character.runningState();
    here.label = controller.label;
    here.file = controller.file;
    here.line = line + 1;
    bool stop = m_interrupt;
    switch (m_step) {
    case Step::Into:
        stop = true;
        break;
    case Step::Over:
        stop = stop || here.characterId == m_from.characterId;
        break;
    case Step::Out:
        stop = stop || here.characterId != m_from.characterId || here.stateNo != m_from.stateNo;
        break;
    case Step::Frame:
    case Step::None:
        break;
    }
    if (!stop && m_breakpoints) {
        const QList<Debug::Breakpoint> list = m_breakpoints->breakpoints();
        for (const Debug::Breakpoint& b : list) {
            if (b.enabled && b.type == Debug::BreakpointType::FileLine && b.line == here.line &&
                sameFile(b.file, here.file) && hit(b, &character)) {
                stop = true;
                break;
            }
        }
    }
    if (!stop)
        stop = dataChanged(session);
    if (stop) {
        m_location = here;
        m_interrupt = false;
        m_step = Step::None;
    }
    return stop;
}

bool EmulatorDebugger::beforeFrame(const EmulatorSession& session)
{
    bool stop = m_interrupt || (m_step == Step::Frame && m_frames > 0);
    if (!stop && m_breakpoints) {
        const Character* p1 =
            session.isStarted() && !session.players().empty() ? session.players().front().character.get() : nullptr;
        const QList<Debug::Breakpoint> list = m_breakpoints->breakpoints();
        for (const Debug::Breakpoint& b : list) {
            if (b.enabled && b.type == Debug::BreakpointType::Frame && hit(b, p1)) {
                stop = true;
                break;
            }
        }
    }
    if (!stop)
        stop = dataChanged(session);
    if (stop) {
        m_location.reset();
        m_interrupt = false;
        m_step = Step::None;
    }
    return stop;
}

QList<Debug::Variable> EmulatorDebugger::variables(const EmulatorSession& session)
{
    QList<Debug::Variable> out;
    if (!session.isStarted())
        return out;
    const Match& match = session.match();
    Debug::Variable game{QStringLiteral("game"), QStringLiteral("Game"), QString(), QString(), {}};
    auto add = [&game](const QString& field, const QString& text, const char* type) {
        game.children << value(QStringLiteral("game.") + field, field, text, QLatin1String(type));
    };
    add(QStringLiteral("time"), QString::number(match.gameTime()), "int");
    add(QStringLiteral("roundstate"), QString::number(match.roundState()), "int");
    add(QStringLiteral("roundno"), QString::number(match.roundNo()), "int");
    add(QStringLiteral("roundtime"), QString::number(match.roundTime()), "int");
    add(QStringLiteral("camera x"), QString::number(match.cameraX()), "float");
    add(QStringLiteral("pause"), QString::number(std::max(match.superPauseTime(), match.pauseTime())), "int");
    out << game;
    for (const Character* c : match.characters()) {
        if (!c->destroyed())
            out << characterVariables(session, *c);
    }
    return out;
}

std::optional<Debug::Variable> EmulatorDebugger::variable(const EmulatorSession& session, const QString& path)
{
    const QList<Debug::Variable> all = variables(session);
    if (const Debug::Variable* found = find(all, path))
        return *found;
    return std::nullopt;
}

QList<Debug::StackFrame> EmulatorDebugger::stack(const EmulatorSession& session) const
{
    QList<Debug::StackFrame> out;
    if (!m_location || !session.isStarted())
        return out;
    const Location& at = *m_location;
    const QString label = at.label.isEmpty() ? QString() : QStringLiteral(", ") + at.label;
    out << Debug::StackFrame{QStringLiteral("%1: [State %2%3]").arg(at.characterName).arg(at.stateNo).arg(label),
                             at.file, at.line};
    if (const Character* c = session.match().playerById(at.characterId)) {
        if (const StateDef* def = c->runningStateData().state(at.stateNo))
            out << Debug::StackFrame{QStringLiteral("%1: [Statedef %2]").arg(at.characterName).arg(at.stateNo),
                                     def->file, def->line + 1};
    }
    return out;
}

} // namespace OFS::Mugen
