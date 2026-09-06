#include "engines/mugen/runtime/Character.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace OFS::Mugen::Runtime {

Match::Match(std::uint32_t seed)
    : m_seed(seed ? seed : 1)
{}

void Match::add(Character* character)
{
    if (character && !m_characters.contains(character))
        m_characters << character;
}

Character* Match::enemy(const Character* of, int index) const
{
    int n = 0;
    for (Character* c : m_characters) {
        if (c == of)
            continue;
        if (n++ == index)
            return c;
    }
    return nullptr;
}

int Match::random(int max)
{
    // xorshift32: a fixed sequence for a seed, so runs can be replayed.
    m_seed ^= m_seed << 13;
    m_seed ^= m_seed >> 17;
    m_seed ^= m_seed << 5;
    return max > 0 ? int(m_seed % std::uint32_t(max)) : 0;
}

bool Match::isAsserted(const QString& flag) const
{
    return std::any_of(m_characters.cbegin(), m_characters.cend(),
                       [&flag](const Character* c) { return c->isAsserted(flag); });
}

void Match::tick()
{
    m_sounds.clear();
    m_spawns.clear();
    for (Character* c : std::as_const(m_characters)) {
        bool near = false;
        for (const Character* other : std::as_const(m_characters)) {
            if (other == c || other->moveType() != QLatin1Char('A'))
                continue;
            const int guardDist = other->hitDef().active ? other->hitDef().guardDist : -1;
            const float reach =
                guardDist >= 0 ? float(guardDist) : other->data().constant(QStringLiteral("size.attack.dist"), 160);
            near = near || std::abs(other->pos().x() - c->pos().x()) <= reach;
        }
        c->setInGuardDist(near);
    }
    for (Character* c : std::as_const(m_characters))
        c->runActions();
    push();
    keepOnScreen();
    for (Character* c : std::as_const(m_characters))
        c->updateBindings();
    for (Character* attacker : std::as_const(m_characters)) {
        for (Character* target : std::as_const(m_characters)) {
            if (attacker != target)
                attacker->attack(*target);
        }
    }
    for (Character* c : std::as_const(m_characters))
        c->tickBinding();
    for (Character* c : std::as_const(m_characters))
        c->finishTick();
    for (Character* c : std::as_const(m_characters))
        c->releaseTargets();
    if (m_envShake.time > 0)
        --m_envShake.time;
    if (m_envColor.time > 0)
        --m_envColor.time;
    m_allPalFx.tick();
    m_bgPalFx.tick();
    ++m_time;
}

void Match::push()
{
    for (int i = 0; i < m_characters.size(); ++i) {
        for (int j = i + 1; j < m_characters.size(); ++j) {
            Character* a = m_characters[i];
            Character* b = m_characters[j];
            if (!a->pushes() || !b->pushes())
                continue;
            const float aHeight = a->data().constant(QStringLiteral("size.height"), 60);
            const float bHeight = b->data().constant(QStringLiteral("size.height"), 60);
            if (a->pos().y() - aHeight >= b->pos().y() || b->pos().y() - bHeight >= a->pos().y())
                continue;
            auto span = [](const Character* c) {
                const float x = float(c->pos().x());
                return c->facing() > 0 ? std::pair(x - c->backWidth(), x + c->frontWidth())
                                       : std::pair(x - c->frontWidth(), x + c->backWidth());
            };
            const auto [aLeft, aRight] = span(a);
            const auto [bLeft, bRight] = span(b);
            const float overlap = std::min(aRight, bRight) - std::max(aLeft, bLeft);
            if (overlap <= 0)
                continue;
            const bool aOnLeft = a->pos().x() < b->pos().x() || (a->pos().x() == b->pos().x() && a->facing() > 0);
            a->moveX(aOnLeft ? -overlap / 2 : overlap / 2);
            b->moveX(aOnLeft ? overlap / 2 : -overlap / 2);
        }
    }
}

void Match::keepOnScreen()
{
    if (m_characters.isEmpty())
        return;
    // The camera centres on the players it follows, and none bound to the screen may pass its margin.
    float least = 0;
    float most = 0;
    bool followed = false;
    for (Character* c : std::as_const(m_characters)) {
        float x = float(c->pos().x());
        if (c->screenBound()) {
            x = std::clamp(x, m_left, m_right);
            c->moveX(x - float(c->pos().x()));
        }
        if (!c->movesCamera())
            continue;
        least = followed ? std::min(least, x) : x;
        most = followed ? std::max(most, x) : x;
        followed = true;
    }
    const float half = m_screenWidth / 2;
    if (followed)
        m_cameraX = std::clamp((least + most) / 2, m_left + half, m_right - half);
    if (m_left + half > m_right - half)
        m_cameraX = (m_left + m_right) / 2;
    for (Character* c : std::as_const(m_characters)) {
        if (!c->screenBound())
            continue;
        const float x =
            std::clamp(float(c->pos().x()), m_cameraX - half + m_screenEdge, m_cameraX + half - m_screenEdge);
        c->moveX(x - float(c->pos().x()));
    }
}

} // namespace OFS::Mugen::Runtime
