#include "engines/mugen/runtime/Character.h"

#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <climits>
#include <cmath>

namespace OFS::Mugen::Runtime {

namespace {

constexpr int MaxLoop = 2500;
constexpr int Never = INT32_MAX;

int s_nextId = 56; // player IDs start where the engine starts them

QChar letterOf(const Param* param, QChar fallback)
{
    if (!param || param->text.trimmed().isEmpty())
        return fallback;
    return param->text.trimmed().at(0).toUpper();
}

const QSet<QString>& pendingControllers()
{
    static const QSet<QString> names = {
        QStringLiteral("afterimage"),
        QStringLiteral("afterimagetime"),
        QStringLiteral("allpalfx"),
        QStringLiteral("angleadd"),
        QStringLiteral("angledraw"),
        QStringLiteral("anglemul"),
        QStringLiteral("angleset"),
        QStringLiteral("assertspecial"),
        QStringLiteral("attackdist"),
        QStringLiteral("attackmulset"),
        QStringLiteral("bgpalfx"),
        QStringLiteral("bindtoparent"),
        QStringLiteral("bindtoroot"),
        QStringLiteral("bindtotarget"),
        QStringLiteral("clearclipboard"),
        QStringLiteral("defencemulset"),
        QStringLiteral("destroyself"),
        QStringLiteral("envcolor"),
        QStringLiteral("envshake"),
        QStringLiteral("explod"),
        QStringLiteral("explodbindtime"),
        QStringLiteral("fallenvshake"),
        QStringLiteral("forcefeedback"),
        QStringLiteral("gamemakeanim"),
        QStringLiteral("helper"),
        QStringLiteral("hitby"),
        QStringLiteral("hitoverride"),
        QStringLiteral("makedust"),
        QStringLiteral("modifyexplod"),
        QStringLiteral("nothitby"),
        QStringLiteral("offset"),
        QStringLiteral("palfx"),
        QStringLiteral("parentvaradd"),
        QStringLiteral("parentvarset"),
        QStringLiteral("pause"),
        QStringLiteral("playerpush"),
        QStringLiteral("playsnd"),
        QStringLiteral("projectile"),
        QStringLiteral("removeexplod"),
        QStringLiteral("reversaldef"),
        QStringLiteral("screenbound"),
        QStringLiteral("sndpan"),
        QStringLiteral("stopsnd"),
        QStringLiteral("superpause"),
        QStringLiteral("targetbind"),
        QStringLiteral("targetdrop"),
        QStringLiteral("targetfacing"),
        QStringLiteral("targetlifeadd"),
        QStringLiteral("targetpoweradd"),
        QStringLiteral("targetstate"),
        QStringLiteral("targetveladd"),
        QStringLiteral("targetvelset"),
        QStringLiteral("trans"),
        QStringLiteral("victoryquote"),
        QStringLiteral("width"),
        QStringLiteral("appendtoclipboard"),
        QStringLiteral("displaytoclipboard"),
        QStringLiteral("remappal"),
    };
    return names;
}

} // namespace

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
    m_seed ^= m_seed << 13;
    m_seed ^= m_seed >> 17;
    m_seed ^= m_seed << 5;
    return max > 0 ? int(m_seed % std::uint32_t(max)) : 0;
}

void Match::tick()
{
    for (Character* c : std::as_const(m_characters)) {
        bool near = false;
        for (const Character* other : std::as_const(m_characters)) {
            if (other == c || other->moveType() != QLatin1Char('A'))
                continue;
            const float reach = other->data().constant(QStringLiteral("size.attack.dist"), 160);
            near = near || std::abs(other->pos().x() - c->pos().x()) <= reach;
        }
        c->setInGuardDist(near);
    }
    for (Character* c : std::as_const(m_characters))
        c->runActions();
    for (Character* attacker : std::as_const(m_characters)) {
        for (Character* target : std::as_const(m_characters)) {
            if (attacker != target)
                attacker->attack(*target);
        }
    }
    for (Character* c : std::as_const(m_characters))
        c->finishTick();
    ++m_time;
}

Character::Character(const CharacterData& data, Match* match)
    : m_data(data)
    , m_match(match)
    , m_id(s_nextId++)
{
    m_life = int(data.constant(QStringLiteral("data.life"), 1000));
    m_commands = data.commands();
    if (match)
        match->add(this);
}

int Character::var(int index) const
{
    return index >= 0 && index < int(m_vars.size()) ? m_vars[std::size_t(index)] : 0;
}

float Character::fvar(int index) const
{
    return index >= 0 && index < int(m_fvars.size()) ? m_fvars[std::size_t(index)] : 0;
}

void Character::start(int state)
{
    m_nesting = 0;
    m_prevStateNo = m_stateNo = std::max(0, state);
    m_time = 0;
    const StateDef* def = m_data.state(m_stateNo);
    m_counters.assign(def ? def->controllers.size() : 0, 0);
    m_pending = true; // entered at the next tick
}

