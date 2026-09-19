#include "engines/mugen/runtime/Character.h"
#include "engines/mugen/runtime/CharacterDetail.h"

#include <algorithm>
#include <cmath>

namespace OFS::Mugen::Runtime {

using namespace Detail;

namespace {

int s_nextId = 56; // player IDs start where the engine starts them

} // namespace

QChar Detail::letterOf(const Param* param, QChar fallback)
{
    if (!param || param->text.trimmed().isEmpty())
        return fallback;
    return param->text.trimmed().at(0).toUpper();
}

const QSet<QString>& Detail::pendingControllers()
{
    static const QSet<QString> names = {};
    return names;
}

Character::Character(const CharacterData& data, Match* match)
    : m_data(data)
    , m_stateData(&data)
    , m_animData(&data)
    , m_match(match)
    , m_id(s_nextId++)
{
    m_life = int(data.lifeMax());
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

float Character::constant(const QString& name, float fallback) const
{
    const auto it = m_constants.constFind(name);
    return it != m_constants.cend() ? *it : m_data.constant(name, fallback);
}

bool Character::explodRules11() const
{
    const ExplodRules rules = m_match ? m_match->explodRules() : ExplodRules::PlayerVersion;
    if (rules != ExplodRules::PlayerVersion)
        return rules == ExplodRules::Mugen11;
    return m_data.isMugen11();
}

void Character::setLife(int life)
{
    m_life = std::clamp(life, 0, int(m_data.lifeMax()));
}

void Character::setPower(int power)
{
    powerOwner()->m_power = std::clamp(power, 0, int(m_data.powerMax()));
}

void Character::resetForRound(Vec2 pos, int facing, bool firstRound)
{
    m_pos = pos;
    m_vel = Vec2();
    m_facing = facing;
    m_life = int(m_data.lifeMax());
    m_ko = false;
    m_posed = false;
    m_ctrl = false;
    m_hitPauseTime = 0;
    m_ghv = GetHitVars();
    m_hitDef = HitDef();
    m_gotHit = false;
    m_hitStateNo = -1;
    m_pendingP1State = -1;
    m_moveContactTime = 0;
    m_targets.clear();
    m_targetIds.clear();
    m_bindTo = nullptr;
    m_bindTime = 0;
    m_explods.clear();
    m_projectiles.clear();
    m_projContactTime = -1;
    m_draw = DrawState();
    m_stateData = &m_data;
    m_minus = 3;
    if (firstRound && m_data.animation(0))
        changeAnim(0);
    changeState(5900, -1, 0);
}

void Character::start(int state)
{
    m_nesting = 0;
    m_prevStateNo = m_stateNo = std::max(0, state);
    m_time = 0;
    m_stateData = &m_data;
    const StateDef* def = m_data.state(m_stateNo);
    m_counters.assign(def ? def->controllers.size() : 0, 0);
    m_pending = true; // entered at the next tick
}

void Character::changeAnim(int number, const CharacterData* owner)
{
    if (number < 0) {
        m_messages << QStringLiteral("attempted change to negative anim %1").arg(number);
        number = 0;
    }
    const CharacterData* from = owner ? owner : &m_data;
    const AirAnimation* animation = from->animation(number);
    if (!animation) {
        m_messages << QStringLiteral("attempted to change to invalid action %1").arg(number);
        return;
    }
    m_anim = animPlayerFor(*animation);
    m_animNo = number;
    m_animData = from;
}

void Character::queueState(int number, const CharacterData* owner)
{
    m_stateData = owner ? owner : &m_data;
    m_prevStateNo = m_stateNo;
    m_stateNo = std::max(0, number);
    m_time = 0;
    const StateDef* def = m_stateData->state(m_stateNo);
    if (!def)
        m_messages << QStringLiteral("changed to invalid state %1 (from state %2)").arg(number).arg(m_prevStateNo);
    m_counters.assign(def ? def->controllers.size() : 0, 0);
    m_pending = true;
}

QList<Character*> Character::targetsWithId(int id) const
{
    QList<Character*> list;
    for (int i = 0; i < m_targets.size(); ++i) {
        if (id < 0 || m_targetIds[i] == id)
            list << m_targets[i];
    }
    return list;
}

void Character::changeState(int number, int anim, int ctrl)
{
    // A character with control turns to its opponent when it changes state (standing or crouching).
    if (m_minus <= 0 && m_ctrl && (m_stateType == QLatin1Char('S') || m_stateType == QLatin1Char('C')))
        autoTurn();
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
    const StateDef* def = m_stateData->state(m_stateNo);
    if (!def)
        m_messages << QStringLiteral("changed to invalid state %1 (from state %2)").arg(number).arg(m_prevStateNo);
    m_counters.assign(def ? def->controllers.size() : 0, 0);
    m_pending = true;
    // A state change made by the current state runs the new state at once.
    if (m_nesting == 0 && m_minus == 0) {
        while (m_pending && m_nesting < MaxLoop) {
            enterState();
            ++m_nesting;
            if (!runState(*m_stateData, m_stateNo, m_counters))
                break;
        }
        m_nesting = 0;
    }
}

void Character::enterState()
{
    m_pending = false;
    const StateDef* def = m_stateData->state(m_stateNo);
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
        powerOwner()->m_power = std::clamp(powerOwner()->m_power + eval(power).toInt(), 0, int(m_data.powerMax()));
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
        m_moveReversedTime = 0;
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
    auto observe = [this, &controller](const std::vector<int>& lines, std::size_t i) {
        if (m_match && i < lines.size())
            m_match->observeLine(*this, controller, lines[i]);
    };
    for (std::size_t i = 0; i < controller.triggerAll.size(); ++i) {
        observe(controller.triggerAllLines, i);
        if (!controller.triggerAll[i].evaluate(*this).toBool())
            return false;
    }
    for (std::size_t g = 0; g < controller.triggers.size(); ++g) {
        const std::vector<Expr>& group = controller.triggers[g];
        const std::vector<int> none;
        const std::vector<int>& lines = g < controller.triggerLines.size() ? controller.triggerLines[g] : none;
        bool all = true;
        for (std::size_t i = 0; i < group.size(); ++i) {
            observe(lines, i);
            if (!group[i].evaluate(*this).toBool()) {
                all = false;
                break;
            }
        }
        if (all)
            return true;
    }
    return false;
}

bool Character::runState(const CharacterData& source, int number, std::vector<int>& counters)
{
    const StateDef* def = source.state(number);
    if (!def)
        return false;
    m_runningState = number;
    m_runningStateData = &source;
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
        if (m_match)
            m_match->observeLine(*this, controller, controller.line);
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

bool Character::inGuardState() const
{
    return m_stateNo == 120 || (m_stateNo >= 130 && m_stateNo <= 132) || m_stateNo == 140 ||
           (m_stateNo >= 150 && m_stateNo <= 155);
}

void Character::prepare()
{
    const InputBuffer& in = m_commands.buffer();
    const bool standing = m_stateType == QLatin1Char('S');
    if (m_ctrl && !m_isHelper) {
        if (m_guarding && m_inGuardDist && !inGuardState() && m_stateType != QLatin1Char('L') && in.back() > 0 &&
            m_data.state(120)) {
            changeState(120);
        } else if (standing && in.up() > 0) {
            if (m_stateNo != 40)
                changeState(40);
        } else if (m_stateType == QLatin1Char('A') && in.up() == 1 &&
                   m_pos.y <= -m_data.constant(QStringLiteral("movement.airjump.height"), 35) &&
                   m_airJumps < int(m_data.constant(QStringLiteral("movement.airjump.num"), 0))) {
            if (m_stateNo != 45 || m_time > 0) {
                ++m_airJumps;
                changeState(45);
            }
        } else if (standing && in.down() > 0) {
            if (m_stateNo != 10) {
                if (m_stateNo != 100)
                    m_vel.x = 0;
                changeState(10);
            }
        } else if (m_stateType == QLatin1Char('C') && in.down() <= 0) {
            if (m_stateNo != 12)
                changeState(12);
        } else if (standing && !m_asserted.contains(QStringLiteral("nowalk")) &&
                   ((in.forward() > 0) !=
                    ((!m_inGuardDist || m_asserted.contains(QStringLiteral("nostandguard"))) && in.back() > 0))) {
            if (m_stateNo != 20)
                changeState(20);
        }
    }
    // Walking stops when neither or both ways are held, with control or not.
    if (!m_isHelper && m_stateNo == 20 && (in.back() > 0) == (in.forward() > 0))
        changeState(0);
    if (m_stateType != QLatin1Char('A'))
        m_airJumps = 0;

    if (m_hitPauseTime <= 0) {
        m_push = isPlayerType();
        m_width[0] = m_width[1] = -1;
        m_screenBound = isPlayerType();
        m_moveCamera[0] = m_moveCamera[1] = isPlayerType();
        if (m_match && m_match->superPauseTime() > 0) {
            if (m_superMoveTime > 0)
                --m_superMoveTime;
        } else if (m_match && m_match->pauseTime() > 0 && m_pauseMoveTime > 0) {
            --m_pauseMoveTime;
        }
        for (HitBySlot& slot : m_hitBy) {
            if (slot.time > 0 && --slot.time == 0)
                slot = HitBySlot();
        }
        for (HitOverrideSlot& slot : m_overrides) {
            if (slot.time > 0 && --slot.time == 0)
                slot = HitOverrideSlot();
        }
    }
    m_asserted.clear();
    m_draw.offset = Vec2();
    m_draw.trans = QStringLiteral("default");
    m_draw.alpha = {256, 0};
    m_draw.angleDraw = false;
    m_draw.angleScale = Vec2{1, 1};
}

void Character::autoTurn()
{
    if (m_asserted.contains(QStringLiteral("noautoturn")))
        return;
    const Character* enemy = m_match ? m_match->enemy(this) : nullptr;
    if (!enemy || (enemy->m_pos.x - m_pos.x) * float(m_facing) >= 0)
        return;
    if (m_stateType == QLatin1Char('S') && m_animNo != 5 && m_data.animation(5))
        changeAnim(5);
    else if (m_stateType == QLatin1Char('C') && m_animNo != 6 && m_data.animation(6))
        changeAnim(6);
    m_facing = -m_facing;
}

void Character::releaseTargets()
{
    for (int i = int(m_targets.size()) - 1; i >= 0; --i) {
        if (m_targets[i]->m_moveType != QLatin1Char('H')) {
            if (m_targets[i]->m_bindTo == this) {
                m_targets[i]->m_bindTo = nullptr;
                m_targets[i]->m_bindTime = 0;
            }
            m_targets.removeAt(i);
            m_targetIds.removeAt(i);
        }
    }
}

float Character::backWidth() const
{
    if (m_width[0] >= 0)
        return m_width[0];
    return constant(
        m_stateType == QLatin1Char('A') ? QStringLiteral("size.air.back") : QStringLiteral("size.ground.back"), 15);
}

float Character::frontWidth() const
{
    if (m_width[1] >= 0)
        return m_width[1];
    return constant(
        m_stateType == QLatin1Char('A') ? QStringLiteral("size.air.front") : QStringLiteral("size.ground.front"), 16);
}

void Character::runActions()
{
    // Idle players face their opponent (before the input, so back and forward follow).
    if (!m_isHelper && m_hitPauseTime <= 0 && m_ctrl &&
        (m_stateNo == 0 || m_stateNo == 11 || m_stateNo == 20 || (m_stateNo == 52 && m_anim.animTime() == 0)))
        autoTurn();
    if (!m_isHelper)
        m_commands.update(m_input, m_facing);
    m_wasHit = false;
    const Match* match = m_match;
    m_frozen =
        match && (match->superPauseTime() > 0 ? m_superMoveTime == 0 : match->pauseTime() > 0 && m_pauseMoveTime == 0);
    if (m_unhittableTime > 0)
        --m_unhittableTime;
    if (m_frozen) {
        m_minus = 1;
        return;
    }
    prepare();
    const bool paused = m_hitPauseTime > 0;
    // Helpers run -3 and -2 as "player" helpers only, and -1 with keyctrl.
    for (int special : {-3, -2, -1}) {
        if (!m_data.state(special) || (special == -1 ? !m_keyCtrl : !isPlayerType()))
            continue;
        m_minus = special;
        runState(m_data, special, m_specialCounters[special]);
    }
    m_minus = 0;
    if (m_pending)
        enterState();
    runState(*m_stateData, m_stateNo, m_counters);

    const bool back = m_commands.buffer().back() > 0;
    m_guarding = ((((m_ctrl || m_stateNo == 52) && m_moveType == QLatin1Char('I')) || inGuardState()) && back &&
                  ((m_stateType == QLatin1Char('S') && !m_asserted.contains(QStringLiteral("nostandguard"))) ||
                   (m_stateType == QLatin1Char('C') && !m_asserted.contains(QStringLiteral("nocrouchguard"))) ||
                   (m_stateType == QLatin1Char('A') && !m_asserted.contains(QStringLiteral("noairguard")))));
    // The engine guards for a player who holds back while an attack comes.
    if (m_ctrl && m_inGuardDist && m_guarding && !inGuardState() && m_data.state(120))
        changeState(120);

    if (!paused && !m_ko && m_life <= 0 && !m_asserted.contains(QStringLiteral("noko")) &&
        !(m_match && (m_match->noKo() || m_match->isAsserted(QStringLiteral("globalnoko"))))) {
        if (m_match && !m_match->isAsserted(QStringLiteral("nokosnd"))) {
            SoundEvent scream;
            scream.playerId = m_id;
            scream.group = 11;
            m_match->addSound(scream);
        }
        m_ko = true;
        m_ctrl = false;
        if (!m_pending && !m_isHelper && m_moveType != QLatin1Char('H')) {
            m_ghv.fall = true;
            m_stateData = &m_data;
            changeState(5030, -1, 0);
            m_time = 1;
        }
    }
    if (paused) {
        m_minus = 1;
        return;
    }
    // The engine gets up a character who has lain long enough (after at least a tick in 5110).
    if (m_stateNo == 5110 && m_time >= 1 && m_ghv.downRecoverTime <= 0 && !m_ko && m_stateData->state(5120))
        changeState(5120);
    if (!m_posFreeze)
        updatePosition();
    m_posFreeze = false;
    if (m_physics == QLatin1Char('A') && m_vel.y > 0 && m_pos.y >= 0 && m_stateNo != 105)
        changeState(52);
    ++m_time;
    if (m_moveContactTime > 0)
        ++m_moveContactTime;
    if (m_moveReversedTime > 0)
        ++m_moveReversedTime;
    if (m_projContactTime >= 0)
        ++m_projContactTime;
    if (m_moveType == QLatin1Char('H')) {
        if (m_ghv.hitShakeTime > 0)
            --m_ghv.hitShakeTime;
        if (m_ghv.fall)
            ++m_ghv.fallTime;
    } else {
        m_ghv.hitShakeTime = 0;
        m_ghv.fall = false;
        m_ghv.fallCount = 0;
        m_superDefenceMul = 1;
    }
    if (m_ghv.hitShakeTime <= 0 && m_ghv.hitTime >= 0)
        --m_ghv.hitTime;
    if (m_ghv.downRecoverTime > 0 && m_stateNo == 5110)
        --m_ghv.downRecoverTime;
    m_minus = 1;
}

void Character::finishTick()
{
    m_minus = 2;
    if (!m_frozen && m_hitPauseTime <= 0)
        m_anim.step();
    if (m_gotHit) {
        m_gotHit = false;
        enterGetHitState();
        m_wasHit = !inGuardState();
    }
    if (m_pendingP1State >= 0) {
        const int state = m_pendingP1State;
        m_pendingP1State = -1;
        changeState(state);
    }
    if (!m_frozen && m_hitPauseTime > 0)
        --m_hitPauseTime;
    if (m_superDefenceMulNext != 1) {
        if (m_moveType == QLatin1Char('H'))
            m_superDefenceMul *= m_superDefenceMulNext;
        m_superDefenceMulNext = 1;
    }
    AfterImage& ai = m_draw.afterImage;
    if (ai.time != 0) {
        if (m_afterImageTicks++ % std::max(1, ai.timeGap) == 0) {
            AfterImageFrame f;
            f.pos = Vec2{m_pos.x + m_draw.offset.x, m_pos.y + m_draw.offset.y};
            f.animData = m_animData;
            f.animNo = m_animNo;
            f.frame = m_anim.frame();
            f.facing = m_facing;
            f.angleDraw = m_draw.angleDraw;
            f.angle = m_draw.angle;
            f.angleScale = m_draw.angleScale;
            m_afterImages.prepend(f);
            while (m_afterImages.size() > std::clamp(ai.length, 1, 60))
                m_afterImages.removeLast();
        }
        if (ai.time > 0)
            --ai.time;
    } else if (!m_afterImages.isEmpty()) {
        m_afterImages.clear();
        m_afterImageTicks = 0;
    }
    m_minus = 3;
}

void Character::updateBindings()
{
    // A target stays bound (GetHitVar(isbound)) until a tick passes without a TargetBind.
    for (Character* t : std::as_const(m_targets)) {
        if (t->m_bindTo != this || !t->m_bindIsTarget)
            continue;
        if (t->m_bindTime <= 0) {
            t->m_bindTo = nullptr;
            continue;
        }
        t->m_vel.x = float(t->m_facing * m_facing) * m_vel.x;
        t->m_vel.y = m_vel.y;
        t->m_pos.x = m_pos.x + float(t->m_bindFacing) * t->m_bindPos.x;
        t->m_pos.y = m_pos.y + t->m_bindPos.y;
    }
}

void Character::updateHelperBinding()
{
    if (m_bindIsTarget || !m_bindTo || m_bindTime == 0)
        return;
    const int f = m_bindTo->m_facing;
    m_pos.x = m_bindTo->m_pos.x + float(f) * m_bindPos.x;
    m_pos.y = m_bindTo->m_pos.y + m_bindPos.y;
    if (m_bindTurn != 0)
        m_facing = m_bindTurn > 0 ? f : -f;
}

void Character::tickBinding()
{
    if (m_bindTime <= 0 || !m_bindTo)
        return;
    if (!m_bindIsTarget) {
        if (!m_frozen && --m_bindTime == 0)
            m_bindTo = nullptr;
        return;
    }
    if (m_bindTo->m_gotHit) {
        m_bindTo = nullptr;
        m_bindTime = 0;
        m_minus = 2;
        m_stateData = &m_data;
        changeState(5050);
        m_minus = 3;
        return;
    }
    --m_bindTime;
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
        m_vel.y += m_data.yAccel();
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

} // namespace OFS::Mugen::Runtime
