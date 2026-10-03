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
    h->m_life = int(parent.m_data.lifeMax());
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

void Match::requestPause(bool super, int playerId, int time, bool pauseBg, bool darken, int endCmdBufTime)
{
    // A longer pause replaces one waiting to start, and a player may replace its own.
    int& pending = super ? m_superPending : m_pausePending;
    int& player = super ? m_superPlayer : m_pausePlayer;
    if (pending < 0 || time > pending || player == playerId) {
        pending = std::max(0, time);
        player = playerId;
        (super ? m_superEndCmdBufTime : m_pauseEndCmdBufTime) = std::clamp(endCmdBufTime, 0, pending);
    }
    if (super) {
        m_superPauseBg = pauseBg;
        m_superDarken = darken;
    } else {
        m_pauseBg = pauseBg;
    }
}

void Match::startMatch(const RoundSettings& settings)
{
    m_settings = settings;
    m_roundActive = true;
    m_roundNo = 0;
    m_wins[0] = m_wins[1] = 0;
    m_roundWins[0].clear();
    m_roundWins[1].clear();
    m_matchOver = false;
    startRound();
}

void Match::resetPositions()
{
    for (Character* c : std::as_const(m_characters)) {
        if (c->m_isHelper)
            continue;
        c->m_pos = Vec2{c->m_team == 0 ? m_settings.p1x : m_settings.p2x, 0};
        c->m_vel = Vec2();
        c->m_facing = c->m_team == 0 ? 1 : -1;
    }
}

void Match::startRound()
{
    ++m_roundNo;
    m_intro = m_settings.startWait + m_settings.ctrlTime + 1;
    m_introClock = -1;
    m_timer = m_settings.time < 0 ? -1 : m_settings.time * m_settings.framesPerCount;
    m_winPoseTime = m_settings.overWinTime;
    m_end = RoundEnd::None;
    m_winner = -1;
    m_perfect = false;
    for (Character* c : std::as_const(m_characters)) {
        if (c->m_isHelper)
            c->m_destroyed = true;
    }
    removeDestroyed();
    for (Character* c : std::as_const(m_characters)) {
        c->resetForRound(Vec2{c->m_team == 0 ? m_settings.p1x : m_settings.p2x, 0}, c->m_team == 0 ? 1 : -1,
                         m_roundNo == 1);
    }
}

int Match::roundState() const
{
    if (!m_roundActive)
        return 2;
    if (m_intro > m_settings.ctrlTime + 1)
        return 0;
    if (m_intro > 0)
        return 1;
    if (m_intro == 0 || m_end == RoundEnd::None)
        return 2;
    if (m_intro < -m_settings.overWaitTime)
        return 4;
    return 3;
}

bool Match::hitsAllowed() const
{
    return !m_roundActive || m_end == RoundEnd::None || m_intro >= -m_settings.overHitTime;
}

bool Match::decideRound()
{
    // Once fighting, a side with nobody standing loses (both: a draw), or the side with less life at time over.
    if (m_intro > 0 || m_end != RoundEnd::None)
        return false;
    bool down[2] = {true, true};
    double life[2] = {0, 0};
    bool full[2] = {true, true};
    for (const Character* c : std::as_const(m_characters)) {
        if (c->m_isHelper || c->m_team < 0 || c->m_team > 1)
            continue;
        const int max = int(c->m_data.lifeMax());
        if (c->alive())
            down[c->m_team] = false;
        life[c->m_team] += max > 0 ? double(std::max(0, c->m_life)) / max : 0;
        full[c->m_team] = full[c->m_team] && c->m_life >= max;
    }
    if (down[0] || down[1]) {
        m_end = down[0] && down[1] ? RoundEnd::DoubleKO : RoundEnd::KO;
        m_winner = down[0] && down[1] ? 2 : down[0] ? 1 : 0;
    } else if (m_timer == 0) {
        m_end = RoundEnd::TimeOver;
        m_winner = life[0] == life[1] ? 2 : life[0] > life[1] ? 0 : 1;
    } else {
        return false;
    }
    m_perfect = (m_winner == 0 || m_winner == 1) && full[m_winner];
    return true;
}

