#include "engines/mugen/runtime/Character.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace OFS::Mugen::Runtime {

namespace {

constexpr int MaxHelpers = 56; // per player, as the engine allows

} // namespace

Match::Match(std::uint32_t seed)
    : m_seed(seed ? seed : 1)
{}

Match::~Match() = default;

void Match::add(Character* character)
{
    if (!character || m_characters.contains(character))
        return;
    const auto players =
        std::count_if(m_characters.cbegin(), m_characters.cend(), [](const Character* c) { return !c->isHelper(); });
    character->m_team = int(players % 2);
    m_characters << character;
}

Character* Match::enemy(const Character* of, int index) const
{
    int n = 0;
    for (Character* c : m_characters) {
        if (c->m_isHelper || c->m_team == of->m_team)
            continue;
        if (n++ == index)
            return c;
    }
    return nullptr;
}

Character* Match::enemyNear(const Character* of, int index) const
{
    QList<Character*> near;
    for (Character* c : m_characters) {
        if (!c->m_isHelper && c->m_team != of->m_team)
            near << c;
    }
    std::stable_sort(near.begin(), near.end(), [of](const Character* a, const Character* b) {
        return std::abs(a->m_pos.x - of->m_pos.x) < std::abs(b->m_pos.x - of->m_pos.x);
    });
    return index >= 0 && index < near.size() ? near[index] : nullptr;
}

int Match::numEnemies(const Character* of) const
{
    return int(std::count_if(m_characters.cbegin(), m_characters.cend(),
                             [of](const Character* c) { return !c->m_isHelper && c->m_team != of->m_team; }));
}

Character* Match::playerById(int id) const
{
    for (Character* c : m_characters) {
        if (c->m_id == id && !c->m_destroyed)
            return c;
    }
    return nullptr;
}

Character* Match::spawnHelper(Character& parent)
{
    Character* player = parent.m_root ? parent.m_root : &parent;
    const auto count = std::count_if(m_characters.cbegin(), m_characters.cend(), [player](const Character* c) {
        return c->m_isHelper && !c->m_destroyed && c->m_root == player;
    });
    if (count >= MaxHelpers)
        return nullptr;
    auto helper = std::make_unique<Character>(parent.m_data, this);
    Character* h = helper.get();
    h->m_isHelper = true;
    h->m_keyCtrl = false;
    h->m_parent = &parent;
    h->m_root = player;
    h->m_team = parent.m_team;
    h->m_life = int(parent.m_data.constant(QStringLiteral("data.life"), 1000));
    m_helpers.push_back(std::move(helper));
    return h;
}

void Match::removeDestroyed()
{
    for (int i = int(m_characters.size()) - 1; i >= 0; --i) {
        Character* gone = m_characters[i];
        if (!gone->m_destroyed)
            continue;
        for (Character* c : std::as_const(m_characters)) {
            if (c == gone)
                continue;
            for (int t = int(c->m_targets.size()) - 1; t >= 0; --t) {
                if (c->m_targets[t] == gone) {
                    c->m_targets.removeAt(t);
                    c->m_targetIds.removeAt(t);
                }
            }
            if (c->m_bindTo == gone) {
                c->m_bindTo = nullptr;
                c->m_bindTime = 0;
            }
            if (c->m_parent == gone)
                c->m_parent = nullptr;
            for (Explod& e : c->m_explods) {
                if (e.bindTo == gone) {
                    e.bindTo = nullptr;
                    e.bindTime = 0;
                }
            }
        }
        if (Character* player = gone->m_root) {
            for (Explod& e : gone->m_explods) {
                if (e.bindTo == gone) {
                    e.bindTo = nullptr;
                    e.bindTime = 0;
                }
                player->m_explods << e;
            }
        }
        m_characters.removeAt(i);
        std::erase_if(m_helpers, [gone](const std::unique_ptr<Character>& h) { return h.get() == gone; });
    }
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

void Match::requestPause(bool super, int playerId, int time, bool pauseBg, bool darken)
{
    // A longer pause replaces one waiting to start, and a player may replace its own.
    int& pending = super ? m_superPending : m_pausePending;
    int& player = super ? m_superPlayer : m_pausePlayer;
    if (pending < 0 || time > pending || player == playerId) {
        pending = std::max(0, time);
        player = playerId;
    }
    if (super) {
        m_superPauseBg = pauseBg;
        m_superDarken = darken;
    } else {
        m_pauseBg = pauseBg;
    }
}

void Match::tick()
{
    m_sounds.clear();
    m_spawns.clear();
    if (m_superTime > 0)
        --m_superTime;
    else if (m_pauseTime > 0)
        --m_pauseTime;
    if (m_superPending >= 0) {
        m_superTime = m_superPending;
        m_superPending = -1;
    }
    if (m_pausePending >= 0) {
        m_pauseTime = m_pausePending;
        m_pausePending = -1;
    }
    for (Character* c : std::as_const(m_characters)) {
        bool near = false;
        for (const Character* other : std::as_const(m_characters)) {
            if (other->m_team == c->m_team || other->moveType() != QLatin1Char('A'))
                continue;
            const int guardDist = other->hitDef().active ? other->hitDef().guardDist : -1;
            const float reach =
                guardDist >= 0 ? float(guardDist) : other->data().constant(QStringLiteral("size.attack.dist"), 160);
            near = near || std::abs(other->pos().x() - c->pos().x()) <= reach;
        }
        c->setInGuardDist(near);
    }
    for (int i = 0; i < m_characters.size(); ++i) {
        if (!m_characters[i]->m_destroyed)
            m_characters[i]->runActions();
    }
    push();
    keepOnScreen();
    for (Character* c : std::as_const(m_characters))
        c->updateBindings();
    for (Character* c : std::as_const(m_characters))
        c->updateHelperBinding();
    for (Character* c : std::as_const(m_characters))
        c->updateProjectiles();
    for (int i = 0; i < m_characters.size(); ++i) {
        for (int j = i + 1; j < m_characters.size(); ++j) {
            if (m_characters[i]->m_team != m_characters[j]->m_team)
                m_characters[i]->cancelProjectiles(*m_characters[j]);
        }
    }
    for (Character* attacker : std::as_const(m_characters)) {
        for (Character* target : std::as_const(m_characters)) {
            if (attacker->m_team != target->m_team && !attacker->m_destroyed && !target->m_destroyed)
                attacker->attack(*target);
        }
    }
    for (Character* attacker : std::as_const(m_characters)) {
        for (Character* target : std::as_const(m_characters)) {
            if (attacker->m_team != target->m_team && !target->m_destroyed)
                attacker->projectilesAttack(*target);
        }
    }
    for (Character* c : std::as_const(m_characters))
        c->tickBinding();
    for (Character* c : std::as_const(m_characters))
        c->finishTick();
    for (Character* c : std::as_const(m_characters))
        c->tickProjectiles();
    for (Character* c : std::as_const(m_characters))
        c->releaseTargets();
    for (Character* c : std::as_const(m_characters))
        c->updateExplods();
    removeDestroyed();
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
            if (!a->pushes() || !b->pushes() || a->m_team == b->m_team || a->m_destroyed || b->m_destroyed)
                continue;
            const float aHeight = a->constant(QStringLiteral("size.height"), 60);
            const float bHeight = b->constant(QStringLiteral("size.height"), 60);
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
