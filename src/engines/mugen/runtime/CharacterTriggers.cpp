#include "engines/mugen/runtime/Character.h"
#include "engines/mugen/runtime/CharacterDetail.h"

#include <algorithm>
#include <cmath>

namespace OFS::Mugen::Runtime {

using namespace Detail;

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
        // AnimElem = n means AnimElemTime(n) = 0. "AnimElem = n, op t" compares AnimElemTime(n).
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
        // X from the centre of the screen.
        return Value::real(call.keyword == QLatin1String("y") ? m_pos.y : m_pos.x - (m_match ? m_match->cameraX() : 0));
    case Trigger::ScreenPos: {
        const float left = m_match ? m_match->cameraX() - m_match->screenWidth() / 2 : -160;
        return Value::real(call.keyword == QLatin1String("y") ? m_pos.y : m_pos.x - left);
    }
    case Trigger::BackEdgeDist:
    case Trigger::FrontEdgeDist:
    case Trigger::BackEdgeBodyDist:
    case Trigger::FrontEdgeBodyDist: {
        const float camera = m_match ? m_match->cameraX() : 0;
        const float half = m_match ? m_match->screenWidth() / 2 : 160;
        const float leftEdge = camera - half;
        const float rightEdge = camera + half;
        const bool back = call.trigger == Trigger::BackEdgeDist || call.trigger == Trigger::BackEdgeBodyDist;
        const bool body = call.trigger == Trigger::BackEdgeBodyDist || call.trigger == Trigger::FrontEdgeBodyDist;
        const bool towardsLeft = back == (m_facing > 0);
        float distance = towardsLeft ? m_pos.x - leftEdge : rightEdge - m_pos.x;
        if (body)
            distance -= back ? backWidth() : frontWidth();
        return Value::integer(int(std::floor(distance)));
    }
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
    case Trigger::Const240p:
        return Value::real(float(m_data.localCoordWidth()) / 320 * arg(0).toFloat());
    case Trigger::Const480p:
        return Value::real(float(m_data.localCoordWidth()) / 640 * arg(0).toFloat());
    case Trigger::Const720p:
        return Value::real(float(m_data.localCoordWidth()) / 1280 * arg(0).toFloat());
    case Trigger::Life:
        return Value::integer(m_life);
    case Trigger::LifeMax:
        return Value::integer(int(m_data.constant(QStringLiteral("data.life"), 1000)));
    case Trigger::Power:
        return Value::integer(powerOwner()->m_power);
    case Trigger::PowerMax:
        return Value::integer(int(m_data.constant(QStringLiteral("data.power"), 3000)));
    case Trigger::Alive:
        return Value::boolean(!m_ko);
    case Trigger::GameTime:
        return Value::integer(m_match ? m_match->gameTime() : 0);
    case Trigger::Random:
        return Value::integer(m_match ? m_match->random(1000) : 0);
    case Trigger::ID:
        return Value::integer(m_id);
    case Trigger::RoundState:
        return Value::integer(m_match ? m_match->roundState() : 2);
    case Trigger::RoundNo:
        return Value::integer(m_match ? m_match->roundNo() : 1);
    case Trigger::RoundsExisted:
        return Value::integer(m_match ? m_match->roundsExisted() : 0);
    case Trigger::TeamSide:
        return Value::integer(m_team + 1);
    case Trigger::MatchNo:
    case Trigger::PalNo:
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
        if (key == QLatin1String("fall.envshake.time"))
            return Value::integer(m_ghv.fallEnvShakeTime);
        if (key == QLatin1String("fall.envshake.freq"))
            return Value::real(m_ghv.fallEnvShakeFreq);
        if (key == QLatin1String("fall.envshake.ampl"))
            return Value::integer(m_ghv.fallEnvShakeAmpl);
        if (key == QLatin1String("fall.envshake.phase"))
            return Value::real(m_ghv.fallEnvShakePhase);
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
        if (key == QLatin1String("down.recover"))
            return Value::boolean(m_ghv.downRecover);
        if (key == QLatin1String("down.recovertime"))
            return Value::integer(m_ghv.downRecoverTime);
        if (key == QLatin1String("isbound"))
            return Value::boolean(m_bindTo != nullptr);
        if (key == QLatin1String("xveladd") || key == QLatin1String("yveladd") || key == QLatin1String("xoff") ||
            key == QLatin1String("yoff"))
            return Value::integer(0);
        return Value::undefined();
    }
    case Trigger::NumTarget:
        return Value::integer(int(targetsWithId(call.args.empty() ? -1 : arg(0).toInt()).size()));
    case Trigger::NumExplod:
        return Value::integer(numExplods(call.args.empty() ? -1 : arg(0).toInt()));
    case Trigger::NumProj:
        return Value::integer((m_root ? m_root : this)->numProjectiles());
    case Trigger::NumProjID:
        return Value::integer((m_root ? m_root : this)->numProjectiles(std::max(0, arg(0).toInt())));
    case Trigger::ProjContact:
    case Trigger::ProjHit:
    case Trigger::ProjGuarded: {
        // ProjHit[ID] = 1: contact on the last tick. "= 1, op t" compares the ticks since.
        const ProjContactKind kind = call.trigger == Trigger::ProjHit       ? ProjContactKind::Hit
                                     : call.trigger == Trigger::ProjGuarded ? ProjContactKind::Guarded
                                                                            : ProjContactKind::Contact;
        const int time = projContactTime(kind, call.keyword.isEmpty() ? 0 : call.keyword.toInt());
        bool result =
            call.hasSecond ? time >= 0 && Ops::compare(Value::integer(time), arg(1), call.second).toBool() : time == 0;
        if (arg(0).toInt() == 0)
            result = !result;
        return special(result);
    }
    case Trigger::ProjContactTime:
    case Trigger::ProjHitTime:
    case Trigger::ProjGuardedTime:
    case Trigger::ProjCancelTime: {
        const ProjContactKind kind = call.trigger == Trigger::ProjHitTime       ? ProjContactKind::Hit
                                     : call.trigger == Trigger::ProjGuardedTime ? ProjContactKind::Guarded
                                     : call.trigger == Trigger::ProjCancelTime  ? ProjContactKind::Cancel
                                                                                : ProjContactKind::Contact;
        return Value::integer(projContactTime(kind, call.args.empty() ? 0 : arg(0).toInt()));
    }
    case Trigger::IsHelper:
        return Value::boolean(m_isHelper && (call.args.empty() || m_helperId == arg(0).toInt()));
    case Trigger::NumHelper: {
        const Character* player = m_root ? m_root : this;
        int count = 0;
        if (m_match) {
            for (const Character* c : m_match->characters()) {
                if (c->m_isHelper && !c->m_destroyed && c->m_root == player &&
                    (call.args.empty() || c->m_helperId == arg(0).toInt()))
                    ++count;
            }
        }
        return Value::integer(count);
    }
    case Trigger::ParentDist:
    case Trigger::RootDist: {
        const Character* to = call.trigger == Trigger::ParentDist ? m_parent : m_root;
        if (!to)
            return Value::undefined();
        if (call.keyword == QLatin1String("y"))
            return Value::real(to->m_pos.y - m_pos.y);
        return Value::real((to->m_pos.x - m_pos.x) * float(m_facing));
    }
    case Trigger::MatchOver:
        return Value::boolean(m_match && m_match->matchOver());
    case Trigger::Win:
    case Trigger::WinKO:
    case Trigger::WinTime:
    case Trigger::WinPerfect:
    case Trigger::Lose:
    case Trigger::LoseKO:
    case Trigger::LoseTime:
    case Trigger::DrawGame: {
        const int winner = m_match ? m_match->roundWinner() : -1;
        const RoundEnd end = m_match ? m_match->roundEnd() : RoundEnd::None;
        const bool win = winner == m_team;
        const bool lose = (winner == 0 || winner == 1) && winner != m_team;
        switch (call.trigger) {
        case Trigger::Win:
            return Value::boolean(win);
        case Trigger::WinKO:
            return Value::boolean(win && end == RoundEnd::KO);
        case Trigger::WinTime:
            return Value::boolean(win && end == RoundEnd::TimeOver);
        case Trigger::WinPerfect:
            return Value::boolean(win && m_match->perfect());
        case Trigger::Lose:
            return Value::boolean(lose);
        case Trigger::LoseKO:
            return Value::boolean(lose && end == RoundEnd::KO);
        case Trigger::LoseTime:
            return Value::boolean(lose && end == RoundEnd::TimeOver);
        default:
            return Value::boolean(winner == 2);
        }
    }
    case Trigger::NumPartner:
    case Trigger::MoveReversed:
    case Trigger::AILevel:
        return Value::boolean(false);
    case Trigger::Command: {
        // A helper reads its player's commands with keyctrl only.
        const Character* reader = m_root ? m_root : this;
        return special(m_keyCtrl && reader->m_commands.active(call.text));
    }
    case Trigger::NumEnemy:
        return Value::integer(m_match ? m_match->numEnemies(this) : 0);
    case Trigger::P2BodyDist:
    case Trigger::P2Dist: {
        const Character* enemy = m_match ? m_match->enemy(this) : nullptr;
        if (!enemy)
            return Value::undefined();
        if (call.keyword == QLatin1String("y"))
            return Value::real(enemy->m_pos.y - m_pos.y);
        float distance = (enemy->m_pos.x - m_pos.x) * float(m_facing);
        // Between the fronts of the bodies (the opponent's back when it looks away).
        if (call.trigger == Trigger::P2BodyDist) {
            const bool facesMe = (m_pos.x - enemy->m_pos.x) * float(enemy->m_facing) >= 0;
            distance -= frontWidth() + (facesMe ? enemy->frontWidth() : enemy->backWidth());
        }
        return Value::real(distance);
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
        return m_match ? m_match->enemy(this, argument.isUndefined() ? 0 : argument.toInt()) : nullptr;
    case Redirect::EnemyNear:
        return m_match ? m_match->enemyNear(this, argument.isUndefined() ? 0 : argument.toInt()) : nullptr;
    case Redirect::PlayerID:
        if (m_match) {
            for (Character* c : m_match->characters()) {
                if (c->m_id == argument.toInt())
                    return c;
            }
        }
        return nullptr;
    case Redirect::Target: {
        const QList<Character*> targets = targetsWithId(argument.isUndefined() ? -1 : argument.toInt());
        return targets.isEmpty() ? nullptr : targets.first();
    }
    case Redirect::Root:
        return m_root;
    case Redirect::Parent:
        return m_parent;
    case Redirect::Helper: {
        const Character* player = m_root ? m_root : this;
        if (m_match) {
            for (Character* c : m_match->characters()) {
                if (c->m_isHelper && !c->m_destroyed && c->m_root == player &&
                    (argument.isUndefined() || c->m_helperId == argument.toInt()))
                    return c;
            }
        }
        return nullptr;
    }
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
