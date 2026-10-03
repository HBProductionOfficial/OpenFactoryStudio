#pragma once

#include "engines/mugen/runtime/AnimPlayer.h"
#include "engines/mugen/runtime/CharacterData.h"
#include "engines/mugen/runtime/Effects.h"
#include "engines/mugen/runtime/Explod.h"
#include "engines/mugen/runtime/Expr.h"
#include "engines/mugen/runtime/Hits.h"
#include "engines/mugen/runtime/Projectile.h"

#include <QHash>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QStringList>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace OFS::Mugen::Runtime {

class Character;

enum class ProjContactKind
{
    Contact,
    Hit,
    Guarded,
    Cancel,
};

// fight.def's [Round] timings, in ticks.
struct RoundSettings
{
    int wins = 2;
    int time = 99; // in counts, -1: no limit
    int framesPerCount = 60;
    int startWait = 30;
    int ctrlTime = 30;
    int overHitTime = 10;
    int overWaitTime = 45;
    int overWinTime = 45;
    int overTime = 210;
    int callTime = 0; // after the intros, the round is held while the fight screen calls it
    float p1x = -70;
    float p2x = 70;
};

enum class RoundEnd
{
    None,
    KO,
    DoubleKO,
    TimeOver,
};

enum class ExplodRules
{
    PlayerVersion,
    Mugen10,
    Mugen11,
};

enum class WinType
{
    Normal,
    Special,
    Hyper,
    Throw,
    Cheese,
    Time,
    Suicide,
};

struct RoundWin
{
    WinType type = WinType::Normal;
    bool perfect = false;
};

class Match
{
public:
    explicit Match(std::uint32_t seed = 1);
    ~Match();

    void add(Character* character);
    const QList<Character*>& characters() const { return m_characters; }
    Character* enemy(const Character* of, int index = 0) const;
    Character* enemyNear(const Character* of, int index = 0) const;
    int numEnemies(const Character* of) const;
    Character* playerById(int id) const;
    Character* spawnHelper(Character& parent);

    // Players go to state 5900, then their intros, the fight, the end and the poses.
    void startMatch(const RoundSettings& settings = RoundSettings());
    // 0 before the intros, 1 intros, 2 fighting, 3 ended, 4 poses.
    int roundState() const;
    int roundNo() const { return m_roundActive ? m_roundNo : 1; }
    int roundsExisted() const { return m_roundActive ? m_roundNo - 1 : 0; }
    int roundTime() const { return m_roundActive ? m_timer : -1; }
    RoundEnd roundEnd() const { return m_end; }
    int roundWinner() const { return m_winner; }
    bool perfect() const { return m_perfect; }
    int wins(int team) const { return team == 0 || team == 1 ? m_wins[team] : 0; }
    const QList<RoundWin>& roundWins(int team) const { return m_roundWins[team == 1 ? 1 : 0]; }
    int introClock() const { return m_introClock; }
    int overClock() const { return m_end == RoundEnd::None ? -1 : -m_intro - 1; }
    bool matchOver() const { return m_matchOver; }
    bool hitsAllowed() const;

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
    void setScreenHeight(float height, float floor)
    {
        m_screenHeight = height;
        m_screenFloor = floor;
    }
    float cameraX() const { return m_cameraX; }
    float stageLeft() const { return m_left; }
    float stageRight() const { return m_right; }
    float screenWidth() const { return m_screenWidth; }
    float screenEdge() const { return m_screenEdge; }
    float screenHeight() const { return m_screenHeight; }
    float screenFloor() const { return m_screenFloor; }
    void setStageInfo(const QString& key, const QString& value) { m_stageInfo.insert(key, value); }
    QString stageInfo(const QString& key) const { return m_stageInfo.value(key); }
    struct Rules
    {
        float attackLifeToPowerMul = 0.7f;
        float getHitLifeToPowerMul = 0.6f;
        float superTargetDefenceMul = 1.5f;
    };
    void setRules(const Rules& rules) { m_rules = rules; }
    const Rules& rules() const { return m_rules; }
    int random(int max);