void Character::changeAnim(int number)
{
    if (number < 0) {
        m_messages << QStringLiteral("attempted change to negative anim %1").arg(number);
        number = 0;
    }
    const AirAnimation* animation = m_data.animation(number);
    if (!animation) {
        m_messages << QStringLiteral("attempted to change to invalid action %1").arg(number);
        return;
    }
    QList<int> times;
    for (const AirFrame& frame : animation->frames)
        times << frame.time;
    m_anim = AnimPlayer(times, animation->loopStartFrame());
    m_animNo = number;
}

void Character::changeState(int number, int anim, int ctrl)
{
    if (anim != -1)
        changeAnim(anim);
    if (ctrl >= 0)
        m_ctrl = ctrl != 0;
    if (m_nesting >= MaxLoop) {
        m_messages << QStringLiteral("state machine stuck in loop (stopped after %1 loops): %2 -> %3")
                          .arg(m_nesting)
                          .arg(m_stateNo)
                          .arg(number);
        return;
    }
    m_prevStateNo = m_stateNo;
    if (number < 0)
        m_messages << QStringLiteral("attempted to change to negative state %1").arg(number);
    m_stateNo = std::max(0, number);
    m_time = 0;
    const StateDef* def = m_data.state(m_stateNo);
    if (!def)
        m_messages << QStringLiteral("changed to invalid state %1 (from state %2)").arg(number).arg(m_prevStateNo);
    m_counters.assign(def ? def->controllers.size() : 0, 0);
    m_pending = true;
    // A state change made by the current state runs the new state at once.
    if (m_nesting == 0 && m_minus == 0) {
        while (m_pending && m_nesting < MaxLoop) {
            enterState();
            ++m_nesting;
            if (!runState(m_stateNo, m_counters))
                break;
        }
        m_nesting = 0;
    }
}

void Character::enterState()
{
    m_pending = false;
    const StateDef* def = m_data.state(m_stateNo);
    if (!def)
        return;
    // Left out, type is S, move type I and physics N. U keeps them.
    const QChar type = letterOf(def->param(QStringLiteral("type")), QLatin1Char('S'));
    const QChar move = letterOf(def->param(QStringLiteral("movetype")), QLatin1Char('I'));
    const QChar physics = letterOf(def->param(QStringLiteral("physics")), QLatin1Char('N'));
    if (type != QLatin1Char('U'))
        m_stateType = type;
    if (move != QLatin1Char('U'))
        m_moveType = move;
    if (physics != QLatin1Char('U'))
        m_physics = physics;
    if (const Param* anim = def->param(QStringLiteral("anim")); anim && anim->isExpression())
        changeAnim(eval(anim).toInt());
    if (const Param* velset = def->param(QStringLiteral("velset")); velset && velset->isExpression()) {
        m_vel.x = eval(velset, 0).toFloat();
        if (velset->values.size() > 1)
            m_vel.y = eval(velset, 1).toFloat();
    }
    if (const Param* ctrl = def->param(QStringLiteral("ctrl")); ctrl && ctrl->isExpression())
        m_ctrl = eval(ctrl).toBool();
    if (const Param* power = def->param(QStringLiteral("poweradd")); power && power->isExpression())
        m_power =
            std::clamp(m_power + eval(power).toInt(), 0, int(m_data.constant(QStringLiteral("data.power"), 3000)));
    if (const Param* priority = def->param(QStringLiteral("sprpriority")); priority && priority->isExpression())
        m_sprPriority = eval(priority).toInt();
    auto keep = [this, def](const char* name) {
        const Param* p = def->param(QLatin1String(name));
        return p && p->isExpression() && eval(p).toBool();
    };
    if (!keep("hitdefpersist"))
        m_hitDef.active = false;
    if (!keep("movehitpersist")) {
        m_moveContactTime = 0;
        m_moveGuarded = false;
    }
    if (!keep("hitcountpersist")) {
        m_hitCount = 0;
        m_uniqHitCount = 0;
    }
}

bool Character::triggered(const Controller& controller)
{
    if (controller.triggers.empty())
        return false;
    for (const Expr& e : controller.triggerAll) {
        if (!e.evaluate(*this).toBool())
            return false;
    }
    for (const std::vector<Expr>& group : controller.triggers) {
        bool all = true;
        for (const Expr& e : group) {
            if (!e.evaluate(*this).toBool()) {
                all = false;
                break;
            }
        }
        if (all)
            return true;
    }
    return false;
}

bool Character::runState(int number, std::vector<int>& counters)
{
    const StateDef* def = m_data.state(number);
    if (!def)
        return false;
    if (counters.size() != def->controllers.size())
        counters.assign(def->controllers.size(), 0);
    for (std::size_t i = 0; i < def->controllers.size(); ++i) {
        const Controller& controller = def->controllers[i];
        if (m_hitPauseTime > 0 && !controller.ignoreHitPause)
            continue;
        // persistent = n runs every n times it triggers. 0 runs once per state.
        if (counters[i] != Never)
            --counters[i];
        if (counters[i] > 0)
            continue;
        if (!triggered(controller))
            continue;
        if (execute(controller))
            return true;
        counters[i] = controller.persistent == 0 ? Never : controller.persistent;
    }
    return false;
}

