#pragma once

#include "engines/mugen/runtime/AnimPlayer.h"
#include "engines/mugen/runtime/CharacterData.h"
#include "engines/mugen/runtime/Effects.h"

#include <QList>
#include <QRectF>
#include <QSet>
#include <QString>

#include <climits>

// Shared by the parts of the character runtime, not part of its interface.
namespace OFS::Mugen::Runtime::Detail {

constexpr int MaxLoop = 2500;    // state changes in one tick, as the engine stops runaway loops
constexpr int Never = INT32_MAX; // the counter of a "persistent = 0" controller that already ran

QChar letterOf(const Param* param, QChar fallback);
const QSet<QString>& pendingControllers();
AnimPlayer animPlayerFor(const AirAnimation& animation);
QList<QRectF> clsnBoxes(const AirAnimation* animation, const AnimPlayer& player, int type, Vec2 pos, int facing);
// Boxes that only touch count as meeting, as in the engine.
bool boxesMeet(const QList<QRectF>& a, const QList<QRectF>& b);

} // namespace OFS::Mugen::Runtime::Detail
