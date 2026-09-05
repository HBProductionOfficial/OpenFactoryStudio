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
        QStringLiteral("hitadd"),
        QStringLiteral("hitby"),
        QStringLiteral("hitdef"),
        QStringLiteral("hitfalldamage"),
        QStringLiteral("hitfallset"),
        QStringLiteral("hitfallvel"),
        QStringLiteral("hitoverride"),
        QStringLiteral("hitvelset"),
        QStringLiteral("makedust"),
        QStringLiteral("modifyexplod"),
        QStringLiteral("movehitreset"),
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
    for (Character* c : std::as_const(m_characters))
        c->tick();
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
    m_commands.update(m_input, m_facing);
    for (int special : {-3, -2, -1}) {
        if (!m_data.state(special))
            continue;
        m_minus = special;
        runState(special, m_specialCounters[special]);
    }
    m_minus = 0;
    if (m_pending)
        enterState();
    runState(m_stateNo, m_counters);

    updatePosition();
    if (m_physics == QLatin1Char('A') && m_vel.y > 0 && m_pos.y >= 0 && m_stateNo != 105)
        changeState(52);
    ++m_time;
    m_anim.step();
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
    case Trigger::HitOver:
        return Value::boolean(true);
    case Trigger::IsHelper:
    case Trigger::NumHelper:
    case Trigger::NumExplod:
    case Trigger::NumTarget:
    case Trigger::NumProj:
    case Trigger::NumProjID:
    case Trigger::NumPartner:
    case Trigger::MatchOver:
    case Trigger::HitCount:
    case Trigger::UniqHitCount:
    case Trigger::MoveContact:
    case Trigger::MoveHit:
    case Trigger::MoveGuarded:
    case Trigger::MoveReversed:
    case Trigger::HitFall:
    case Trigger::CanRecover:
    case Trigger::InGuardDist:
    case Trigger::HitPauseTime:
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