const Character* Match::attackerNear(const Character* c) const
{
    for (const Character* other : std::as_const(m_characters)) {
        if (other->m_team == c->m_team || other->m_destroyed)
            continue;
        // A projectile on its way: within its guard.dist (else the owner's size.proj.attack.dist).
        for (const Projectile& p : other->m_projectiles) {
            if (p.gone || p.status != Projectile::Status::Active || p.hits <= 0)
                continue;
            const float reach = p.hitDef.guardDist >= 0
                                    ? float(p.hitDef.guardDist)
                                    : other->data().constant(QStringLiteral("size.proj.attack.dist"), 90);
            if (std::abs(p.pos.x - c->m_pos.x) <= reach)
                return other;
        }
        if (other->moveType() != QLatin1Char('A'))
            continue;
        const int guardDist = other->hitDef().active ? other->hitDef().guardDist : -1;
        const float reach =
            guardDist >= 0 ? float(guardDist) : other->data().constant(QStringLiteral("size.attack.dist"), 160);
        if (std::abs(other->pos().x() - c->pos().x()) <= reach)
            return other;
    }
    return nullptr;
}

WinType Match::winType() const
{
    if (m_end == RoundEnd::TimeOver)
        return WinType::Time;
    const Character* loser = nullptr;
    for (const Character* c : std::as_const(m_characters)) {
        if (!c->m_isHelper && c->m_team == 1 - m_winner && !c->alive())
            loser = c;
    }
    if (!loser)
        return WinType::Normal;
    const GetHitVars& ghv = loser->m_ghv;
    if (ghv.attr.isEmpty())
        return WinType::Suicide;
    if (ghv.guarded)
        return WinType::Cheese;
    if (ghv.attr.size() > 1 && ghv.attr[1] == QLatin1Char('T'))
        return WinType::Throw;
    if (ghv.attr.startsWith(QLatin1Char('H')))
        return WinType::Hyper;
    if (ghv.attr.startsWith(QLatin1Char('S')))
        return WinType::Special;
    return WinType::Normal;
}

void Match::stepRound()
{
    if (!m_roundActive || m_matchOver)
        return;
    const RoundSettings& s = m_settings;
    if (m_introClock >= 0 || m_intro <= s.ctrlTime)
        ++m_introClock;
    if (m_intro > s.ctrlTime) {
        // Before and during the intros. An intro holds the round while it is asserted.
        --m_intro;
        if (isAsserted(QStringLiteral("intro")) && m_intro <= s.ctrlTime)
            m_intro = s.ctrlTime + 1;
    } else if (m_intro > 0) {
        if (m_intro == s.ctrlTime && m_introClock == 0)
            resetPositions();
        const bool calling = m_intro == s.ctrlTime && m_introClock < s.callTime;
        if (!calling && --m_intro == 0) {
            for (Character* c : std::as_const(m_characters)) {
                if (c->m_isHelper || !c->alive())
                    continue;
                c->m_ctrl = true;
                if (c->m_stateNo != 0) {
                    c->m_stateData = &c->m_data;
                    c->m_minus = 3;
                    c->changeState(0, -1, 1);
                }
            }
        }
    }
    if (m_intro == 0 && !isAsserted(QStringLiteral("timerfreeze")) && m_superTime <= 0 && m_pauseTime <= 0 &&
        m_timer > 0)
        --m_timer;
    if (m_end == RoundEnd::None && !decideRound()) {
        if (m_intro < 0)
            m_intro = 0;
        return;
    }
    --m_intro;
    if (m_intro == -s.overHitTime && (m_winner == 0 || m_winner == 1)) {
        ++m_wins[m_winner];
        m_roundWins[m_winner] << RoundWin{winType(), m_perfect};
    }
    const int poses = -s.overWaitTime;
    if (m_intro == poses - 1) {
        for (Character* c : std::as_const(m_characters))
            c->m_ctrl = false;
    }
    if (m_intro < poses)
        --m_winPoseTime;
    if (m_winPoseTime <= 0) {
        for (Character* c : std::as_const(m_characters)) {
            if (c->m_isHelper || c->m_posed || !c->alive() || c->m_hitPauseTime > 0)
                continue;
            c->m_posed = true;
            const int state = m_winner == c->m_team ? 180 : (m_winner == 0 || m_winner == 1) ? 170 : 175;
            c->m_stateData = &c->m_data;
            c->m_minus = 3;
            c->changeState(state, -1, -1);
        }
    }
    if (m_intro <= poses - s.overTime && !isAsserted(QStringLiteral("roundnotover"))) {
        if (m_wins[0] >= s.wins || m_wins[1] >= s.wins)
            m_matchOver = true;
        else
            startRound();
    }
}

