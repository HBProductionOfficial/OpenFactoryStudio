#pragma once

#include "engines/mugen/runtime/AnimPlayer.h"
#include "engines/mugen/runtime/Effects.h"
#include "engines/mugen/runtime/Hits.h"

#include <QList>

#include <array>

namespace OFS::Mugen::Runtime {

class CharacterData;

struct Projectile
{
    enum class Status
    {
        Active,
        Hit,
        Cancelled,
        Removed,
    };

    struct Anim
    {
        int number = -1;
        bool common = false;
    };

    int id = 0;
    int ownerId = 0;
    HitDef hitDef;
    Status status = Status::Active;

    Anim anim{0, false};
    Anim hitAnim;
    Anim remAnim;
    Anim cancelAnim;
    Anim shown{0, false};
    const CharacterData* animData = nullptr;
    AnimPlayer player;

    // In game units. Its bounds are in its maker's units (`unit` game units each).
    Vec2 pos;
    Vec2 vel;
    Vec2 remVel;
    Vec2 accel;
    Vec2 velMul{1, 1};
    Vec2 scale{1, 1};
    int facing = 1;
    float unit = 1;

    int hits = 1;
    int missTime = 0;
    int curMissTime = 0;
    int priority = 1;
    int priorityPoints = 1;
    int sprPriority = 3;
    int edgeBound = 40;
    int stageBound = 40;
    int heightBound[2] = {-240, 1};
    int removeTime = -1; // -1 forever
    bool remove = true;  // goes away when its hits are used up
    int superMoveTime = 0;
    int pauseMoveTime = 0;
    bool ownPal = false; // its owner's PalFX and RemapPal do not reach it (1.1)
    PaletteRemap remap;
    std::array<int, 3> shadow{0, 0, 0};
    AfterImage afterImage;
    QList<AfterImageFrame> afterImages;
    int afterImageTicks = 0;

    int time = 0;
    int hitPause = 0;
    bool frozen = false;
    bool paused = false;
    bool contact = false;
    bool removeStarted = false;
    bool gone = false;
};

} // namespace OFS::Mugen::Runtime