void Character::tick()
{
    runActions();
    finishTick();
}

void Character::runActions()
{
    m_commands.update(m_input, m_facing);
    const bool paused = m_hitPauseTime > 0;
    for (int special : {-3, -2, -1}) {
        if (!m_data.state(special))
            continue;
        m_minus = special;
        runState(special, m_specialCounters[special]);
    }
    m_minus = 0;
    if (m_pending && !paused)
        enterState();
    runState(m_stateNo, m_counters);

    // The engine guards for a player who holds back while an attack comes.
    const bool guardState = m_stateNo >= 120 && m_stateNo <= 155;
    if (!paused && m_ctrl && m_moveType == QLatin1Char('I') && !guardState && m_inGuardDist &&
        m_commands.holdingBack() && m_data.state(120) &&
        (m_stateType == QLatin1Char('S') || m_stateType == QLatin1Char('C') || m_stateType == QLatin1Char('A')))
        changeState(120);

    if (paused) {
        --m_hitPauseTime;
        return;
    }
    if (!m_posFreeze)
        updatePosition();
    m_posFreeze = false;
    if (m_physics == QLatin1Char('A') && m_vel.y > 0 && m_pos.y >= 0 && m_stateNo != 105)
        changeState(52);
    ++m_time;
    if (m_moveContactTime > 0)
        ++m_moveContactTime;
    if (m_moveType == QLatin1Char('H')) {
        if (m_ghv.hitShakeTime > 0)
            --m_ghv.hitShakeTime;
        if (m_ghv.fall)
            ++m_ghv.fallTime;
    } else {
        m_ghv.hitShakeTime = 0;
        m_ghv.fall = false;
        m_ghv.fallCount = 0;
    }
    if (m_ghv.hitShakeTime <= 0 && m_ghv.hitTime >= 0)
        --m_ghv.hitTime;
}

void Character::finishTick()
{
    if (m_gotHit) {
        m_gotHit = false;
        enterGetHitState();
    }
    if (m_pendingP1State >= 0) {
        const int state = m_pendingP1State;
        m_pendingP1State = -1;
        changeState(state);
    }
    if (m_hitPauseTime <= 0)
        m_anim.step();
}

void Character::enterGetHitState()
{
    m_moveType = QLatin1Char('H');
    if (m_hitStateNo >= 0) {
        const int state = m_hitStateNo;
        m_hitStateNo = -1;
        changeState(state, -1, 0);
        m_moveType = QLatin1Char('H');
        return;
    }
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
    QList<QRectF> rects;
    const AirAnimation* animation = m_data.animation(m_animNo);
    if (!animation || m_anim.isEmpty())
        return rects;
    for (const AirBox& box : animation->boxes(m_anim.frame(), type)) {
        float left = float(std::min(box.left, box.right));
        float right = float(std::max(box.left, box.right));
        if (m_facing < 0) {
            const float l = -right;
            right = -left;
            left = l;
        }
        const float top = float(std::min(box.top, box.bottom));
        const float bottom = float(std::max(box.top, box.bottom));
        rects << QRectF(m_pos.x + left, m_pos.y + top, right - left, bottom - top);
    }
    return rects;
}

bool Character::canGuard(const HitDef& hit) const
{
    const bool guardState = m_stateNo >= 120 && m_stateNo <= 155;
    if (!m_commands.holdingBack())
        return false;
    if (!(((m_ctrl || m_stateNo == 52) && m_moveType == QLatin1Char('I')) || guardState))
        return false;
    if (m_moveType == QLatin1Char('H') && !m_ghv.guarded)
        return false;
    const QString flags = hit.guardFlag.toUpper();
    const bool mid = flags.contains(QLatin1Char('M'));
    return (m_stateType == QLatin1Char('S') && (mid || flags.contains(QLatin1Char('H')))) ||
           (m_stateType == QLatin1Char('C') && (mid || flags.contains(QLatin1Char('L')))) ||
           (m_stateType == QLatin1Char('A') && flags.contains(QLatin1Char('A')));
}

