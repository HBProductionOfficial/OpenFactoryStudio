#include "engines/mugen/runtime/Character.h"
#include "engines/mugen/runtime/CharacterDetail.h"

#include <algorithm>
#include <cmath>

namespace OFS::Mugen::Runtime {

using namespace Detail;

void Character::enterGetHitState()
{
    m_moveType = QLatin1Char('H');
    m_hitPauseTime = 0;
    if (m_hitStateNo >= 0) {
        const int state = m_hitStateNo;
        m_hitStateNo = -1;
        m_stateData = m_hitStateOwner ? m_hitStateOwner : &m_data;
        m_hitStateOwner = nullptr;
        changeState(state, -1, 0);
        m_moveType = QLatin1Char('H');
        return;
    }
    m_stateData = &m_data;
    if (m_ghv.guarded && m_life > 0) {
        changeState(m_stateType == QLatin1Char('S') ? 150 : m_stateType == QLatin1Char('C') ? 152 : 154, -1, 0);
        return;
    }
    if (m_stateType == QLatin1Char('L') && m_pos.y == 0)
        changeState(5080, -1, 0);
    else if (m_ghv.type == HitType::Trip)
        changeState(5070, -1, 0);
    else if (m_stateType == QLatin1Char('S') || (m_stateType == QLatin1Char('C') && m_life <= 0))
        changeState(5000, -1, 0);
    else if (m_stateType == QLatin1Char('C'))
        changeState(5010, -1, 0);
    else
        changeState(5020, -1, 0);
}

QList<QRectF> Character::boxes(int type) const
{
    return clsnBoxes(m_animData->animation(m_animNo), m_anim, type, m_pos, m_facing);
}

bool Character::canGuard(const HitDef& hit) const
{
    const bool guardState = inGuardState();
    if (!m_commands.holdingBack())
        return false;
    if ((m_stateType == QLatin1Char('S') && m_asserted.contains(QStringLiteral("nostandguard"))) ||
        (m_stateType == QLatin1Char('C') && m_asserted.contains(QStringLiteral("nocrouchguard"))) ||
        (m_stateType == QLatin1Char('A') && m_asserted.contains(QStringLiteral("noairguard"))))
        return false;
    if (!(((m_ctrl || m_stateNo == 52) && m_moveType == QLatin1Char('I')) || guardState))
        return false;
    if (m_moveType == QLatin1Char('H') && !m_ghv.guarded)
        return false;
    // M is high and low.
    const QString flags = hit.guardFlag.toUpper();
    const bool mid = flags.contains(QLatin1Char('M'));
    return (m_stateType == QLatin1Char('S') && (mid || flags.contains(QLatin1Char('H')))) ||
           (m_stateType == QLatin1Char('C') && (mid || flags.contains(QLatin1Char('L')))) ||
           (m_stateType == QLatin1Char('A') && flags.contains(QLatin1Char('A')));
}

bool Character::hitAllowed(const HitDef& hd, const Character& target) const
{
    const QString flags = hd.hitFlag.toUpper();
    const QChar type = target.m_stateType;
    const bool falling = type == QLatin1Char('A') && target.m_ghv.fall && target.m_moveType == QLatin1Char('H');
    bool allowed = false;
    if (type == QLatin1Char('S'))
        allowed = flags.contains(QLatin1Char('H')) || flags.contains(QLatin1Char('M'));
    else if (type == QLatin1Char('C'))
        allowed = flags.contains(QLatin1Char('L')) || flags.contains(QLatin1Char('M'));
    else if (type == QLatin1Char('A'))
        allowed = falling ? flags.contains(QLatin1Char('F')) : flags.contains(QLatin1Char('A'));
    else if (type == QLatin1Char('L'))
        allowed = flags.contains(QLatin1Char('D'));
    if (flags.contains(QLatin1Char('+')) && target.m_moveType != QLatin1Char('H'))
        allowed = false;
    if (!target.hittableBy(hd) || target.m_unhittableTime > 0)
        allowed = false;
    if (flags.contains(QLatin1Char('-')) && target.m_moveType == QLatin1Char('H'))
        allowed = false;
    return allowed;
}

namespace {

bool attributesMatch(const QString& states, const QStringList& classes, QChar state, const QStringList& attackClasses)
{
    if (!states.contains(state))
        return false;
    for (const QString& k : attackClasses) {
        for (const QString& c : classes) {
            const QString wanted = c.trimmed().toUpper();
            const QString attack = k.trimmed().toUpper();
            if (wanted == attack || (wanted.size() == 2 && wanted.at(0) == QLatin1Char('A') && attack.size() == 2 &&
                                     attack.at(1) == wanted.at(1)))
                return true;
        }
    }
    return false;
}

} // namespace

bool Character::wouldHit(const Character& target) const
{
    return m_hitDef.active && !m_hitDef.reversal && &target != this && !m_frozen && hitAllowed(m_hitDef, target) &&
           boxesMeet(boxes(1), target.boxes(2));
}

bool Character::lands(const Character& target) const
{
    // Against an attack of the target's that reaches back this tick, the higher priority lands. At equal
    // priority Hit trades with Hit and beats Miss, and Dodge or Miss against Miss lets neither land.
    if (!target.wouldHit(*this))
        return true;
    const HitDef& mine = m_hitDef;
    const HitDef& theirs = target.m_hitDef;
    if (theirs.priority != mine.priority)
        return theirs.priority < mine.priority;
    if (theirs.priorityType == PriorityType::Dodge || mine.priorityType == PriorityType::Dodge)
        return false;
    if (mine.priorityType == PriorityType::Miss)
        return false;
    return true;
}

bool Character::reverses(const Character& attacker) const
{
    const HitDef& rd = m_hitDef;
    const HitDef& hd = attacker.m_hitDef;
    if (!rd.active || !rd.reversal || m_frozen || !hd.active || hd.reversal || attacker.m_frozen)
        return false;
    if (!attributesMatch(rd.reversalStates, rd.reversalClasses, hd.attrStates.isEmpty() ? QChar() : hd.attrStates.at(0),
                         hd.attrClasses))
        return false;
    return boxesMeet(boxes(1), attacker.boxes(1));
}

void Character::reverse(Character& attacker)
{
    const HitDef rd = m_hitDef;
    if (rd.pauseTime[0] > 0)
        m_hitPauseTime = rd.pauseTime[0];
    m_moveContactTime = 1;
    m_moveGuarded = false;
    m_hitDef.active = false;
    if (!m_targets.contains(&attacker)) {
        m_targets << &attacker;
        m_targetIds << rd.id;
    }
    if (rd.p1StateNo >= 0) {
        m_pendingP1State = rd.p1StateNo;
        m_ctrl = false;
    }
    attacker.m_hitDef.active = false;
    attacker.m_moveReversedTime = 1;
    attacker.m_ctrl = false;
    attacker.m_ghv.hitShakeTime = std::max(0, rd.pauseTime[1]);
    if (rd.p2StateNo >= 0) {
        attacker.m_gotHit = true;
        attacker.m_hitStateNo = rd.p2StateNo;
        attacker.m_hitStateOwner = rd.p2GetP1State ? &m_data : &attacker.m_data;
    }
}

int Character::overrideFor(const HitDef& hit, QChar attackerState) const
{
    for (int i = 0; i < int(m_overrides.size()); ++i) {
        const HitOverrideSlot& slot = m_overrides[std::size_t(i)];
        if (slot.time == 0 || slot.stateNo < 0)
            continue;
        if (attributesMatch(slot.states, QStringList(slot.classes.cbegin(), slot.classes.cend()), attackerState,
                            hit.attrClasses))
            return i;
    }
    return -1;
}

bool Character::attack(Character& target)
{
    if (!m_hitDef.active)
        return false;
    const HitDef& hd = m_hitDef;
    // The target's HitOverride takes the attack, but throws (p1stateno, p2stateno) miss it.
    const int slot = target.overrideFor(hd, m_stateType);
    if (slot >= 0 && (hd.p1StateNo >= 0 || hd.p2StateNo >= 0))
        return false;
    const bool guarded = deliverHit(target, hd, m_facing, slot);
    if (slot >= 0) {
        if (hd.pauseTime[0] > 0)
            m_hitPauseTime = hd.pauseTime[0];
        m_moveContactTime = 1;
        m_moveGuarded = false;
        m_hitCount += hd.numHits;
        m_hitDef.active = false;
        return true;
    }

    const int pause = guarded ? hd.guardPauseTime[0] : hd.pauseTime[0];
    if (pause > 0)
        m_hitPauseTime = pause;
    m_moveContactTime = 1;
    m_moveGuarded = guarded;
    if (!guarded) {
        m_hitCount += hd.numHits;
        ++m_uniqHitCount;
        if (hd.p1StateNo >= 0) {
            m_pendingP1State = hd.p1StateNo;
            m_ctrl = false;
        }
        if (hd.p2Facing > 0)
            target.m_facing = -m_facing;
        else if (hd.p2Facing < 0)
            target.m_facing = m_facing;
        if (hd.p1Facing < 0)
            m_facing = -m_facing;
    }
    m_hitDef.active = false;
    return true;
}

bool Character::deliverHit(Character& target, const HitDef& hd, int facing, int overrideSlot)
{
    const Character::HitOverrideSlot* override =
        overrideSlot >= 0 ? &target.m_overrides[std::size_t(overrideSlot)] : nullptr;
    if (override && override->forceAir)
        target.m_stateType = QLatin1Char('A');
    const QChar type = target.m_stateType;
    const bool guarded = !override && target.canGuard(hd) && !m_asserted.contains(QStringLiteral("unguardable"));
    const float attackMul = m_data.constant(QStringLiteral("data.attack"), 100) / 100.0f * m_attackMul;
    const float defence = target.m_data.constant(QStringLiteral("data.defence"), 100) / 100.0f * target.m_defenceMul *
                          target.m_superDefenceMul;
    int damage = int(std::lround((guarded ? hd.guardDamage : hd.hitDamage) * attackMul / std::max(0.01f, defence)));
    const bool kill = guarded ? hd.guardKill : hd.kill;
    if (!kill && damage >= target.m_life)
        damage = std::max(0, target.m_life - 1);
    if (!override)
        target.m_life = std::max(0, target.m_life - damage);
    const int maxPower = int(m_data.constant(QStringLiteral("data.power"), 3000));
    powerOwner()->m_power =
        std::clamp(powerOwner()->m_power + (guarded ? hd.guardGetPower : hd.hitGetPower), 0, maxPower);
    const int targetMaxPower = int(target.m_data.constant(QStringLiteral("data.power"), 3000));
    target.powerOwner()->m_power =
        std::clamp(target.powerOwner()->m_power + (guarded ? hd.guardGivePower : hd.hitGivePower), 0, targetMaxPower);

    GetHitVars& ghv = target.m_ghv;
    const bool combo = target.m_moveType == QLatin1Char('H') && !ghv.guarded;
    const float side = float(-facing); // velocities are given towards the attacker's front
    ghv.damage = damage;
    ghv.kill = kill;
    ghv.groundType = hd.groundType;
    ghv.airType = hd.airType;
    ghv.type = type == QLatin1Char('A') ? hd.airType : hd.groundType;
    ghv.yAccel = hd.yAccel;
    ghv.hitId = hd.id;
    if (guarded) {
        ghv.guarded = true;
        ghv.hitShakeTime = std::max(0, hd.guardPauseTime[1]);
        ghv.hitTime = std::max(0, hd.guardHitTime);
        ghv.slideTime = hd.guardSlideTime;
        if (type == QLatin1Char('A')) {
            ghv.ctrlTime = hd.airGuardCtrlTime;
            ghv.xvel = hd.airGuardVelocity[0] * side;
            ghv.yvel = hd.airGuardVelocity[1];
        } else {
            ghv.ctrlTime = hd.guardCtrlTime;
            ghv.xvel = hd.guardVelocity * side;
            ghv.yvel = 0;
        }
        ghv.fall = false;
    } else {
        ghv.guarded = false;
        ghv.hitShakeTime = std::max(0, hd.pauseTime[1]);
        ghv.slideTime = hd.groundSlideTime;
        ghv.fallXVelocity = hd.fallXVelocity;
        ghv.fallYVelocity = hd.fallYVelocity;
        ghv.fallRecover = hd.fallRecover;
        ghv.fallRecoverTime = hd.fallRecoverTime;
        ghv.fallDamage = hd.fallDamage;
        ghv.fallKill = hd.fallKill;
        ghv.fallEnvShakeTime = hd.fallEnvShakeTime;
        ghv.fallEnvShakeFreq = hd.fallEnvShakeFreq;
        ghv.fallEnvShakeAmpl = hd.fallEnvShakeAmpl;
        ghv.fallEnvShakePhase = hd.fallEnvShakePhase;
        ghv.downRecover = hd.downRecover;
        const int down = hd.downRecoverTime >= 0 ? hd.downRecoverTime
                                                 : int(target.m_data.constant(QStringLiteral("data.liedown.time"), 60));
        if (ghv.downRecoverTime > down || type != QLatin1Char('L'))
            ghv.downRecoverTime = down;
        if (type == QLatin1Char('A')) {
            ghv.hitTime = hd.airHitTime;
            ghv.ctrlTime = hd.airHitTime;
            ghv.xvel = hd.airVelocity[0] * side;
            ghv.yvel = hd.airVelocity[1];
            ghv.fall = ghv.fall || hd.airFall;
        } else if (type == QLatin1Char('L')) {
            ghv.hitTime = hd.downHitTime;
            ghv.ctrlTime = hd.downHitTime;
            ghv.xvel = hd.downVelocity[0] * side;
            ghv.yvel = hd.downVelocity[1];
            ghv.fall = ghv.fall || hd.fall;
        } else {
            ghv.ctrlTime = hd.groundHitTime;
            ghv.xvel = hd.groundVelocity[0] * side;
            ghv.yvel = hd.groundVelocity[1];
            ghv.fall = ghv.fall || hd.fall;
            if (ghv.fall && ghv.yvel == 0)
                ghv.yvel = -0.001f; // the engine lifts falling characters a little
            ghv.hitTime = ghv.yvel != 0 ? hd.airHitTime : hd.groundHitTime;
        }
        ghv.hitTime = std::max(0, ghv.hitTime);
        if (ghv.hitTime > 0)
            ++ghv.hitTime;
        ghv.hitCount = combo ? ghv.hitCount + 1 : 1;
        if (ghv.fall)
            ++ghv.fallCount;
    }
    ghv.groundAnimType = hd.animType;
    ghv.airAnimType = hd.airAnimType;
    ghv.fallAnimType = hd.fallAnimType;
    if (ghv.fall)
        ghv.animType = hd.fallAnimType;
    else if (type == QLatin1Char('A'))
        ghv.animType = hd.airAnimType;
    else
        ghv.animType = hd.animType >= AnimType::Back && ghv.yvel == 0 ? AnimType::Hard : hd.animType;
    target.m_gotHit = true;
    target.m_hitStateNo = guarded ? -1 : hd.p2StateNo;
    target.m_hitStateOwner = hd.p2GetP1State ? &m_data : &target.m_data;
    target.m_ctrl = false;
    if (override) {
        target.m_hitStateNo = override->stateNo;
        target.m_hitStateOwner = &target.m_data;
        return false;
    }
    if (!guarded && !m_targets.contains(&target)) {
        m_targets << &target;
        m_targetIds << hd.id;
    }
    return guarded;
}

void Character::readHitDef(const Controller& c)
{
    m_hitDef = hitDefFrom(c);
}

HitDef Character::hitDefFrom(const Controller& c)
{
    HitDef hd;
    if (const Param* attr = c.param(QStringLiteral("attr"))) {
        const QStringList parts = attr->text.split(QLatin1Char(','));
        hd.attrStates = parts.value(0).trimmed().toUpper();
        for (int i = 1; i < parts.size(); ++i)
            hd.attrClasses << parts[i].trimmed().toUpper();
    }
    if (const Param* p = c.param(QStringLiteral("hitflag")))
        hd.hitFlag = p->text.trimmed();
    if (const Param* p = c.param(QStringLiteral("guardflag")))
        hd.guardFlag = p->text.trimmed();
    auto animType = [&c](const QString& name, AnimType fallback) {
        const Param* p = c.param(name);
        if (!p)
            return fallback;
        const QString t = p->text.trimmed().toLower();
        if (t.startsWith(QLatin1String("diag")))
            return AnimType::DiagUp;
        if (t.startsWith(QLatin1Char('u')))
            return AnimType::Up;
        if (t.startsWith(QLatin1Char('b')))
            return AnimType::Back;
        if (t.startsWith(QLatin1Char('h')))
            return AnimType::Hard;
        if (t.startsWith(QLatin1Char('m')))
            return AnimType::Medium;
        return AnimType::Light;
    };
    auto hitType = [&c](const QString& name, HitType fallback) {
        const Param* p = c.param(name);
        if (!p)
            return fallback;
        const QString t = p->text.trimmed().toLower();
        if (t.startsWith(QLatin1Char('l')))
            return HitType::Low;
        if (t.startsWith(QLatin1Char('t')))
            return HitType::Trip;
        if (t.startsWith(QLatin1Char('n')))
            return HitType::None;
        return HitType::High;
    };
    hd.animType = animType(QStringLiteral("animtype"), AnimType::Light);
    hd.airAnimType = animType(QStringLiteral("air.animtype"), hd.animType);
    hd.fallAnimType =
        animType(QStringLiteral("fall.animtype"), hd.airAnimType == AnimType::Up ? AnimType::Up : AnimType::Back);
    hd.hitDamage = intParam(c, QStringLiteral("damage"), 0, 0);
    hd.guardDamage = intParam(c, QStringLiteral("damage"), 0, 1);
    hd.pauseTime[0] = intParam(c, QStringLiteral("pausetime"), 0, 0);
    hd.pauseTime[1] = intParam(c, QStringLiteral("pausetime"), 0, 1);
    hd.guardPauseTime[0] = intParam(c, QStringLiteral("guard.pausetime"), hd.pauseTime[0], 0);
    hd.guardPauseTime[1] = intParam(c, QStringLiteral("guard.pausetime"), hd.pauseTime[1], 1);
    hd.groundType = hitType(QStringLiteral("ground.type"), HitType::High);
    hd.airType = hitType(QStringLiteral("air.type"), hd.groundType);
    hd.groundSlideTime = intParam(c, QStringLiteral("ground.slidetime"), 0);
    hd.groundHitTime = intParam(c, QStringLiteral("ground.hittime"), 0);
    hd.guardSlideTime = intParam(c, QStringLiteral("guard.slidetime"), hd.groundSlideTime);
    hd.guardHitTime = intParam(c, QStringLiteral("guard.hittime"), hd.groundHitTime);
    hd.guardCtrlTime = intParam(c, QStringLiteral("guard.ctrltime"), hd.guardSlideTime);
    hd.airGuardCtrlTime = intParam(c, QStringLiteral("airguard.ctrltime"), hd.guardCtrlTime);
    hd.airHitTime = intParam(c, QStringLiteral("air.hittime"), 20);
    hd.groundVelocity[0] = floatParam(c, QStringLiteral("ground.velocity"), 0, 0);
    hd.groundVelocity[1] = floatParam(c, QStringLiteral("ground.velocity"), 0, 1);
    hd.guardVelocity = floatParam(c, QStringLiteral("guard.velocity"), hd.groundVelocity[0]);
    hd.airVelocity[0] = floatParam(c, QStringLiteral("air.velocity"), 0, 0);
    hd.airVelocity[1] = floatParam(c, QStringLiteral("air.velocity"), 0, 1);
    hd.airGuardVelocity[0] = floatParam(c, QStringLiteral("airguard.velocity"), hd.airVelocity[0] * 1.5f, 0);
    hd.airGuardVelocity[1] = floatParam(c, QStringLiteral("airguard.velocity"), hd.airVelocity[1] * 0.5f, 1);
    hd.downVelocity[0] = floatParam(c, QStringLiteral("down.velocity"), hd.airVelocity[0], 0);
    hd.downVelocity[1] = floatParam(c, QStringLiteral("down.velocity"), hd.airVelocity[1], 1);
    hd.downHitTime = intParam(c, QStringLiteral("down.hittime"), 20);
    hd.yAccel = floatParam(c, QStringLiteral("yaccel"), 0.35f);
    hd.fall = intParam(c, QStringLiteral("fall"), 0) != 0;
    hd.airFall = intParam(c, QStringLiteral("air.fall"), hd.fall ? 1 : 0) != 0;
    if (has(c, QStringLiteral("fall.xvelocity")))
        hd.fallXVelocity = floatParam(c, QStringLiteral("fall.xvelocity"), 0);
    hd.fallYVelocity = floatParam(c, QStringLiteral("fall.yvelocity"), -4.5f);
    hd.fallRecover = intParam(c, QStringLiteral("fall.recover"), 1) != 0;
    hd.fallRecoverTime = intParam(c, QStringLiteral("fall.recovertime"), 4);
    hd.downRecover = intParam(c, QStringLiteral("down.recover"), 1) != 0;
    hd.downRecoverTime = intParam(c, QStringLiteral("down.recovertime"), -1);
    hd.fallDamage = intParam(c, QStringLiteral("fall.damage"), 0);
    hd.p1StateNo = intParam(c, QStringLiteral("p1stateno"), -1);
    hd.p2StateNo = intParam(c, QStringLiteral("p2stateno"), -1);
    hd.kill = intParam(c, QStringLiteral("kill"), 1) != 0;
    hd.guardKill = intParam(c, QStringLiteral("guard.kill"), 1) != 0;
    hd.fallKill = intParam(c, QStringLiteral("fall.kill"), 1) != 0;
    hd.numHits = intParam(c, QStringLiteral("numhits"), 1);
    hd.hitGetPower = intParam(c, QStringLiteral("getpower"), hd.hitDamage, 0);
    hd.guardGetPower = intParam(c, QStringLiteral("getpower"), hd.hitGetPower / 2, 1);
    hd.hitGivePower = intParam(c, QStringLiteral("givepower"), hd.hitDamage, 0);
    hd.guardGivePower = intParam(c, QStringLiteral("givepower"), hd.hitGivePower / 2, 1);
    hd.p1SprPriority = intParam(c, QStringLiteral("p1sprpriority"), 1);
    hd.p2SprPriority = intParam(c, QStringLiteral("p2sprpriority"), 0);
    hd.id = intParam(c, QStringLiteral("id"), 0);
    hd.guardDist = intParam(c, QStringLiteral("guard.dist"), -1);
    hd.fallEnvShakeTime = intParam(c, QStringLiteral("fall.envshake.time"), 0);
    hd.fallEnvShakeFreq = floatParam(c, QStringLiteral("fall.envshake.freq"), 60);
    hd.fallEnvShakeAmpl = intParam(c, QStringLiteral("fall.envshake.ampl"), -4);
    hd.fallEnvShakePhase = floatParam(c, QStringLiteral("fall.envshake.phase"), hd.fallEnvShakeFreq >= 90 ? 90 : 0);
    hd.chainId = intParam(c, QStringLiteral("chainid"), -1);
    hd.priority = intParam(c, QStringLiteral("priority"), 4, 0);
    if (const Param* priority = c.param(QStringLiteral("priority"))) {
        const QString kind = priority->tail;
        hd.priorityType = kind == QLatin1String("miss")    ? PriorityType::Miss
                          : kind == QLatin1String("dodge") ? PriorityType::Dodge
                                                           : PriorityType::Hit;
    }
    hd.p2GetP1State = intParam(c, QStringLiteral("p2getp1state"), 1) != 0;
    hd.p1Facing = intParam(c, QStringLiteral("p1facing"), 0);
    hd.p2Facing = intParam(c, QStringLiteral("p2facing"), 0);
    hd.active = true;
    return hd;
}

} // namespace OFS::Mugen::Runtime
