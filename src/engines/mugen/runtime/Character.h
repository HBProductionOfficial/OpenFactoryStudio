#pragma once

#include "engines/mugen/runtime/AnimPlayer.h"
#include "engines/mugen/runtime/CharacterData.h"
#include "engines/mugen/runtime/Expr.h"
#include "engines/mugen/runtime/Hits.h"

#include <QHash>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>

#include <array>
#include <cstdint>
#include <vector>

namespace OFS::Mugen::Runtime {

class Character;

struct Vec2
{
    float x = 0;
    float y = 0;
};

class Match
{
public:
    explicit Match(std::uint32_t seed = 1);

    void add(Character* character);
    const QList<Character*>& characters() const { return m_characters; }
    Character* enemy(const Character* of, int index = 0) const;

    int gameTime() const { return m_time; }
    int random(int max);

    // Every character's states, then the hits, then the reactions and animations.
    void tick();

private:
    QList<Character*> m_characters;
    int m_time = 0;
    std::uint32_t m_seed;
};

class Character : public Context
{
public:
    explicit Character(const CharacterData& data, Match* match = nullptr);

    void start(int state = 0);
    // States -3, -2 and -1, then the current state, the movement, the time and the animation. In a match
    // the tick is split around the hit checks: runActions, then the attacks, then finishTick.
    void tick();
    void runActions();
    void finishTick();

    bool attack(Character& target);
    // 1: attack (Clsn1), 2: body (Clsn2), in screen coordinates.
    QList<QRectF> boxes(int type) const;
    const HitDef& hitDef() const { return m_hitDef; }
    const GetHitVars& getHitVars() const { return m_ghv; }
    int hitPauseTime() const { return m_hitPauseTime; }
    void setInGuardDist(bool in) { m_inGuardDist = in; }

    void changeState(int number, int anim = -1, int ctrl = -1);
    void changeAnim(int number);

    const CharacterData& data() const { return m_data; }
    int stateNo() const { return m_stateNo; }
    int prevStateNo() const { return m_prevStateNo; }
    int time() const { return m_time; }
    QChar stateType() const { return m_stateType; }
    QChar moveType() const { return m_moveType; }
    QChar physics() const { return m_physics; }
    bool ctrl() const { return m_ctrl; }
    QPointF pos() const { return QPointF(m_pos.x, m_pos.y); }
    QPointF vel() const { return QPointF(m_vel.x, m_vel.y); }
    int facing() const { return m_facing; }
    int animNo() const { return m_animNo; }
    const AnimPlayer& anim() const { return m_anim; }
    int life() const { return m_life; }
    int power() const { return m_power; }
    int var(int index) const;
    float fvar(int index) const;
    int id() const { return m_id; }

    void setPos(const QPointF& pos) { m_pos = Vec2{float(pos.x()), float(pos.y())}; }
    void setInput(const InputState& input) { m_input = input; }
    void setFacing(int facing) { m_facing = facing < 0 ? -1 : 1; }

    const QStringList& messages() const { return m_messages; }
    const QStringList& trace() const { return m_trace; }
    void setTracing(bool on) { m_tracing = on; }

    // Context
    Value trigger(const TriggerCall& call) override;
    Context* redirect(Redirect kind, const Value& argument) override;
    Value assign(Trigger kind, std::int32_t index, const Value& value) override;

private:
    bool runState(int number, std::vector<int>& counters);
    bool triggered(const Controller& controller);
    bool execute(const Controller& controller);
    void enterState();
    void updatePosition();
    void readHitDef(const Controller& c);
    bool canGuard(const HitDef& hit) const;
    void enterGetHitState();

    Value eval(const Param* param, int index = 0);
    int intParam(const Controller& c, const QString& name, int fallback, int index = 0);
    float floatParam(const Controller& c, const QString& name, float fallback, int index = 0);
    bool has(const Controller& c, const QString& name) const { return c.param(name) != nullptr; }

    const CharacterData& m_data;
    Match* m_match;
    int m_id;

    int m_stateNo = 0;
    int m_prevStateNo = 0;
    int m_time = 0;
    QChar m_stateType = QLatin1Char('S');
    QChar m_moveType = QLatin1Char('I');
    QChar m_physics = QLatin1Char('S');
    bool m_ctrl = true;
    bool m_pending = false;
    int m_minus = 0;
    int m_nesting = 0;
    std::vector<int> m_counters;
    QHash<int, std::vector<int>> m_specialCounters;

    Vec2 m_pos;
    Vec2 m_vel;
    int m_facing = 1;
    int m_animNo = 0;
    AnimPlayer m_anim;
    int m_life = 1000;
    int m_power = 0;
    int m_sprPriority = 0;
    std::array<std::int32_t, 60> m_vars{};
    std::array<float, 40> m_fvars{};
    std::array<std::int32_t, 5> m_sysVars{};
    std::array<float, 5> m_sysFVars{};

    CommandList m_commands;
    InputState m_input;

    HitDef m_hitDef;
    GetHitVars m_ghv;
    int m_hitPauseTime = 0;
    bool m_gotHit = false;
    int m_hitStateNo = -1;
    int m_pendingP1State = -1;
    int m_moveContactTime = 0;
    bool m_moveGuarded = false;
    int m_hitCount = 0;
    int m_uniqHitCount = 0;
    bool m_inGuardDist = false;
    bool m_posFreeze = false;

    QStringList m_messages;
    QStringList m_trace;
    bool m_tracing = false;
};

} // namespace OFS::Mugen::Runtime