bool Character::attack(Character& target)
{
    if (!m_hitDef.active || &target == this)
        return false;
    const QString flags = m_hitDef.hitFlag.toUpper();
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
    if (flags.contains(QLatin1Char('-')) && target.m_moveType == QLatin1Char('H'))
        allowed = false;
    if (!allowed)
        return false;
    bool contact = false;
    const QList<QRectF> body = target.boxes(2);
    for (const QRectF& hit : boxes(1)) {
        for (const QRectF& part : body)
            contact = contact || hit.intersects(part);
    }
    if (!contact)
        return false;

    const HitDef& hd = m_hitDef;
    const bool guarded = target.canGuard(hd);
    const float attackMul = m_data.constant(QStringLiteral("data.attack"), 100) / 100.0f;
    const float defence = target.m_data.constant(QStringLiteral("data.defence"), 100) / 100.0f;
    int damage = int(std::lround((guarded ? hd.guardDamage : hd.hitDamage) * attackMul / std::max(0.01f, defence)));
    const bool kill = guarded ? hd.guardKill : hd.kill;
    if (!kill && damage >= target.m_life)
        damage = std::max(0, target.m_life - 1);
    target.m_life = std::max(0, target.m_life - damage);
    const int maxPower = int(m_data.constant(QStringLiteral("data.power"), 3000));
    m_power = std::clamp(m_power + (guarded ? hd.guardGetPower : hd.hitGetPower), 0, maxPower);
    const int targetMaxPower = int(target.m_data.constant(QStringLiteral("data.power"), 3000));
    target.m_power = std::clamp(target.m_power + (guarded ? hd.guardGivePower : hd.hitGivePower), 0, targetMaxPower);

    GetHitVars& ghv = target.m_ghv;
    const bool combo = target.m_moveType == QLatin1Char('H') && !ghv.guarded;
    const float side = float(-m_facing);
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
                ghv.yvel = -0.001f;
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
    target.m_ctrl = false;

    const int pause = guarded ? hd.guardPauseTime[0] : hd.pauseTime[0];
    if (pause > 0)
        m_hitPauseTime = pause;
    m_moveContactTime = 1;
    m_moveGuarded = guarded;
    if (!guarded) {
        m_hitCount += hd.numHits;
        ++m_uniqHitCount;
        if (hd.p1StateNo >= 0)
            m_pendingP1State = hd.p1StateNo;
    }
    m_hitDef.active = false;
    return true;
}

void Character::readHitDef(const Controller& c)
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
    hd.chainId = intParam(c, QStringLiteral("chainid"), -1);
    hd.active = true;
    m_hitDef = hd;
}

void Character::updatePosition()
{
    m_pos.x += m_vel.x * float(m_facing);
    m_pos.y += m_vel.y;
    if (m_physics == QLatin1Char('S')) {
        m_vel.x *= m_data.constant(QStringLiteral("movement.stand.friction"), 0.85f);
        if (std::abs(m_vel.x) < 1)
            m_vel.x = 0;
    } else if (m_physics == QLatin1Char('C')) {
        m_vel.x *= m_data.constant(QStringLiteral("movement.crouch.friction"), 0.82f);
    } else if (m_physics == QLatin1Char('A')) {
        m_vel.y += m_data.constant(QStringLiteral("movement.yaccel"), 0.44f);
    }
}

Value Character::eval(const Param* param, int index)
{
    if (!param || index < 0 || index >= int(param->values.size()))
        return Value::undefined();
    return param->values[std::size_t(index)].evaluate(*this);
}

int Character::intParam(const Controller& c, const QString& name, int fallback, int index)
{
    const Param* param = c.param(name);
    if (!param || index >= int(param->values.size()))
        return fallback;
    return eval(param, index).toInt();
}

float Character::floatParam(const Controller& c, const QString& name, float fallback, int index)
{
    const Param* param = c.param(name);
    if (!param || index >= int(param->values.size()))
        return fallback;
    return eval(param, index).toFloat();
}

