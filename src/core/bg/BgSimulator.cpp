#include "core/bg/BgSimulator.h"

#include "core/anims/AirDocument.h"

#include <cmath>
#include <numbers>

namespace OFS {

namespace {

double wave(double radius, int period, int time)
{
    if (period <= 0)
        return 0;
    return radius * std::sin(2 * std::numbers::pi * time / period);
}

} // namespace

BgSimulator::BgSimulator(const BgGroup& group)
    : m_group(group)
    , m_elements(group.elements)
{
    int link = 0;
    for (int i = 0; i < m_elements.size(); ++i) {
        BgElement& e = m_elements[i];
        if (i > 0 && e.positionLink) {
            e.start += m_elements[link].start;
            e.delta = m_elements[link].delta;
            e.velocity = m_elements[link].velocity;
        } else {
            link = i;
        }
        m_links << link;
    }
    reset();
}

void BgSimulator::reset()
{
    m_tick = 0;
    m_states.clear();
    for (const BgElement& e : std::as_const(m_elements)) {
        BgState state;
        state.action = e.action;
        state.velocity = e.velocity;
        const double* sines[2] = {e.sinX, e.sinY};
        for (int k = 0; k < 2; ++k) {
            state.radius[k] = sines[k][0];
            state.period[k] = int(sines[k][1]);
            // The third value is a phase in degrees.
            if (state.period[k] > 0) {
                state.sineTime[k] = int(sines[k][2] * state.period[k] / 360) % state.period[k];
                if (state.sineTime[k] < 0)
                    state.sineTime[k] += state.period[k];
            }
        }
        m_states << state;
    }
}

void BgSimulator::runTo(int tick)
{
    if (tick < m_tick)
        reset();
    while (m_tick < tick)
        step();
}

bool BgSimulator::active(const BgController& c, int loop) const
{
    int end = c.endTime;
    if (c.startTime < 0 || (loop >= 0 && c.startTime >= loop))
        return false;
    if (loop > 0 && end > loop)
        end = loop;
    if (m_tick < c.startTime)
        return false;
    if (loop > 0)
        return (m_tick - c.startTime) % loop <= end - c.startTime;
    return m_tick <= end;
}

void BgSimulator::run(const BgController& c, const QList<int>& targets)
{
    const double value = c.value.value(0);
    for (int i : targets) {
        BgState& s = m_states[i];
        switch (c.type) {
        case BgController::Type::Null:
            break;
        case BgController::Type::Visible:
            s.visible = value != 0;
            break;
        case BgController::Type::Enable:
            s.enabled = value != 0;
            break;
        case BgController::Type::VelSet:
            if (c.x)
                s.velocity.setX(*c.x);
            if (c.y)
                s.velocity.setY(*c.y);
            break;
        case BgController::Type::VelAdd:
            if (c.x)
                s.velocity.rx() += *c.x;
            if (c.y)
                s.velocity.ry() += *c.y;
            break;
        case BgController::Type::PosSet:
            if (c.x)
                s.position.setX(*c.x);
            if (c.y)
                s.position.setY(*c.y);
            break;
        case BgController::Type::PosAdd:
            if (c.x)
                s.position.rx() += *c.x;
            if (c.y)
                s.position.ry() += *c.y;
            break;
        case BgController::Type::Anim:
            s.action = int(value);
            s.actionTicks = 0;
            break;
        case BgController::Type::SinX:
        case BgController::Type::SinY: {
            // value = amplitude, period, time offset in ticks (the element phase is in degrees).
            const int k = c.type == BgController::Type::SinY ? 1 : 0;
            const int period = value == 0 ? 0 : int(c.value.value(1));
            const int offset = int(c.value.value(2));
            s.radius[k] = value;
            s.period[k] = period;
            s.sineTime[k] = period != 0 ? ((offset % period) + std::abs(period)) % std::abs(period) : 0;
            break;
        }
        }
    }
}

void BgSimulator::step()
{
    for (const BgControllerDef& def : std::as_const(m_group.controllerDefs)) {
        for (const BgController& c : def.controllers) {
            if (!active(c, c.loopTime.value_or(def.loopTime)))
                continue;
            const std::optional<QList<int>>& ids = c.ids ? c.ids : def.ids;
            const bool all = !ids || ids->isEmpty() || (ids->size() == 1 && ids->first() == -1);
            QList<int> targets;
            for (int i = 0; i < m_elements.size(); ++i) {
                if (all || ids->contains(m_elements[i].id))
                    targets << i;
            }
            run(c, targets);
        }
    }
    for (BgState& s : m_states) {
        if (!s.enabled)
            continue;
        s.position += s.velocity;
        for (int k = 0; k < 2; ++k) {
            if (s.period[k] > 0)
                s.sineTime[k] = (s.sineTime[k] + 1) % s.period[k];
        }
        ++s.actionTicks;
    }
    ++m_tick;
}

QPointF BgSimulator::sineOffset(int element) const
{
    auto own = [this](int i) {
        const BgState& s = m_states[i];
        return QPointF(wave(s.radius[0], s.period[0], s.sineTime[0]), wave(s.radius[1], s.period[1], s.sineTime[1]));
    };
    QPointF offset = own(element);
    if (m_links[element] != element)
        offset += own(m_links[element]);
    return offset;
}

QPointF BgSimulator::position(int element) const
{
    return m_elements[element].start + m_states[element].position + sineOffset(element);
}

bool BgSimulator::shown(int element) const
{
    return m_states[element].visible && m_states[element].enabled;
}

int BgSimulator::animationFrame(const AirAnimation& animation, int ticks)
{
    const int count = int(animation.frames.size());
    if (count == 0)
        return -1;
    int elapsed = 0;
    for (int f = 0; f < count; ++f) {
        const int time = animation.frames[f].time;
        if (time < 0)
            return f; // endless
        if (ticks < elapsed + time)
            return f;
        elapsed += time;
    }
    if (elapsed <= 0)
        return 0;
    const int loopFrame = animation.loopStartFrame();
    int loopStart = 0;
    for (int f = 0; f < loopFrame; ++f)
        loopStart += std::max(0, animation.frames[f].time);
    const int loopLength = elapsed - loopStart;
    if (loopLength <= 0)
        return count - 1;
    const int t = loopStart + (ticks - elapsed) % loopLength;
    for (int f = loopFrame; f < count; ++f) {
        const int time = std::max(0, animation.frames[f].time);
        if (t < loopStart + time)
            return f;
        loopStart += time;
    }
    return count - 1;
}

} // namespace OFS
