#pragma once

#include "engines/mugen/runtime/AnimPlayer.h"
#include "engines/mugen/runtime/CharacterData.h"
#include "engines/mugen/runtime/Effects.h"
#include "engines/mugen/runtime/Explod.h"
#include "engines/mugen/runtime/Expr.h"
#include "engines/mugen/runtime/Hits.h"

#include <QHash>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QStringList>

#include <array>
#include <cstdint>
#include <vector>

namespace OFS::Mugen::Runtime {

class Character;

class Match
{
public:
    explicit Match(std::uint32_t seed = 1);

    void add(Character* character);
    const QList<Character*>& characters() const { return m_characters; }
    Character* enemy(const Character* of, int index = 0) const;

    int gameTime() const { return m_time; }

    void setBounds(float left, float right)
    {
        m_left = left;
        m_right = right;
    }
    void setScreen(float width, float edge)
    {
        m_screenWidth = width;
        m_screenEdge = edge;
    }
    float cameraX() const { return m_cameraX; }
    float screenWidth() const { return m_screenWidth; }
    float screenEdge() const { return m_screenEdge; }
    int random(int max);

    bool isAsserted(const QString& flag) const;
    const QList<SoundEvent>& sounds() const { return m_sounds; }
    const QList<SpawnEvent>& spawns() const { return m_spawns; }
    void addSound(const SoundEvent& event) { m_sounds << event; }
    void addSpawn(const SpawnEvent& event) { m_spawns << event; }
    EnvShake& envShake() { return m_envShake; }
    EnvColor& envColor() { return m_envColor; }
    PalFx& allPalFx() { return m_allPalFx; }
    PalFx& bgPalFx() { return m_bgPalFx; }

    // A new pause starts on the next tick.
    int superPauseTime() const { return m_superTime; }
    int pauseTime() const { return m_pauseTime; }
    bool superDarken() const { return m_superDarken; }
    bool pauseBackground() const { return m_superTime > 0 ? m_superPauseBg : m_pauseBg; }
    void requestPause(bool super, int playerId, int time, bool pauseBg, bool darken = false);

    void setFightFx(const CharacterData* data) { m_fightFx = data; }
    const CharacterData* fightFx() const { return m_fightFx; }

    // Every character's states, then the hits, then the reactions and animations.
    void tick();

private:
    void push();
    void keepOnScreen();

    QList<Character*> m_characters;
    int m_time = 0;
    std::uint32_t m_seed;
    float m_left = -1000;
    float m_right = 1000;
    float m_screenWidth = 320;
    float m_screenEdge = 15;
    float m_cameraX = 0;
    QList<SoundEvent> m_sounds;
    QList<SpawnEvent> m_spawns;
    EnvShake m_envShake;
    EnvColor m_envColor;
    PalFx m_allPalFx;
    PalFx m_bgPalFx;
    int m_superTime = 0;
    int m_pauseTime = 0;
    int m_superPending = -1;
    int m_pausePending = -1;
    int m_superPlayer = 0;
    int m_pausePlayer = 0;
    bool m_superDarken = true;
    bool m_superPauseBg = true;
    bool m_pauseBg = true;
    const CharacterData* m_fightFx = nullptr;
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
    float backWidth() const;
    float frontWidth() const;
    bool pushes() const { return m_push; }
    void moveX(float dx) { m_pos.x += dx; }
    void releaseTargets();
    void updateBindings();
    void tickBinding();

    void changeState(int number, int anim = -1, int ctrl = -1);
    void changeAnim(int number, const CharacterData* owner = nullptr);
    const CharacterData& stateOwner() const { return *m_stateData; }
    const QList<Character*>& targets() const { return m_targets; }

    const DrawState& draw() const { return m_draw; }
    bool isAsserted(const QString& flag) const { return m_asserted.contains(flag); }
    bool screenBound() const { return m_screenBound; }
    bool movesCamera() const { return m_moveCamera[0]; }
    const QString& clipboard() const { return m_clipboard; }
    int victoryQuote() const { return m_victoryQuote; }
    float attackMul() const { return m_attackMul; }
    float defenceMul() const { return m_defenceMul; }
    bool frozen() const { return m_frozen; }
    float superDefenceMul() const { return m_superDefenceMul; }
    const QList<Explod>& explods() const { return m_explods; }
    int numExplods(int id = -1) const;
    void updateExplods();

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
    void prepare();
    bool inGuardState() const;
    bool runState(const CharacterData& source, int number, std::vector<int>& counters);
    bool executeEffect(const Controller& c);
    bool hittableBy(const HitDef& hit) const;
    bool executeExplod(const Controller& c);
    void readExplod(const Controller& c, Explod& e, bool create);
    void placeExplod(Explod& e);
    // TargetState from another character waits for this character's next turn.
    void queueState(int number, const CharacterData* owner);
    QList<Character*> targetsWithId(int id) const;
    bool triggered(const Controller& controller);
    bool execute(const Controller& controller);
    void enterState();
    void updatePosition();
    void readHitDef(const Controller& c);
    bool canGuard(const HitDef& hit) const;
    void enterGetHitState();
    // The engine turns idle characters to face an opponent who went behind.
    void autoTurn();

    Value eval(const Param* param, int index = 0);
    int intParam(const Controller& c, const QString& name, int fallback, int index = 0);
    float floatParam(const Controller& c, const QString& name, float fallback, int index = 0);
    bool has(const Controller& c, const QString& name) const { return c.param(name) != nullptr; }

    const CharacterData& m_data;
    const CharacterData* m_stateData;
    const CharacterData* m_animData;
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
    // Where the character is in its tick: -3, -2, -1 for the special states, 0 for the current state (a
    // ChangeState then runs the new state at once), 1 after the actions, 2 while finishing, 3 between
    // ticks (state changes wait for the next tick).
    int m_minus = 3;
    int m_nesting = 0;
    std::vector<int> m_counters;
    QHash<int, std::vector<int>> m_specialCounters;

    Vec2 m_pos;
    Vec2 m_vel;
    int m_facing = 1;
    int m_animNo = -1;
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
    const CharacterData* m_hitStateOwner = nullptr;
    QList<Character*> m_targets;
    QList<int> m_targetIds;
    Character* m_bindTo = nullptr;
    Vec2 m_bindPos;
    int m_bindTime = 0;
    int m_bindFacing = 1;
    int m_pendingP1State = -1;
    int m_moveContactTime = 0;
    bool m_moveGuarded = false;
    int m_hitCount = 0;
    int m_uniqHitCount = 0;
    bool m_inGuardDist = false;
    bool m_posFreeze = false;
    bool m_push = true;
    float m_width[2] = {-1, -1};
    bool m_guarding = false;
    int m_airJumps = 0;

    struct HitBySlot
    {
        bool notHitBy = false;
        QString states;
        QSet<QString> classes;
        int time = 0;
    };
    std::array<HitBySlot, 2> m_hitBy;
    float m_attackMul = 1;
    float m_defenceMul = 1;
    QSet<QString> m_asserted;
    bool m_screenBound = true;
    bool m_moveCamera[2] = {true, true};
    DrawState m_draw;
    QString m_clipboard;
    int m_victoryQuote = -1;
    bool m_frozen = false;
    int m_superMoveTime = 0;
    int m_pauseMoveTime = 0;
    int m_unhittableTime = 0;
    float m_superDefenceMul = 1;
    float m_superDefenceMulNext = 1;
    QList<Explod> m_explods;
    bool m_wasHit = false;

    QStringList m_messages;
    QStringList m_trace;
    bool m_tracing = false;
};

} // namespace OFS::Mugen::Runtime