bool Character::execute(const Controller& c)
{
    if (m_tracing)
        m_trace << QStringLiteral("%1: %2").arg(m_stateNo).arg(c.label.isEmpty() ? c.type : c.label);
    const QString& type = c.type;
    if (type == QLatin1String("null"))
        return false;
    if (type == QLatin1String("changestate") || type == QLatin1String("selfstate")) {
        changeState(intParam(c, QStringLiteral("value"), m_stateNo), intParam(c, QStringLiteral("anim"), -1),
                    intParam(c, QStringLiteral("ctrl"), -1));
        return true;
    }
    if (type == QLatin1String("changeanim") || type == QLatin1String("changeanim2")) {
        changeAnim(intParam(c, QStringLiteral("value"), m_animNo));
        if (has(c, QStringLiteral("elem")))
            m_anim.setElement(intParam(c, QStringLiteral("elem"), 1), intParam(c, QStringLiteral("elemtime"), 0));
        return false;
    }
    if (type == QLatin1String("velset") || type == QLatin1String("veladd") || type == QLatin1String("velmul")) {
        const bool add = type == QLatin1String("veladd");
        const bool mul = type == QLatin1String("velmul");
        if (has(c, QStringLiteral("x"))) {
            const float x = floatParam(c, QStringLiteral("x"), 0);
            m_vel.x = add ? m_vel.x + x : mul ? m_vel.x * x : x;
        }
        if (has(c, QStringLiteral("y"))) {
            const float y = floatParam(c, QStringLiteral("y"), 0);
            m_vel.y = add ? m_vel.y + y : mul ? m_vel.y * y : y;
        }
        return false;
    }
    if (type == QLatin1String("posset")) {
        if (has(c, QStringLiteral("x")))
            m_pos.x = floatParam(c, QStringLiteral("x"), 0);
        if (has(c, QStringLiteral("y")))
            m_pos.y = floatParam(c, QStringLiteral("y"), 0);
        return false;
    }
    if (type == QLatin1String("posadd")) {
        m_pos.x += floatParam(c, QStringLiteral("x"), 0) * float(m_facing);
        m_pos.y += floatParam(c, QStringLiteral("y"), 0);
        return false;
    }
    if (type == QLatin1String("ctrlset")) {
        m_ctrl = intParam(c, QStringLiteral("value"), 0) != 0;
        return false;
    }
    if (type == QLatin1String("turn")) {
        m_facing = -m_facing;
        return false;
    }
    if (type == QLatin1String("statetypeset")) {
        for (const auto& [key, field] :
             {std::pair{QStringLiteral("statetype"), &m_stateType}, std::pair{QStringLiteral("movetype"), &m_moveType},
              std::pair{QStringLiteral("physics"), &m_physics}}) {
            if (const Param* p = c.param(key))
                *field = letterOf(p, *field);
        }
        return false;
    }
    if (type == QLatin1String("poweradd") || type == QLatin1String("powerset")) {
        const int value = intParam(c, QStringLiteral("value"), 0);
        const int max = int(m_data.constant(QStringLiteral("data.power"), 3000));
        m_power = std::clamp(type == QLatin1String("poweradd") ? m_power + value : value, 0, max);
        return false;
    }
    if (type == QLatin1String("lifeset")) {
        m_life = std::clamp(intParam(c, QStringLiteral("value"), m_life), 0,
                            int(m_data.constant(QStringLiteral("data.life"), 1000)));
        return false;
    }
    if (type == QLatin1String("sprpriority")) {
        m_sprPriority = intParam(c, QStringLiteral("value"), 0);
        return false;
    }
    if (type == QLatin1String("gravity")) {
        m_vel.y += m_data.constant(QStringLiteral("movement.yaccel"), 0.44f);
        return false;
    }
    if (type == QLatin1String("varset") || type == QLatin1String("varadd")) {
        const bool add = type == QLatin1String("varadd");
        auto apply = [this, add](Trigger kind, int index, const Value& value) {
            Value current = Value::integer(0);
            if (add) {
                TriggerCall call;
                call.trigger = kind;
                const Value i = Value::integer(index);
                call.args = std::span<const Value>(&i, 1);
                current = trigger(call);
                assign(kind, index, Ops::add(current, value));
            } else {
                assign(kind, index, value);
            }
        };
        if (has(c, QStringLiteral("v")))
            apply(Trigger::Var, intParam(c, QStringLiteral("v"), 0), eval(c.param(QStringLiteral("value"))));
        if (has(c, QStringLiteral("fv")))
            apply(Trigger::FVar, intParam(c, QStringLiteral("fv"), 0), eval(c.param(QStringLiteral("value"))));
        static const QRegularExpression indexed(QStringLiteral("^(var|fvar|sysvar|sysfvar)\\((\\d+)\\)$"));
        for (auto it = c.params.cbegin(); it != c.params.cend(); ++it) {
            const QRegularExpressionMatch m = indexed.match(it.key());
            if (!m.hasMatch())
                continue;
            const QString kind = m.captured(1);
            const Trigger t = kind == QLatin1String("var")      ? Trigger::Var
                              : kind == QLatin1String("fvar")   ? Trigger::FVar
                              : kind == QLatin1String("sysvar") ? Trigger::SysVar
                                                                : Trigger::SysFVar;
            apply(t, m.captured(2).toInt(), eval(&it.value()));
        }
        return false;
    }
    if (type == QLatin1String("varrandom")) {
        const int index = intParam(c, QStringLiteral("v"), 0);
        int low = 0;
        int high = 1000;
        if (const Param* range = c.param(QStringLiteral("range"))) {
            if (range->values.size() == 1) {
                high = eval(range, 0).toInt();
            } else if (range->values.size() > 1) {
                low = eval(range, 0).toInt();
                high = eval(range, 1).toInt();
            }
        }
        const int value = low + (m_match ? m_match->random(std::max(1, high - low + 1)) : 0);
        assign(Trigger::Var, index, Value::integer(value));
        return false;
    }
    if (type == QLatin1String("varrangeset")) {
        const int first = intParam(c, QStringLiteral("first"), 0);
        const bool floats = has(c, QStringLiteral("fvalue"));
        const int last = intParam(c, QStringLiteral("last"), floats ? 39 : 59);
        const Value value = eval(c.param(floats ? QStringLiteral("fvalue") : QStringLiteral("value")));
        for (int i = first; i <= last; ++i)
            assign(floats ? Trigger::FVar : Trigger::Var, i, value);
        return false;
    }
    if (type == QLatin1String("hitdef")) {
        readHitDef(c);
        return false;
    }
    if (type == QLatin1String("hitvelset")) {
        if (intParam(c, QStringLiteral("x"), 1) != 0)
            m_vel.x = m_ghv.xvel * float(m_facing);
        if (intParam(c, QStringLiteral("y"), 1) != 0)
            m_vel.y = m_ghv.yvel;
        return false;
    }
    if (type == QLatin1String("hitfallvel")) {
        if (!std::isnan(m_ghv.fallXVelocity))
            m_vel.x = m_ghv.fallXVelocity;
        m_vel.y = m_ghv.fallYVelocity;
        return false;
    }
    if (type == QLatin1String("hitfallset")) {
        const int value = intParam(c, QStringLiteral("value"), -1);
        if (value >= 0)
            m_ghv.fall = value != 0;
        if (has(c, QStringLiteral("xvel")))
            m_ghv.fallXVelocity = floatParam(c, QStringLiteral("xvel"), 0);
        if (has(c, QStringLiteral("yvel")))
            m_ghv.fallYVelocity = floatParam(c, QStringLiteral("yvel"), 0);
        return false;
    }
    if (type == QLatin1String("hitfalldamage")) {
        int life = m_life - m_ghv.fallDamage;
        if (!m_ghv.fallKill)
            life = std::max(life, std::min(1, m_life));
        m_life = std::max(0, life);
        return false;
    }
    if (type == QLatin1String("posfreeze")) {
        m_posFreeze = intParam(c, QStringLiteral("value"), 1) != 0;
        return false;
    }
    if (type == QLatin1String("movehitreset")) {
        m_moveContactTime = 0;
        m_moveGuarded = false;
        return false;
    }
    if (type == QLatin1String("hitadd")) {
        m_hitCount += intParam(c, QStringLiteral("value"), 0);
        return false;
    }
    if (type == QLatin1String("lifeadd")) {
        const int max = int(m_data.constant(QStringLiteral("data.life"), 1000));
        int life = m_life + intParam(c, QStringLiteral("value"), 0);
        if (intParam(c, QStringLiteral("kill"), 1) == 0)
            life = std::max(life, std::min(1, m_life));
        m_life = std::clamp(life, 0, max);
        return false;
    }
    if (!pendingControllers().contains(type)) {
        const QString message =
            QStringLiteral("unknown controller type '%1' (%2, line %3)").arg(type, c.file).arg(c.line + 1);
        if (!m_messages.contains(message))
            m_messages << message;
    }
    return false;
}

