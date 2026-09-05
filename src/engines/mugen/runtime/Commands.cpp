#include "engines/mugen/runtime/Commands.h"

#include "engines/mugen/runtime/IniText.h"

#include <algorithm>
#include <cstdlib>
#include <initializer_list>

namespace OFS::Mugen::Runtime {

namespace {

bool isDirection(Key k)
{
    return k <= Key::N;
}

int maxOf(std::initializer_list<int> values)
{
    return *std::max_element(values.begin(), values.end());
}

int minAbs(std::initializer_list<int> values)
{
    int result = INT32_MAX;
    for (int v : values)
        result = std::min(result, std::abs(v));
    return result;
}

} // namespace

// Steps

bool CommandStep::isSingleDirection() const
{
    return keys.size() == 1 && isDirection(keys[0].key);
}

bool CommandStep::isDirToButton(const CommandStep& next) const
{
    for (const StepKey& k : next.keys) {
        if (k.slash)
            return false;
    }
    for (const StepKey& k : keys) {
        if (!isDirection(k.key))
            return false;
    }
    for (const StepKey& k : keys) {
        for (const StepKey& n : next.keys) {
            if (k.key == n.key)
                return false;
        }
    }
    for (const StepKey& n : next.keys) {
        if (!isDirection(n.key) && !n.tilde)
            return true;
    }
    for (const StepKey& k : keys) {
        if (k.tilde) {
            for (const StepKey& n : next.keys) {
                if (!(isDirection(n.key) && n.tilde))
                    return true;
            }
        }
    }
    return false;
}

// Input buffer

void InputBuffer::reset()
{
    m_now.fill(0);
    m_previous.fill(0);
}

void InputBuffer::update(const InputState& input, int facing)
{
    m_previous = m_now;
    const bool back = facing >= 0 ? input.left : input.right;
    const bool forward = facing >= 0 ? input.right : input.left;
    const bool held[Count] = {
        input.up, input.down, back,    forward, !(input.up || input.down || input.left || input.right),
        input.a,  input.b,    input.c, input.x, input.y,
        input.z,  input.s};
    for (int i = 0; i < Count; ++i) {
        int& t = m_now[std::size_t(i)];
        if (held[i] != (t > 0))
            t = held[i] ? 1 : -1;
        else
            t += held[i] ? 1 : -1;
    }
}

int InputBuffer::state(const StepKey& k) const
{
    const int u = m_now[U], d = m_now[D], b = m_now[B], f = m_now[F];
    const int up = m_previous[U], dp = m_previous[D], bp = m_previous[B], fp = m_previous[F];
    // Held, without the other directions.
    if (!k.tilde && !k.dollar) {
        switch (k.key) {
        case Key::U:
            return std::min(-maxOf({b, d, f}), u);
        case Key::D:
            return std::min(-maxOf({b, u, f}), d);
        case Key::B:
            return std::min(-maxOf({d, u, f}), b);
        case Key::F:
            return std::min(-maxOf({d, u, b}), f);
        case Key::UF:
            return std::min(-std::max(d, b), std::min(u, f));
        case Key::UB:
            return std::min(-std::max(d, f), std::min(u, b));
        case Key::DF:
            return std::min(-std::max(u, b), std::min(d, f));
        case Key::DB:
            return std::min(-std::max(u, f), std::min(d, b));
        case Key::N:
            return m_now[N];
        default:
            break;
        }
    }
    // "$": the direction, whatever else is held.
    if (!k.tilde && k.dollar) {
        const int any = minAbs({u, d, b, f});
        switch (k.key) {
        case Key::U:
            return u > 0 ? any : 0;
        case Key::D:
            return d > 0 ? any : 0;
        case Key::B:
            return b > 0 ? any : 0;
        case Key::F:
            return f > 0 ? any : 0;
        case Key::UB:
            return u > 0 && b > 0 ? any : 0;
        case Key::UF:
            return u > 0 && f > 0 ? any : 0;
        case Key::DB:
            return d > 0 && b > 0 ? any : 0;
        case Key::DF:
            return d > 0 && f > 0 ? any : 0;
        case Key::N:
            return minAbs({u, d, b, f, m_now[A], m_now[Bt], m_now[C], m_now[X], m_now[Y], m_now[Z], m_now[S]});
        default:
            break;
        }
    }
    // "~": released (or held the tick before), without the other directions.
    if (k.tilde && !k.dollar) {
        auto released = [](int now, int before) {
            return now < 0 || before > 0;
        };
        switch (k.key) {
        case Key::U:
            return released(u, up) ? -std::min(-maxOf({b, d, f}), u) : 0;
        case Key::D:
            return released(d, dp) ? -std::min(-maxOf({b, u, f}), d) : 0;
        case Key::B:
            return released(b, bp) ? -std::min(-maxOf({d, u, f}), b) : 0;
        case Key::F:
            return released(f, fp) ? -std::min(-maxOf({d, u, b}), f) : 0;
        case Key::UF:
            return released(u, up) && released(f, fp) ? -std::min(-std::max(d, b), std::min(u, f)) : 0;
        case Key::UB:
            return released(u, up) && released(b, bp) ? -std::min(-std::max(d, f), std::min(u, b)) : 0;
        case Key::DF:
            return released(d, dp) && released(f, fp) ? -std::min(-std::max(u, b), std::min(d, f)) : 0;
        case Key::DB:
            return released(d, dp) && released(b, bp) ? -std::min(-std::max(u, f), std::min(d, b)) : 0;
        case Key::N:
            return -m_now[N];
        default:
            break;
        }
    }
    if (k.tilde && k.dollar) {
        auto release = [](int now, int before, std::initializer_list<int> others) {
            if (!(now < 0 || before > 0))
                return 0;
            return now < 0 ? -now : minAbs(others);
        };
        switch (k.key) {
        case Key::U:
            return release(u, up, {d, b, f});
        case Key::D:
            return release(d, dp, {u, b, f});
        case Key::B:
            return release(b, bp, {u, d, f});
        case Key::F:
            return release(f, fp, {u, d, b});
        case Key::UB:
            if ((u < 0 || up > 0) && (b < 0 || bp > 0))
                return u < 0 || b < 0 ? -std::min(u, b) : minAbs({d, f});
            return 0;
        case Key::UF:
            if ((u < 0 || up > 0) && (f < 0 || fp > 0))
                return u < 0 || f < 0 ? -std::min(u, f) : minAbs({d, b});
            return 0;
        case Key::DB:
            if ((d < 0 || dp > 0) && (b < 0 || bp > 0))
                return d < 0 || b < 0 ? -std::min(d, b) : minAbs({u, f});
            return 0;
        case Key::DF:
            if ((d < 0 || dp > 0) && (f < 0 || fp > 0))
                return d < 0 || f < 0 ? -std::min(d, f) : minAbs({u, b});
            return 0;
        case Key::N:
            return minAbs({u, d, b, f, m_now[A], m_now[Bt], m_now[C], m_now[X], m_now[Y], m_now[Z], m_now[S]});
        default:
            break;
        }
    }
    static const Raw buttons[] = {A, Bt, C, X, Y, Z, S};
    if (!isDirection(k.key)) {
        const Raw raw = buttons[int(k.key) - int(Key::a)];
        const int now = m_now[raw];
        if (!k.tilde)
            return now;
        return now < 0 || m_previous[raw] > 0 ? -now : 0;
    }
    return 0;
}

int InputBuffer::charge(const StepKey& k) const
{
    auto raw = [](Key key) -> int {
        switch (key) {
        case Key::U:
        case Key::UB:
        case Key::UF:
            return U;
        case Key::D:
        case Key::DB:
        case Key::DF:
            return D;
        case Key::B:
            return B;
        case Key::F:
            return F;
        case Key::N:
            return N;
        default:
            return A + (int(key) - int(Key::a));
        }
    };
    const std::size_t i = std::size_t(raw(k.key));
    return k.tilde ? m_previous[i] : m_now[i];
}

// Commands

Command Command::parse(const QString& name, const QString& text, int time, int bufferTime,
                       const std::array<Key, 7>& remap, QString* error)
{
    Command command;
    command.m_name = name;
    command.m_maxTime = std::max(1, time);
    command.m_maxBufferTime = std::max(1, bufferTime);
    auto fail = [error](const QString& message) {
        if (error && error->isEmpty())
            *error = message;
    };
    const QStringList steps = text.split(QLatin1Char(','));
    for (int s = 0; s < steps.size(); ++s) {
        const QString stepText = steps[s].trimmed();
        CommandStep step;
        if (stepText.isEmpty()) {
            fail(QStringLiteral("empty command step"));
            continue;
        }
        for (QString part : stepText.split(QLatin1Char('+'))) {
            part = part.trimmed().remove(QLatin1Char(' '));
            StepKey key;
            int i = 0;
            auto number = [&part, &i] {
                int n = 0;
                while (i < part.size() && part[i].isDigit())
                    n = n * 10 + part[i++].digitValue();
                return n;
            };
            for (bool more = true; more && i < part.size();) {
                const QChar c = part[i];
                if (c == QLatin1Char('>')) {
                    step.greater = true;
                    ++i;
                } else if (c == QLatin1Char('~')) {
                    key.tilde = true;
                    ++i;
                    if (const int n = number(); n > 0)
                        key.chargeTime = n;
                } else if (c == QLatin1Char('/')) {
                    key.slash = true;
                    ++i;
                    if (const int n = number(); n > 0)
                        key.chargeTime = n;
                } else if (c == QLatin1Char('$')) {
                    key.dollar = true;
                    ++i;
                } else {
                    more = false;
                }
            }
            const QString symbol = part.mid(i);
            static const QList<QPair<QString, Key>> directions = {
                {QStringLiteral("UB"), Key::UB}, {QStringLiteral("UF"), Key::UF}, {QStringLiteral("DB"), Key::DB},
                {QStringLiteral("DF"), Key::DF}, {QStringLiteral("U"), Key::U},   {QStringLiteral("D"), Key::D},
                {QStringLiteral("B"), Key::B},   {QStringLiteral("F"), Key::F},   {QStringLiteral("N"), Key::N}};
            bool found = false;
            for (const auto& [spelling, k] : directions) {
                if (symbol == spelling) {
                    key.key = k;
                    found = true;
                    break;
                }
            }
            if (!found && symbol.size() == 1) {
                static const QString buttons = QStringLiteral("abcxyzs");
                const int index = int(buttons.indexOf(symbol[0]));
                if (index >= 0) {
                    key.key = remap[std::size_t(index)];
                    key.dollar = false;
                    found = true;
                }
            }
            if (!found) {
                fail(QStringLiteral("invalid symbol '%1' in command \"%2\"").arg(symbol, name));
                continue;
            }
            step.keys.push_back(key);
        }
        // "/" covers every key of its step, as the engine reads it.
        bool slash = false;
        for (const StepKey& k : step.keys)
            slash = slash || k.slash;
        if (slash) {
            for (StepKey& k : step.keys)
                k.slash = true;
        }
        command.m_steps.push_back(step);
    }
    command.m_completed.assign(command.m_steps.size(), false);
    command.m_stepTimers.assign(command.m_steps.size(), 0);
    // Steps are checked from the last to the first, so one input cannot complete two of them.
    // Direction-then-button runs are checked forwards, so they can complete in the same tick.
    for (int i = int(command.m_steps.size()) - 1; i >= 0;) {
        if (i > 0 && command.m_steps[std::size_t(i - 1)].isDirToButton(command.m_steps[std::size_t(i)])) {
            int start = i - 1;
            while (start > 0 &&
                   command.m_steps[std::size_t(start - 1)].isDirToButton(command.m_steps[std::size_t(start)]))
                --start;
            for (int j = start; j <= i; ++j)
                command.m_order.push_back(std::size_t(j));
            i = start - 1;
        } else {
            command.m_order.push_back(std::size_t(i));
            --i;
        }
    }
    return command;
}

void Command::clear()
{
    m_time = 0;
    std::fill(m_completed.begin(), m_completed.end(), false);
    std::fill(m_stepTimers.begin(), m_stepTimers.end(), 0);
}

bool Command::greaterCheckFails(std::size_t i, const InputBuffer& input) const
{
    // Any other key pressed or released this tick breaks a ">" step.
    auto unexpected = [this, i, &input](Key k) {
        for (bool tilde : {false, true}) {
            if (input.state(StepKey{k, false, tilde, false, 0}) != 1)
                continue;
            bool allowed = false;
            for (const StepKey& sk : m_steps[i].keys)
                allowed = allowed || (sk.key == k && sk.tilde == tilde);
            return !allowed;
        }
        return false;
    };
    for (Key k : {Key::U, Key::D, Key::B, Key::F, Key::UF, Key::UB, Key::DF, Key::DB, Key::a, Key::b, Key::c, Key::x,
                  Key::y, Key::z, Key::s}) {
        if (unexpected(k))
            return true;
    }
    return false;
}

void Command::step(const InputBuffer& input)
{
    if (m_bufferTime > 0)
        --m_bufferTime;
    if (m_steps.empty())
        return;
    bool any = false;
    for (std::size_t i = 0; i < m_steps.size(); ++i) {
        if (m_completed[i]) {
            ++m_stepTimers[i];
            any = true;
        }
    }
    if (any)
        ++m_time;
    else if (m_time > 0)
        clear();

    for (std::size_t i : m_order) {
        if (i > 0 && !m_completed[i - 1])
            continue;
        bool matched = true;
        for (const StepKey& k : m_steps[i].keys) {
            const int t = input.state(k);
            bool ok = k.slash ? t > 0 : t == 1;
            if (ok && k.chargeTime > 1 && input.charge(k) < k.chargeTime)
                ok = false;
            if (!ok) {
                matched = false;
                break;
            }
        }
        if (m_steps[i].greater && i > 0 && m_completed[i - 1] && !m_completed[i] && greaterCheckFails(i, input)) {
            matched = false;
            m_completed[i - 1] = false;
            m_stepTimers[i - 1] = 0;
        }
        if (matched) {
            m_completed[i] = true;
            m_stepTimers[i] = 0;
            if (i > 0) {
                m_completed[i - 1] = false;
                m_stepTimers[i - 1] = 0;
            }
            if (i == 0)
                m_time = 0;
        }
    }

    const bool complete = m_completed.back();
    if (!complete && m_time < m_maxTime)
        return;
    clear();
    if (complete)
        m_bufferTime = std::max(m_bufferTime, m_maxBufferTime);
}

// Command list

void CommandList::load(const QString& text, QStringList* errors)
{
    const IniText ini = IniText::parse(text);
    // Buttons remapped ([Remap] x = y: x acts as y).
    std::array<Key, 7> remap = {Key::a, Key::b, Key::c, Key::x, Key::y, Key::z, Key::s};
    if (const IniSection* section = ini.section(QStringLiteral("Remap"))) {
        static const QString buttons = QStringLiteral("abcxyzs");
        for (int i = 0; i < 7; ++i) {
            const QString to = section->value(QString(buttons[i])).trimmed().toLower();
            const int index = to.size() == 1 ? int(buttons.indexOf(to[0])) : -1;
            if (index >= 0)
                remap[std::size_t(i)] = Key(int(Key::a) + index);
        }
    }
    int defaultTime = 15;
    int defaultBuffer = 1;
    if (const IniSection* section = ini.section(QStringLiteral("Defaults"))) {
        defaultTime = section->value(QStringLiteral("command.time"), QStringLiteral("15")).toInt();
        defaultBuffer =
            std::clamp(section->value(QStringLiteral("command.buffer.time"), QStringLiteral("1")).toInt(), 1, 30);
    }
    for (const IniSection& section : ini.sections()) {
        if (section.name.compare(QLatin1String("Command"), Qt::CaseInsensitive) != 0)
            continue;
        const QString name = unquote(section.value(QStringLiteral("name")));
        const QString sequence = section.value(QStringLiteral("command"));
        if (name.isEmpty() || sequence.trimmed().isEmpty())
            continue;
        const int time = section.value(QStringLiteral("time"), QString::number(defaultTime)).toInt();
        const int buffer =
            std::clamp(section.value(QStringLiteral("buffer.time"), QString::number(defaultBuffer)).toInt(), 1, 30);
        QString error;
        Command command = Command::parse(name, sequence, time, buffer, remap, &error);
        if (!error.isEmpty() && errors)
            *errors << QStringLiteral("%1: %2").arg(section.line + 1).arg(error);
        m_commands.push_back(command);
    }
}

QStringList CommandList::names() const
{
    QStringList list;
    for (const Command& c : m_commands) {
        if (!list.contains(c.name()))
            list << c.name();
    }
    return list;
}

void CommandList::update(const InputState& input, int facing)
{
    m_input.update(input, facing);
    for (Command& c : m_commands)
        c.step(m_input);
}

bool CommandList::active(const QString& name) const
{
    for (const Command& c : m_commands) {
        if (c.active() && c.name() == name) // names are case-sensitive
            return true;
    }
    return false;
}

void CommandList::clear()
{
    m_input.reset();
    for (Command& c : m_commands)
        c.clear();
}

} // namespace OFS::Mugen::Runtime
