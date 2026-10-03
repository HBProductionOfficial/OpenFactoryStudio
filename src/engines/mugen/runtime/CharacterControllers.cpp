#include "engines/mugen/runtime/Character.h"
#include "engines/mugen/runtime/CharacterDetail.h"

#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace OFS::Mugen::Runtime {

using namespace Detail;

bool Character::execute(const Controller& c)
{
    if (m_tracing)
        m_trace << QStringLiteral("%1: %2").arg(m_stateNo).arg(c.label.isEmpty() ? c.type : c.label);
    const QString& type = c.type;
    if (type == QLatin1String("null"))
        return false;
    if (type == QLatin1String("changestate") || type == QLatin1String("selfstate")) {
        if (type == QLatin1String("selfstate"))
            m_stateData = &m_data;
        changeState(intParam(c, QStringLiteral("value"), m_stateNo), intParam(c, QStringLiteral("anim"), -1),
                    intParam(c, QStringLiteral("ctrl"), -1));
        return true;
    }
    if (type == QLatin1String("changeanim") || type == QLatin1String("changeanim2")) {
        changeAnim(intParam(c, QStringLiteral("value"), m_animNo),
                   type == QLatin1String("changeanim2") ? m_stateData : nullptr);
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
        const int max = int(m_data.powerMax());
        int& power = powerOwner()->m_power;
        power = std::clamp(type == QLatin1String("poweradd") ? power + value : value, 0, max);
        return false;
    }
    if (type == QLatin1String("lifeset")) {
        m_life = std::clamp(intParam(c, QStringLiteral("value"), m_life), 0, int(m_data.lifeMax()));
        return false;
    }
    if (type == QLatin1String("sprpriority")) {
        m_sprPriority = intParam(c, QStringLiteral("value"), 0);
        return false;
    }
    if (type == QLatin1String("gravity")) {
        m_vel.y += m_data.yAccel();
        return false;
    }
    if (type == QLatin1String("varset") || type == QLatin1String("varadd") || type == QLatin1String("parentvarset") ||
        type == QLatin1String("parentvaradd")) {
        const bool add = type.endsWith(QLatin1String("add"));
        Character* to = type.startsWith(QLatin1String("parent")) ? m_parent : this;
        if (!to)
            return false;
        auto apply = [to, add](Trigger kind, int index, const Value& value) {
            if (add) {
                TriggerCall call;
                call.trigger = kind;
                const Value i = Value::integer(index);
                call.args = std::span<const Value>(&i, 1);
                to->assign(kind, index, Ops::add(to->trigger(call), value));
            } else {
                to->assign(kind, index, value);
            }
        };
        // "v = n" / "fv = n" with "value", or "var(n) = value"...
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
        // Only for a character that was hit and falls.
        if (m_moveType != QLatin1Char('H') || !m_ghv.fall)
            return false;
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
        if (m_moveType != QLatin1Char('H') || !m_ghv.fall)
            return false;
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
    if (type.startsWith(QLatin1String("target"))) {
        const QList<Character*> targets = targetsWithId(intParam(c, QStringLiteral("id"), -1));
        if (type == QLatin1String("targetbind")) {
            const int time = intParam(c, QStringLiteral("time"), 1);
            for (Character* t : targets) {
                t->m_bindTo = time > 0 ? this : nullptr;
                t->m_bindIsTarget = true;
                t->m_bindTime = std::max(0, time);
                t->m_bindPos =
                    Vec2{floatParam(c, QStringLiteral("pos"), 0, 0), floatParam(c, QStringLiteral("pos"), 0, 1)};
                t->m_bindFacing = m_facing;
                if (m_bindTo == t) {
                    m_bindTo = nullptr;
                    m_bindTime = 0;
                }
            }
        } else if (type == QLatin1String("targetfacing")) {
            // 1: the way this character faces, -1: the other way, 0: unchanged.
            const int value = intParam(c, QStringLiteral("value"), 0);
            for (Character* t : targets) {
                if (value != 0)
                    t->m_facing = value > 0 ? m_facing : -m_facing;
            }
        } else if (type == QLatin1String("targetlifeadd")) {
            // Damage as a hit deals it: this character's attack and the target's defence apply unless absolute.
            const int value = intParam(c, QStringLiteral("value"), 0);
            const bool kill = intParam(c, QStringLiteral("kill"), 1) != 0;
            const bool absolute = intParam(c, QStringLiteral("absolute"), 0) != 0;
            const double attack = double(m_data.constant(QStringLiteral("data.attack"), 100)) / 100.0 * m_attackMul;
            for (Character* t : targets) {
                if (value == 0)
                    break;
                double damage = -value;
                if (!absolute) {
                    const double defence =
                        t->m_data.defence() / 100.0 * double(t->m_defenceMul) * double(t->m_superDefenceMul);
                    damage = damage * attack / std::max(0.01, defence);
                }
                if (damage > 0 && damage < 1)
                    damage = 1;
                damage = std::min(damage, double(t->m_life));
                if (!kill && damage >= t->m_life && t->m_life > 0)
                    damage = t->m_life - 1;
                t->m_life = std::clamp(t->m_life - int(std::lround(damage)), 0, int(t->m_data.lifeMax()));
            }
        } else if (type == QLatin1String("targetpoweradd")) {
            const int value = intParam(c, QStringLiteral("value"), 0);
            for (Character* t : targets)
                t->powerOwner()->m_power = std::clamp(t->powerOwner()->m_power + value, 0, int(t->m_data.powerMax()));
        } else if (type == QLatin1String("targetstate")) {
            const int state = intParam(c, QStringLiteral("value"), 0);
            for (Character* t : targets) {
                t->m_ctrl = false;
                t->queueState(state, &m_data);
            }
        } else if (type == QLatin1String("targetvelset") || type == QLatin1String("targetveladd")) {
            const bool add = type == QLatin1String("targetveladd");
            for (Character* t : targets) {
                if (has(c, QStringLiteral("x")))
                    t->m_vel.x = (add ? t->m_vel.x : 0) + floatParam(c, QStringLiteral("x"), 0);
                if (has(c, QStringLiteral("y")))
                    t->m_vel.y = (add ? t->m_vel.y : 0) + floatParam(c, QStringLiteral("y"), 0);
            }
        } else if (type == QLatin1String("targetdrop")) {
            // All but those hit with excludeid (only one of them with keepone). The dropped ones are let go.
            const int keep = intParam(c, QStringLiteral("excludeid"), -1);
            const bool keepOne = intParam(c, QStringLiteral("keepone"), 1) != 0;
            bool kept = false;
            for (int i = 0; i < m_targets.size();) {
                if (keep >= 0 && m_targetIds[i] == keep && !(keepOne && kept)) {
                    kept = true;
                    ++i;
                    continue;
                }
                if (m_targets[i]->m_bindTo == this) {
                    m_targets[i]->m_bindTo = nullptr;
                    m_targets[i]->m_bindTime = 0;
                }
                m_targets.removeAt(i);
                m_targetIds.removeAt(i);
            }
        } else if (!pendingControllers().contains(type)) {
            m_messages
                << QStringLiteral("unknown controller type '%1' (%2, line %3)").arg(type, c.file).arg(c.line + 1);
        }
        return false;
    }
    if (type == QLatin1String("playerpush")) {
        m_push = intParam(c, QStringLiteral("value"), 1) != 0;
        return false;
    }
    if (type == QLatin1String("width")) {
        // player = front, back: the width that pushes. edge = front, back: the distance kept from the
        // screen's sides. value sets both. A missing back is 0.
        auto read = [this](const Param* p, float* backFront) {
            if (!p || !p->isExpression())
                return;
            backFront[1] = eval(p, 0).toFloat();
            backFront[0] = p->values.size() > 1 ? eval(p, 1).toFloat() : 0.f;
        };
        const Param* value = c.param(QStringLiteral("value"));
        read(value ? value : c.param(QStringLiteral("player")), m_width);
        read(value ? value : c.param(QStringLiteral("edge")), m_edge);
        return false;
    }
    if (type == QLatin1String("movehitreset")) {
        m_moveContactTime = 0;
        m_moveGuarded = false;
        return false;
    }
    if (type == QLatin1String("hitadd")) {
        const int value = intParam(c, QStringLiteral("value"), 0);
        m_hitCount += value;
        for (Character* t : std::as_const(m_targets)) {
            if (t->m_moveType == QLatin1Char('H'))
                t->m_ghv.hitCount = std::max(0, t->m_ghv.hitCount + value);
        }
        return false;
    }
    if (type == QLatin1String("lifeadd")) {
        // Scaled by the character's defence unless absolute. kill = 0 cannot bring the life below 1.
        const int max = int(m_data.lifeMax());
        int value = intParam(c, QStringLiteral("value"), 0);
        if (intParam(c, QStringLiteral("absolute"), 0) == 0)
            value = int(std::lround(value / std::max(0.01, m_data.defence() / 100.0)));
        int life = m_life + value;
        if (intParam(c, QStringLiteral("kill"), 1) == 0)
            life = std::max(life, std::min(1, m_life));
        m_life = std::clamp(life, 0, max);
        return false;
    }
    if (type == QLatin1String("destroyself")) {
        if (!m_isHelper)
            return false;
        destroy(intParam(c, QStringLiteral("recursive"), 0) != 0, intParam(c, QStringLiteral("removeexplods"), 0) != 0);
        return true;
    }
    if (executeEffect(c) || executeExplod(c) || executeHelper(c) || executeProjectile(c))
        return false;
    if (!pendingControllers().contains(type)) {
        const QString message =
            QStringLiteral("unknown controller type '%1' (%2, line %3)").arg(type, c.file).arg(c.line + 1);
        if (!m_messages.contains(message))
            m_messages << message;
    }
    return false;
}

} // namespace OFS::Mugen::Runtime
