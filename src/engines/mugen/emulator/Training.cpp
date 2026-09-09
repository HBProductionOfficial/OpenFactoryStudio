#include "engines/mugen/emulator/Training.h"

#include "engines/mugen/runtime/Character.h"

#include <cmath>

namespace OFS::Mugen {

using namespace Runtime;

namespace {

// The engine's distances for the dummy, in pixels between the fronts of the two.
constexpr float CloseDistance = 10;
constexpr float MediumNear = 80;
constexpr float MediumFar = 130;
constexpr float FarDistance = 260;
constexpr float EdgeRoom = 10;
constexpr int JamDelay = 11;
constexpr int RefillTime = 60;

} // namespace

void Training::reset(std::uint32_t seed)
{
    m_lifeTimer[0] = m_lifeTimer[1] = 0;
    m_powerTimer[0] = m_powerTimer[1] = 0;
    m_jamDelay = 0;
    m_seed = seed ? seed : 1;
    m_aiX = m_aiY = m_aiHold = 0;
}

int Training::random(int max)
{
    m_seed = m_seed * 1103515245u + 12345u;
    return max > 0 ? int((m_seed >> 16) % std::uint32_t(max)) : 0;
}

InputState Training::dummyInput(const Match& match, const Character& dummy, const InputState& keys)
{
    if (m_options.control == TrainingOptions::Control::Manual)
        return keys;
    const bool facingRight = dummy.facing() > 0;
    InputState in;
    bool& forward = facingRight ? in.right : in.left;
    bool& back = facingRight ? in.left : in.right;

    if (m_options.control == TrainingOptions::Control::AI) {
        if (match.roundState() != 2)
            return in;
        if (--m_aiHold <= 0) {
            m_aiHold = 4 + random(16);
            m_aiX = random(3) - 1;
            m_aiY = random(5) == 0 ? -1 : random(4) == 0 ? 1 : 0;
        }
        forward = m_aiX > 0;
        back = m_aiX < 0;
        in.up = m_aiY < 0;
        in.down = m_aiY > 0;
        if (random(100) < m_options.aiLevel * 3) {
            bool* buttons[] = {&in.a, &in.b, &in.c, &in.x, &in.y, &in.z};
            *buttons[random(6)] = true;
        }
        return in;
    }

    if (match.roundState() != 2)
        return keys;
    const Character* enemy = match.enemyNear(&dummy);
    int direction = 0;
    if (enemy && m_options.distance != TrainingOptions::Distance::Any) {
        const float dummyFront = dummy.constant(QStringLiteral("size.ground.front"), 16);
        const float dummyBack = dummy.constant(QStringLiteral("size.ground.back"), 15);
        const float enemyFront = enemy->constant(QStringLiteral("size.ground.front"), 16);
        const float between = float(std::abs(enemy->pos().x() - dummy.pos().x())) - dummyFront - enemyFront;
        const float left = match.cameraX() - match.screenWidth() / 2;
        const float right = match.cameraX() + match.screenWidth() / 2;
        const float x = float(dummy.pos().x());
        const float room = facingRight ? x - dummyBack - left : right - x - dummyBack;
        switch (m_options.distance) {
        case TrainingOptions::Distance::Close:
            direction = between > CloseDistance ? 1 : 0;
            break;
        case TrainingOptions::Distance::Medium:
            direction = between > MediumFar ? 1 : between < MediumNear && room > EdgeRoom ? -1 : 0;
            break;
        case TrainingOptions::Distance::Far:
            direction = between < FarDistance && room > EdgeRoom ? -1 : 0;
            break;
        case TrainingOptions::Distance::Any:
            break;
        }
    }
    if (direction > 0) {
        forward = true;
    } else if (direction < 0) {
        back = true;
    } else {
        switch (m_options.mode) {
        case TrainingOptions::Mode::Crouch:
            in.down = true;
            break;
        case TrainingOptions::Mode::Jump:
            in.up = true;
            break;
        case TrainingOptions::Mode::WJump:
            // Press up again each time it starts falling, for the next air jump.
            in.up = dummy.vel().y() >= 0;
            break;
        case TrainingOptions::Mode::Stand:
            break;
        }
    }
    if (m_options.buttonJam > 0) {
        if (m_jamDelay > 0) {
            --m_jamDelay;
        } else {
            m_jamDelay = JamDelay;
            bool* buttons[] = {&in.a, &in.b, &in.c, &in.x, &in.y, &in.z, &in.s};
            *buttons[std::clamp(m_options.buttonJam, 1, 7) - 1] = true;
        }
    }
    if (m_options.guard == TrainingOptions::Guard::Auto) {
        if (const Character* attacker = match.attackerNear(&dummy)) {
            back = true;
            forward = false;
            in.up = false;
            const HitDef& hd = attacker->hitDef();
            if (hd.active && hd.guardFlag.contains(QLatin1Char('L'), Qt::CaseInsensitive) &&
                !hd.guardFlag.contains(QLatin1Char('H'), Qt::CaseInsensitive))
                in.down = true;
            else if (hd.active && !hd.guardFlag.contains(QLatin1Char('L'), Qt::CaseInsensitive))
                in.down = false;
        }
    }
    if (keys.left || keys.right || keys.up || keys.down) {
        in.left = keys.left;
        in.right = keys.right;
        in.up = keys.up;
        in.down = keys.down;
    }
    in.a = in.a || keys.a;
    in.b = in.b || keys.b;
    in.c = in.c || keys.c;
    in.x = in.x || keys.x;
    in.y = in.y || keys.y;
    in.z = in.z || keys.z;
    in.s = in.s || keys.s;
    return in;
}

void Training::refill(const Match& match, Character& player, int side)
{
    if (side < 0 || side > 1)
        return;
    const int lifeMax = int(player.constant(QStringLiteral("data.life"), 1000));
    const int powerMax = int(player.constant(QStringLiteral("data.power"), 3000));
    if (match.roundState() == 0) {
        player.setPower(powerMax);
        m_lifeTimer[side] = m_powerTimer[side] = 0;
        return;
    }
    m_lifeTimer[side] = player.moveType() == QLatin1Char('H') ? 0 : m_lifeTimer[side] + 1;
    if (m_lifeTimer[side] >= RefillTime) {
        player.setLife(lifeMax);
        m_lifeTimer[side] = 0;
    }
    m_powerTimer[side] = player.ctrl() ? m_powerTimer[side] + 1 : 0;
    if (m_powerTimer[side] >= RefillTime) {
        player.setPower(powerMax);
        m_powerTimer[side] = 0;
    }
}

} // namespace OFS::Mugen