Value Character::trigger(const TriggerCall& call)
{
    auto arg = [&call](int i) {
        return i < int(call.args.size()) ? call.args[std::size_t(i)] : Value::undefined();
    };
    auto special = [&call](bool match) {
        return Value::boolean(match != call.negate);
    };
    switch (call.trigger) {
    case Trigger::Time:
        return Value::integer(m_time);
    case Trigger::StateNo:
        return Value::integer(m_stateNo);
    case Trigger::PrevStateNo:
        return Value::integer(m_prevStateNo);
    case Trigger::Anim:
        return Value::integer(m_animNo);
    case Trigger::AnimTime:
        return Value::integer(m_anim.animTime());
    case Trigger::AnimElemTime:
        return Value::integer(m_anim.animElemTime(arg(0).toInt()));
    case Trigger::AnimElemNo:
        return Value::integer(m_anim.animElemNo(arg(0).toInt()));
    case Trigger::AnimElem: {
        const int element = arg(0).toInt();
        if (element < 1 || element > m_anim.frameCount())
            return special(false);
        const Value time = Value::integer(m_anim.animElemTime(element));
        if (!call.hasSecond)
            return special(time.i == 0);
        return special(Ops::compare(time, arg(1), call.second).toBool());
    }
    case Trigger::AnimExist:
    case Trigger::SelfAnimExist:
        return Value::boolean(m_data.animation(arg(0).toInt()) != nullptr);
    case Trigger::Ctrl:
        return Value::boolean(m_ctrl);
    case Trigger::StateType:
        return special(call.keyword.startsWith(m_stateType));
    case Trigger::MoveType:
        return special(call.keyword.startsWith(m_moveType));
    case Trigger::TimeMod: {
        const int divisor = arg(0).toInt();
        if (divisor <= 0)
            return Value::undefined();
        return special(m_time % divisor == arg(1).toInt());
    }
    case Trigger::Pos:
        return Value::real(call.keyword == QLatin1String("y") ? m_pos.y : m_pos.x);
    case Trigger::Vel:
        return Value::real(call.keyword == QLatin1String("y") ? m_vel.y : m_vel.x);
    case Trigger::Facing:
        return Value::integer(m_facing);
    case Trigger::Var:
        return Value::integer(var(arg(0).toInt()));
    case Trigger::FVar:
        return Value::real(fvar(arg(0).toInt()));
    case Trigger::SysVar: {
        const int i = arg(0).toInt();
        return Value::integer(i >= 0 && i < int(m_sysVars.size()) ? m_sysVars[std::size_t(i)] : 0);
    }
    case Trigger::SysFVar: {
        const int i = arg(0).toInt();
        return Value::real(i >= 0 && i < int(m_sysFVars.size()) ? m_sysFVars[std::size_t(i)] : 0);
    }
    case Trigger::Const: {
        if (!m_data.hasConstant(call.keyword))
            return Value::undefined();
        const float value = m_data.constant(call.keyword);
        if (call.keyword.startsWith(QLatin1String("data.")) || call.keyword == QLatin1String("movement.airjump.num"))
            return Value::integer(int(value));
        return Value::real(value);
    }
    case Trigger::Life:
        return Value::integer(m_life);
    case Trigger::LifeMax:
        return Value::integer(int(m_data.constant(QStringLiteral("data.life"), 1000)));
    case Trigger::Power:
        return Value::integer(m_power);
    case Trigger::PowerMax:
        return Value::integer(int(m_data.constant(QStringLiteral("data.power"), 3000)));
    case Trigger::Alive:
        return Value::boolean(m_life > 0);
    case Trigger::GameTime:
        return Value::integer(m_match ? m_match->gameTime() : 0);
    case Trigger::Random:
        return Value::integer(m_match ? m_match->random(1000) : 0);
    case Trigger::ID:
        return Value::integer(m_id);
    case Trigger::RoundState:
        return Value::integer(2);
    case Trigger::RoundNo:
    case Trigger::MatchNo:
    case Trigger::PalNo:
    case Trigger::TeamSide:
        return Value::integer(1);
    case Trigger::TicksPerSecond:
        return Value::integer(60);
    case Trigger::HitShakeOver:
        return Value::boolean(m_ghv.hitShakeTime <= 0);
    case Trigger::HitOver:
        return Value::boolean(m_ghv.hitTime < 0);
    case Trigger::HitFall:
        return Value::boolean(m_ghv.fall);
    case Trigger::CanRecover:
        return Value::boolean(m_ghv.fall && m_ghv.fallRecover && m_ghv.fallTime >= m_ghv.fallRecoverTime);
    case Trigger::MoveContact:
        return Value::integer(m_moveContactTime);
    case Trigger::MoveHit:
        return Value::integer(m_moveGuarded ? 0 : m_moveContactTime);
    case Trigger::MoveGuarded:
        return Value::integer(m_moveGuarded ? m_moveContactTime : 0);
    case Trigger::HitCount:
        return Value::integer(m_hitCount);
    case Trigger::UniqHitCount:
        return Value::integer(m_uniqHitCount);
    case Trigger::HitPauseTime:
        return Value::integer(m_hitPauseTime);
    case Trigger::InGuardDist:
        return Value::boolean(m_inGuardDist);
    case Trigger::HitDefAttr: {
        if (!m_hitDef.active)
            return special(false);
        bool states = false;
        for (QChar s : call.keyword)
            states = states || m_hitDef.attrStates.contains(s);
        bool classes = call.list.isEmpty();
        for (const QString& cls : call.list)
            classes = classes || m_hitDef.attrClasses.contains(cls);
        return special(states && classes);
    }
    case Trigger::GetHitVar: {
        const QString& key = call.keyword;
        if (key == QLatin1String("xvel"))
            return Value::real(m_ghv.xvel * float(m_facing));
        if (key == QLatin1String("yvel"))
            return Value::real(m_ghv.yvel);
        if (key == QLatin1String("yaccel"))
            return Value::real(m_ghv.yAccel);
        if (key == QLatin1String("type"))
            return Value::integer(int(m_ghv.type));
        if (key == QLatin1String("animtype"))
            return Value::integer(int(m_ghv.animType));
        if (key == QLatin1String("airtype"))
            return Value::integer(int(m_ghv.airType));
        if (key == QLatin1String("groundtype"))
            return Value::integer(int(m_ghv.groundType));
        if (key == QLatin1String("damage"))
            return Value::integer(m_ghv.damage);
        if (key == QLatin1String("hitcount"))
            return Value::integer(m_ghv.hitCount);
        if (key == QLatin1String("fallcount"))
            return Value::integer(m_ghv.fallCount);
        if (key == QLatin1String("hitshaketime"))
            return Value::integer(m_ghv.hitShakeTime);
        if (key == QLatin1String("hittime"))
            return Value::integer(m_ghv.hitTime);
        if (key == QLatin1String("slidetime"))
            return Value::integer(m_ghv.slideTime);
        if (key == QLatin1String("ctrltime"))
            return Value::integer(m_ghv.ctrlTime);
        if (key == QLatin1String("recovertime"))
            return Value::integer(m_ghv.recoverTime);
        if (key == QLatin1String("guarded"))
            return Value::boolean(m_ghv.guarded);
        if (key == QLatin1String("fall"))
            return Value::boolean(m_ghv.fall);
        if (key == QLatin1String("fall.damage"))
            return Value::integer(m_ghv.fallDamage);
        if (key == QLatin1String("fall.xvel"))
            return std::isnan(m_ghv.fallXVelocity) ? Value::undefined() : Value::real(m_ghv.fallXVelocity);
        if (key == QLatin1String("fall.yvel"))
            return Value::real(m_ghv.fallYVelocity);
        if (key == QLatin1String("fall.recover"))
            return Value::boolean(m_ghv.fallRecover);
        if (key == QLatin1String("fall.time"))
            return Value::integer(m_ghv.fallTime);
        if (key == QLatin1String("fall.recovertime"))
            return Value::integer(m_ghv.fallRecoverTime);
        if (key == QLatin1String("fall.kill"))
            return Value::boolean(m_ghv.fallKill);
        if (key == QLatin1String("chainid") || key == QLatin1String("hitid"))
            return Value::integer(m_ghv.hitId);
        if (key == QLatin1String("xveladd") || key == QLatin1String("yveladd") || key == QLatin1String("xoff") ||
            key == QLatin1String("yoff") || key == QLatin1String("isbound"))
            return Value::integer(0);
        return Value::undefined();
    }
    case Trigger::IsHelper:
    case Trigger::NumHelper:
    case Trigger::NumExplod:
    case Trigger::NumTarget:
    case Trigger::NumProj:
    case Trigger::NumProjID:
    case Trigger::NumPartner:
    case Trigger::MatchOver:
    case Trigger::MoveReversed:
    case Trigger::AILevel:
    case Trigger::Win:
    case Trigger::WinKO:
    case Trigger::WinTime:
    case Trigger::WinPerfect:
    case Trigger::Lose:
    case Trigger::LoseKO:
    case Trigger::LoseTime:
    case Trigger::DrawGame:
        return Value::boolean(false);
    case Trigger::Command:
        return special(m_commands.active(call.text));
    case Trigger::NumEnemy:
        return Value::integer(m_match && m_match->enemy(this) ? 1 : 0);
    case Trigger::P2BodyDist:
    case Trigger::P2Dist: {
        const Character* enemy = m_match ? m_match->enemy(this) : nullptr;
        if (!enemy)
            return Value::undefined();
        if (call.keyword == QLatin1String("y"))
            return Value::real(enemy->m_pos.y - m_pos.y);
        return Value::real((enemy->m_pos.x - m_pos.x) * float(m_facing));
    }
    case Trigger::P2Life: {
        const Character* enemy = m_match ? m_match->enemy(this) : nullptr;
        return enemy ? Value::integer(enemy->m_life) : Value::undefined();
    }
    case Trigger::P2StateNo: {
        const Character* enemy = m_match ? m_match->enemy(this) : nullptr;
        return enemy ? Value::integer(enemy->m_stateNo) : Value::undefined();
    }
    case Trigger::P2StateType: {
        const Character* enemy = m_match ? m_match->enemy(this) : nullptr;
        return enemy ? special(call.keyword.startsWith(enemy->m_stateType)) : Value::undefined();
    }
    case Trigger::P2MoveType: {
        const Character* enemy = m_match ? m_match->enemy(this) : nullptr;
        return enemy ? special(call.keyword.startsWith(enemy->m_moveType)) : Value::undefined();
    }
    default:
        return Value::undefined();
    }
}

