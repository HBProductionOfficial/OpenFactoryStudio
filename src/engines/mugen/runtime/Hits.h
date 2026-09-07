#pragma once

#include <QString>
#include <QStringList>

#include <array>
#include <cmath>

namespace OFS::Mugen::Runtime {

enum class AnimType
{
    Light = 0,
    Medium = 1,
    Hard = 2,
    Back = 3,
    Up = 4,
    DiagUp = 5,
};

enum class HitType
{
    None = 0,
    High = 1,
    Low = 2,
    Trip = 3,
};

enum class PriorityType
{
    Hit,   // both land when the priorities are equal
    Miss,  // loses to Hit at equal priority, neither lands against Miss
    Dodge, // neither lands at equal priority
};

// A ReversalDef is a HitDef too: it turns back the attacks of its attributes that its Clsn1 meets.
struct HitDef
{
    bool active = false;
    bool reversal = false;
    QString reversalStates;
    QStringList reversalClasses;
    int priority = 4;
    PriorityType priorityType = PriorityType::Hit;
    QString attrStates = QStringLiteral("S");
    QStringList attrClasses;
    QString hitFlag = QStringLiteral("MAF");
    QString guardFlag;
    AnimType animType = AnimType::Light;
    AnimType airAnimType = AnimType::Light;
    AnimType fallAnimType = AnimType::Back;
    int hitDamage = 0;
    int guardDamage = 0;
    int pauseTime[2] = {0, 0}; // attacker's pause, defender's shake
    int guardPauseTime[2] = {0, 0};
    HitType groundType = HitType::High;
    HitType airType = HitType::High;
    int groundSlideTime = 0;
    int groundHitTime = 0;
    int guardSlideTime = 0;
    int guardHitTime = 0;
    int guardCtrlTime = 0;
    int airGuardCtrlTime = 0;
    int airHitTime = 20;
    int downHitTime = 20;
    float groundVelocity[2] = {0, 0};
    float guardVelocity = 0;
    float airVelocity[2] = {0, 0};
    float airGuardVelocity[2] = {0, 0};
    float downVelocity[2] = {0, 0};
    float yAccel = 0.35f;
    bool fall = false;
    bool airFall = false;
    float fallXVelocity = std::nanf("");
    float fallYVelocity = -4.5f;
    bool fallRecover = true;
    int fallRecoverTime = 4;
    int fallDamage = 0;
    int p1StateNo = -1;
    int p2StateNo = -1;
    int p1Facing = 0;
    int p2Facing = 0;
    bool kill = true;
    bool guardKill = true;
    bool fallKill = true;
    int numHits = 1;
    int hitGetPower = 0;
    int guardGetPower = 0;
    int hitGivePower = 0;
    int guardGivePower = 0;
    int p1SprPriority = 1;
    int p2SprPriority = 0;
    int id = 0;
    int chainId = -1;
    bool p2GetP1State = true;
    bool downRecover = true;
    int downRecoverTime = -1; // -1: the target's liedown.time
    int guardDist = -1;       // -1: the attacker's size.attack.dist
    // sparkNo: fightfx unless own, -1 none, -2 the character's default.
    int sparkNo = -2;
    bool sparkOwn = false;
    int guardSparkNo = -2;
    bool guardSparkOwn = false;
    float sparkX = 0;
    float sparkY = 0;
    int hitSound[2] = {-1, 0};
    bool hitSoundOwn = false;
    int guardSound[2] = {-1, 0};
    bool guardSoundOwn = false;
    int envShakeTime = 0;
    float envShakeFreq = 60;
    int envShakeAmpl = -4;
    float envShakePhase = 0;
    int palFxTime = 0;
    std::array<int, 3> palFxAdd{0, 0, 0};
    std::array<int, 3> palFxMul{256, 256, 256};
    std::array<int, 3> palFxSinAdd{0, 0, 0};
    int palFxSinPeriod = 0;
    float palFxColor = 256;
    bool palFxInvertAll = false;
    int fallEnvShakeTime = 0;
    float fallEnvShakeFreq = 60;
    int fallEnvShakeAmpl = -4;
    float fallEnvShakePhase = 0;
};

// What the last hit left on the character that took it (GetHitVar).
struct GetHitVars
{
    bool guarded = false;
    HitType type = HitType::None;
    HitType groundType = HitType::None;
    HitType airType = HitType::None;
    AnimType animType = AnimType::Light;
    AnimType groundAnimType = AnimType::Light;
    AnimType airAnimType = AnimType::Light;
    AnimType fallAnimType = AnimType::Back;
    int damage = 0;
    int hitCount = 0;
    int fallCount = 0;
    int hitShakeTime = 0;
    int hitTime = 0;
    int slideTime = 0;
    int ctrlTime = 0;
    int recoverTime = 0;
    float xvel = 0; // toward the screen. HitVelSet turns it to the character's facing.
    float yvel = 0;
    float yAccel = 0;
    bool fall = false;
    int fallDamage = 0;
    float fallXVelocity = std::nanf("");
    float fallYVelocity = 0;
    bool fallRecover = true;
    int fallRecoverTime = 0;
    int fallTime = 0;
    bool fallKill = true;
    int hitId = 0;
    bool kill = true;
    bool downRecover = true;
    int downRecoverTime = -1;
    int fallEnvShakeTime = 0;
    float fallEnvShakeFreq = 60;
    int fallEnvShakeAmpl = -4;
    float fallEnvShakePhase = 0;
};

} // namespace OFS::Mugen::Runtime
