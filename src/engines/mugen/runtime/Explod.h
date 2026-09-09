#pragma once

#include "engines/mugen/runtime/AnimPlayer.h"
#include "engines/mugen/runtime/Effects.h"

#include <QString>

#include <array>

namespace OFS::Mugen::Runtime {

class Character;
class CharacterData;

struct Explod
{
    enum class PosType
    {
        P1,
        P2,
        Front,
        Back,
        Left,
        Right,
        None, // pos is where it goes (1.1): on the stage, or on the screen
    };

    int id = -1;
    int ownerId = 0;
    int animNo = 0;
    bool common = false;
    const CharacterData* animData = nullptr; // whose animations (nullptr: fightfx)
    AnimPlayer anim;

    PosType posType = PosType::P1;
    bool screen = false;
    bool screenSpace = false;
    int bindId = -2; // 1.1 bindID: the player or helper it follows, -1 its owner, -2 none
    Vec2 pos;                 // a character's position or the screen's corner, in game units
    Vec2 relative;
    Vec2 next;
    Vec2 vel;
    Vec2 accel;
    int facing = 1;
    int relativeFacing = 1;
    int vFacing = 1;
    Character* bindTo = nullptr;
    int bindTime = 0; // -1 forever

    int removeTime = -2; // -2 when the animation ends, -1 never
    int superMoveTime = 0;
    int pauseMoveTime = 0;
    bool removeOnGetHit = false;
    bool ignoreHitPause = true;

    Vec2 scale{1, 1};
    int sprPriority = 0;
    bool onTop = false;
    bool ownPal = false;
    QString trans = QStringLiteral("default");
    std::array<int, 2> alpha{256, 0};

    int time = 0;
    bool paused = false;

    Vec2 drawPos() const { return Vec2{pos.x + relative.x, pos.y + relative.y}; }
    int drawFacing() const { return facing * relativeFacing; }
};

} // namespace OFS::Mugen::Runtime