Context* Character::redirect(Redirect kind, const Value& argument)
{
    switch (kind) {
    case Redirect::Enemy:
    case Redirect::EnemyNear:
        return m_match ? m_match->enemy(this, argument.isUndefined() ? 0 : argument.toInt()) : nullptr;
    case Redirect::PlayerID:
        if (m_match) {
            for (Character* c : m_match->characters()) {
                if (c->m_id == argument.toInt())
                    return c;
            }
        }
        return nullptr;
    case Redirect::Root:
    case Redirect::Parent:
    case Redirect::Helper:
    case Redirect::Target:
    case Redirect::Partner:
        return nullptr;
    }
    return nullptr;
}

Value Character::assign(Trigger kind, std::int32_t index, const Value& value)
{
    switch (kind) {
    case Trigger::Var:
        if (index < 0 || index >= int(m_vars.size()))
            return Value::undefined();
        m_vars[std::size_t(index)] = value.toInt();
        return Value::integer(m_vars[std::size_t(index)]);
    case Trigger::FVar:
        if (index < 0 || index >= int(m_fvars.size()))
            return Value::undefined();
        m_fvars[std::size_t(index)] = value.toFloat();
        return Value::real(m_fvars[std::size_t(index)]);
    case Trigger::SysVar:
        if (index < 0 || index >= int(m_sysVars.size()))
            return Value::undefined();
        m_sysVars[std::size_t(index)] = value.toInt();
        return Value::integer(m_sysVars[std::size_t(index)]);
    case Trigger::SysFVar:
        if (index < 0 || index >= int(m_sysFVars.size()))
            return Value::undefined();
        m_sysFVars[std::size_t(index)] = value.toFloat();
        return Value::real(m_sysFVars[std::size_t(index)]);
    default:
        return Value::undefined();
    }
}

} // namespace OFS::Mugen::Runtime