    bool isAsserted(const QString& flag) const;
    void setExplodRules(ExplodRules rules) { m_explodRules = rules; }
    ExplodRules explodRules() const { return m_explodRules; }
    // Called before each controller and trigger line (0-based). The debugger may hold the match there.
    using LineObserver = std::function<void(const Character& character, const Controller& controller, int line)>;
    void setLineObserver(LineObserver observer) { m_lineObserver = std::move(observer); }
    void observeLine(const Character& character, const Controller& controller, int line) const
    {
        if (m_lineObserver)
            m_lineObserver(character, controller, line);
    }
    void setNoKo(bool noKo) { m_noKo = noKo; }
    bool noKo() const { return m_noKo; }
    const Character* attackerNear(const Character* c) const;
    const QList<SoundEvent>& sounds() const { return m_sounds; }
    const QList<SpawnEvent>& spawns() const { return m_spawns; }
    void addSound(const SoundEvent& event) { m_sounds << event; }
    void addSpawn(const SpawnEvent& event) { m_spawns << event; }
    EnvShake& envShake() { return m_envShake; }
    const EnvShake& envShake() const { return m_envShake; }
    EnvColor& envColor() { return m_envColor; }
    const EnvColor& envColor() const { return m_envColor; }
    PalFx& allPalFx() { return m_allPalFx; }
    const PalFx& allPalFx() const { return m_allPalFx; }
    PalFx& bgPalFx() { return m_bgPalFx; }
    const PalFx& bgPalFx() const { return m_bgPalFx; }

    // A new pause starts on the next tick.
    int superPauseTime() const { return m_superTime; }
    int pauseTime() const { return m_pauseTime; }
    bool superDarken() const { return m_superDarken; }
    bool pauseBackground() const { return m_superTime > 0 ? m_superPauseBg : m_pauseBg; }
    int endCmdBufTime() const { return m_superTime > 0 ? m_superEndCmdBufTime : m_pauseEndCmdBufTime; }
    void requestPause(bool super, int playerId, int time, bool pauseBg, bool darken = false, int endCmdBufTime = 0);

    void setFightFx(const CharacterData* data) { m_fightFx = data; }
    const CharacterData* fightFx() const { return m_fightFx; }

    // Every character's states, then the hits, then the reactions and animations.
    void tick();

private:
    void push();
    void keepOnScreen();
    void removeDestroyed();
    void startRound();
    void stepRound();
    bool decideRound();
    WinType winType() const;
    void resetPositions();

    QList<Character*> m_characters;
    int m_time = 0;
    std::uint32_t m_seed;
    float m_left = -1000;
    float m_right = 1000;
    float m_screenWidth = 320;
    float m_screenEdge = 15;
    float m_screenHeight = 240;
    float m_screenFloor = 200;
    float m_cameraX = 0;
    QHash<QString, QString> m_stageInfo;
    Rules m_rules;
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
    int m_superEndCmdBufTime = 0;
    int m_pauseEndCmdBufTime = 0;
    const CharacterData* m_fightFx = nullptr;
    std::vector<std::unique_ptr<Character>> m_helpers;

    bool m_noKo = false;
    LineObserver m_lineObserver;
    ExplodRules m_explodRules = ExplodRules::PlayerVersion;
    bool m_roundActive = false;
    RoundSettings m_settings;
    int m_roundNo = 0;
    int m_intro = 0; // counts down to 0 (the fight), then below while the round ends
    int m_timer = -1;
    int m_winPoseTime = 0;
    RoundEnd m_end = RoundEnd::None;
    int m_winner = -1;
    bool m_perfect = false;
    int m_wins[2] = {0, 0};
    QList<RoundWin> m_roundWins[2];
    int m_introClock = -1;
    bool m_matchOver = false;
};

class Character : public Context
{
    friend class Match;

public:
    explicit Character(const CharacterData& data, Match* match = nullptr);

    void start(int state = 0);
    // States -3, -2 and -1, then the current state, the movement, the time and the animation. In a match
    // the tick is split around the hit checks: runActions, then the attacks, then finishTick.
    void tick();
    void runActions();
    void finishTick();

