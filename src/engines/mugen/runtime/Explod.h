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
    Vec2 pos;        // a character's position or the screen's corner, in game units
    Vec2 relative;
    Vec2 next;
    Vec2 vel;
    Vec2 accel;
    float unit = 1; // game units per unit of relative, vel and accel (its maker's localcoord)
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
    float angle = 0;
    // Turned about its x and y axes (1.1), in the engine's fixed perspective.
    float xAngle = 0;
    float yAngle = 0;
    int sprPriority = 0;
    bool onTop = false;
    bool ownPal = false;
    PaletteRemap remap;
    std::array<int, 3> shadow{0, 0, 0}; // the colour its shadow takes away (0,0,0: none, r -1: the stage's)
    QString trans = QStringLiteral("default");
    std::array<int, 2> alpha{256, 0};

    int time = 0;
    bool paused = false;

    Vec2 drawPos() const { return Vec2{pos.x + relative.x * unit, pos.y + relative.y * unit}; }
    int drawFacing() const { return facing * relativeFacing; }
};

} // namespace OFS::Mugen::Runtime