void Match::tick()
{
    m_sounds.clear();
    m_spawns.clear();
    // Screen and palette effects count their time in frames shown, so last tick's run down now.
    if (m_envShake.time > 0)
        --m_envShake.time;
    if (m_envColor.time > 0)
        --m_envColor.time;
    m_allPalFx.tick();
    m_bgPalFx.tick();
    for (Character* c : std::as_const(m_characters))
        c->m_draw.palFx.tick();
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
    stepRound();
    for (Character* c : std::as_const(m_characters))
        c->setInGuardDist(attackerNear(c) != nullptr);
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
    if (hitsAllowed()) {
        // Decide everything first, then carry it out: reversals, then the attacks that land.
        QList<std::pair<Character*, Character*>> reversals;
        QList<std::pair<Character*, Character*>> hits;
        auto opposed = [](const Character* a, const Character* b) {
            return a->m_team != b->m_team && !a->m_destroyed && !b->m_destroyed;
        };
        auto affects = [](const Character* a, const Character* b) {
            if (a == b || a->m_destroyed || b->m_destroyed)
                return false;
            const AffectTeam team = a->m_hitDef.affectTeam;
            return a->m_team != b->m_team ? team != AffectTeam::Friend : team != AffectTeam::Enemy;
        };
        for (Character* a : std::as_const(m_characters)) {
            for (Character* b : std::as_const(m_characters)) {
                if (opposed(a, b) && a->reverses(*b))
                    reversals << std::pair(a, b);
            }
        }
        auto reversed = [&reversals](const Character* c) {
            return std::any_of(reversals.cbegin(), reversals.cend(), [c](const auto& r) { return r.second == c; });
        };
        for (Character* a : std::as_const(m_characters)) {
            for (Character* b : std::as_const(m_characters)) {
                if (affects(a, b) && !reversed(a) && a->wouldHit(*b) && a->lands(*b))
                    hits << std::pair(a, b);
            }
        }
        for (const auto& [reverser, attacker] : std::as_const(reversals))
            reverser->reverse(*attacker);
        for (const auto& [attacker, target] : std::as_const(hits))
            attacker->attack(*target);
        for (Character* c : std::as_const(m_characters)) {
            if (c->m_hitDefLanded)
                c->m_hitDef.active = false;
            c->m_hitDefLanded = false;
        }
        for (Character* attacker : std::as_const(m_characters)) {
            for (Character* target : std::as_const(m_characters)) {
                if (attacker != target && !target->m_destroyed)
                    attacker->projectilesAttack(*target);
            }
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
        const float leftEdge = c->facing() > 0 ? c->backEdgeWidth() : c->frontEdgeWidth();
        const float rightEdge = c->facing() > 0 ? c->frontEdgeWidth() : c->backEdgeWidth();
        const float low = m_cameraX - half + m_screenEdge + leftEdge;
        const float high = std::max(low, m_cameraX + half - m_screenEdge - rightEdge);
        const float x = std::clamp(float(c->pos().x()), low, high);
        c->moveX(x - float(c->pos().x()));
    }
}

} // namespace OFS::Mugen::Runtime