    bool wouldHit(const Character& target) const;
    bool lands(const Character& target) const;
    bool reverses(const Character& attacker) const;
    bool attack(Character& target);
    void reverse(Character& attacker);
    // 1: attack (Clsn1), 2: body (Clsn2), in screen coordinates.
    QList<QRectF> boxes(int type) const;
    const HitDef& hitDef() const { return m_hitDef; }
    const GetHitVars& getHitVars() const { return m_ghv; }
    int hitPauseTime() const { return m_hitPauseTime; }
    void setInGuardDist(bool in) { m_inGuardDist = in; }
    float backWidth() const;
    float frontWidth() const;
    float backEdgeWidth() const { return m_edge[0]; }
    float frontEdgeWidth() const { return m_edge[1]; }
    int jugglePoints() const { return m_juggle; }
    bool pushes() const { return m_push; }
    void moveX(float dx) { m_pos.x += dx; }
    void releaseTargets();
    void updateBindings();
    void tickBinding();

    void changeState(int number, int anim = -1, int ctrl = -1);
    void changeAnim(int number, const CharacterData* owner = nullptr);
    const CharacterData& stateOwner() const { return *m_stateData; }
    const CharacterData& animationData() const { return *m_animData; }
    const QList<Character*>& targets() const { return m_targets; }

    const DrawState& draw() const { return m_draw; }
    const QList<AfterImageFrame>& afterImages() const { return m_afterImages; }
    bool isAsserted(const QString& flag) const { return m_asserted.contains(flag); }
    bool screenBound() const { return m_screenBound; }
    bool movesCamera() const { return m_moveCamera[0]; }
    const QString& clipboard() const { return m_clipboard; }
    int victoryQuote() const { return m_victoryQuote; }
    float attackMul() const { return m_attackMul; }
    float defenceMul() const { return m_defenceMul; }
    bool frozen() const { return m_frozen; }
    bool alive() const { return !m_ko; }
    void resetForRound(Vec2 pos, int facing, bool firstRound);
    float superDefenceMul() const { return m_superDefenceMul; }
    const QList<Explod>& explods() const { return m_explods; }
    int numExplods(int id = -1) const;
    void updateExplods();
    const QList<Projectile>& projectiles() const { return m_projectiles; }
    int numProjectiles(int id = -1) const;
    void updateProjectiles();
    void cancelProjectiles(Character& other);
    void projectilesAttack(Character& target);
    void tickProjectiles();
    QList<QRectF> projectileBoxes(const Projectile& p, int type) const;

    const CharacterData& data() const { return m_data; }
    QString name() const { return m_isHelper ? m_helperName : m_data.name(); }
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
    // Helpers share their root's power.
    int power() const { return powerOwner()->m_power; }
    int var(int index) const;
    float fvar(int index) const;
    int sysVar(int index) const { return index >= 0 && index < 5 ? m_sysVars[std::size_t(index)] : 0; }
    float sysFVar(int index) const { return index >= 0 && index < 5 ? m_sysFVars[std::size_t(index)] : 0; }
    int runningState() const { return m_runningState; }
    const CharacterData& runningStateData() const { return m_runningStateData ? *m_runningStateData : m_data; }
    void setVar(int index, int value)
    {
        if (index >= 0 && index < int(m_vars.size()))
            m_vars[std::size_t(index)] = value;
    }
    int id() const { return m_id; }
    int sprPriority() const { return m_sprPriority; }

    bool isHelper() const { return m_isHelper; }
    int helperId() const { return m_helperId; }
    Character* parent() const { return m_parent; }
    Character* root() const { return m_root; }
    bool destroyed() const { return m_destroyed; }
    // A helper without ownpal shares its parent's palette, so the parent's PalFX and RemapPal reach it.
    bool ownPal() const { return !m_isHelper || m_ownPal; }
    int team() const { return m_team; }
    bool isPlayerType() const { return !m_isHelper || m_helperPlayerType; }
    float constant(const QString& name, float fallback) const;
    float lifeMax() const { return constant(QStringLiteral("data.life"), m_data.lifeMax()); }
    float powerMax() const { return constant(QStringLiteral("data.power"), m_data.powerMax()); }
    void setLife(int life);
    void setPower(int power);
    bool explodRules11() const;
    void setAiLevel(int level) { m_aiLevel = level; }
    int aiLevel() const { return (m_root ? m_root : this)->m_aiLevel; }

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
    bool executeHelper(const Controller& c);
    Character* powerOwner() { return m_root ? m_root : this; }
    const Character* powerOwner() const { return m_root ? m_root : this; }
    void updateHelperBinding();
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
    HitDef hitDefFrom(const Controller& c);
    bool hitAllowed(const HitDef& hd, const Character& target, bool projectile = false) const;
    int juggleCost(const HitDef& hd, bool projectile) const { return hd.airJuggle + (projectile ? 0 : m_stateJuggle); }
    bool deliverHit(Character& target, const HitDef& hd, int facing, int overrideSlot = -1, bool projectile = false);
    void destroy(bool recursive, bool removeExplods);
    void showHit(Character& target, const HitDef& hd, int facing, bool guarded);
    bool executeProjectile(const Controller& c);
    const CharacterData* projectileAnimData(const Projectile::Anim& anim) const;
    bool showProjectileAnim(Projectile& p, const Projectile::Anim& anim);
    int projContactTime(ProjContactKind kind, int id) const;
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
    int m_runningState = 0;
    const CharacterData* m_runningStateData = nullptr;
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
    int m_aiLevel = 0;
    int m_sprPriority = 0;
    std::array<std::int32_t, 60> m_vars{};
    std::array<float, 40> m_fvars{};
    std::array<std::int32_t, 5> m_sysVars{};
    std::array<float, 5> m_sysFVars{};

    CommandList m_commands;
    InputState m_input;

    HitDef m_hitDef;
    bool m_hitDefAttr = false; // HitDefAttr still reads it after the hit
    bool m_hitDefLanded = false;
    int m_stateJuggle = 0;
    int m_juggle = 15;
    GetHitVars m_ghv;
    int m_hitPauseTime = 0;
    bool m_gotHit = false;
    int m_hitStateNo = -1;
    const CharacterData* m_hitStateOwner = nullptr;
    QList<Character*> m_targets;
    QList<int> m_targetIds;
    Character* m_bindTo = nullptr;
    bool m_bindIsTarget = false;
    int m_bindTurn = 0;
    Vec2 m_bindPos;
    int m_bindTime = 0;
    int m_bindFacing = 1;
    int m_pendingP1State = -1;
    int m_moveContactTime = 0;
    bool m_moveGuarded = false;
    int m_moveReversedTime = 0;
    int m_hitCount = 0;
    int m_uniqHitCount = 0;
    bool m_inGuardDist = false;
    bool m_posFreeze = false;
    bool m_push = true;
    float m_width[2] = {-1, -1};
    float m_edge[2] = {0, 0};
    int m_lastDust = -1000;
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
    struct HitOverrideSlot
    {
        QString states;
        QSet<QString> classes;
        int stateNo = -1;
        int time = 0;
        bool forceAir = false;
    };
    std::array<HitOverrideSlot, 8> m_overrides;
    int overrideFor(const HitDef& hit, QChar attackerState) const;
    float m_attackMul = 1;
    float m_defenceMul = 1;
    QSet<QString> m_asserted;
    bool m_screenBound = true;
    bool m_moveCamera[2] = {true, true};
    DrawState m_draw;
    QString m_clipboard;
    int m_victoryQuote = -1;
    bool m_frozen = false;
    bool m_ko = false;
    bool m_posed = false;
    int m_superMoveTime = 0;
    int m_pauseMoveTime = 0;
    int m_unhittableTime = 0;
    float m_superDefenceMul = 1;
    float m_superDefenceMulNext = 1;
    QList<Explod> m_explods;
    QList<AfterImageFrame> m_afterImages;
    int m_afterImageTicks = 0;
    bool m_wasHit = false;
    QList<Projectile> m_projectiles;
    ProjContactKind m_projContact = ProjContactKind::Hit;
    int m_projContactTime = -1;
    int m_projContactId = 0;

    bool m_isHelper = false;
    bool m_helperPlayerType = false;
    bool m_keyCtrl = true;
    bool m_ownPal = false;
    int m_helperId = 0;
    QString m_helperName;
    Character* m_parent = nullptr;
    Character* m_root = nullptr;
    int m_team = 0;
    bool m_destroyed = false;
    QHash<QString, float> m_constants;

    QStringList m_messages;
    QStringList m_trace;
    bool m_tracing = false;
};

} // namespace OFS::Mugen::Runtime
