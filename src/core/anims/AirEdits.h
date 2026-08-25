#pragma once

#include "core/anims/AirDocument.h"

class QTextDocument;

namespace OFS::AirEdits {

// Edits of an AIR text for the Animations editor. Like Fighter Factory, each one rewrites only the
// lines it touches. Animations and frames are positions in AirDocument::parse(lines).

// Writes the lines as one undo step, replacing only the lines that differ.
OFS_CORE_EXPORT bool apply(QTextDocument* document, const QStringList& lines);
OFS_CORE_EXPORT QStringList lines(const QTextDocument* document);

// The name is the "; name" line right above the header. An empty name removes it.
OFS_CORE_EXPORT void setName(QStringList& lines, int animation, const QString& name);
OFS_CORE_EXPORT void setNumber(QStringList& lines, int animation, int number);

OFS_CORE_EXPORT void setFrame(QStringList& lines, int animation, int frame, const AirFrame& value);
// One LoopStart per animation, so setting it moves it.
OFS_CORE_EXPORT void setLoopStart(QStringList& lines, int animation, int frame, bool on);
OFS_CORE_EXPORT void setInterpolation(QStringList& lines, int animation, int frame, int flag, bool on);
enum class Field
{
    Offset,
    Time,
    Flip,
    Transparency,
    Scale, // the angle is left as it is
};
OFS_CORE_EXPORT void applyToNext(QStringList& lines, int animation, int frame, Field field);

OFS_CORE_EXPORT int addAnimation(QStringList& lines, int after, int number, const QString& name,
                                 const QList<AirFrame>& frames);
OFS_CORE_EXPORT void deleteAnimation(QStringList& lines, int animation);
OFS_CORE_EXPORT int duplicateAnimation(QStringList& lines, int animation, int number);
OFS_CORE_EXPORT QStringList animationText(const QStringList& lines, int animation);
OFS_CORE_EXPORT void replaceAnimation(QStringList& lines, int animation, const QStringList& text);

OFS_CORE_EXPORT int insertFrames(QStringList& lines, int animation, int after, const QList<AirFrame>& frames);
OFS_CORE_EXPORT AirFrame blankFrame(int time = 10);
OFS_CORE_EXPORT void deleteFrame(QStringList& lines, int animation, int frame);

struct OFS_CORE_EXPORT BoxRef
{
    int block = -1;
    int box = -1;
    bool isValid() const { return block >= 0 && box >= 0; }
    friend bool operator==(const BoxRef& a, const BoxRef& b) { return a.block == b.block && a.box == b.box; }
};
OFS_CORE_EXPORT BoxRef addBox(QStringList& lines, int animation, int frame, int type, const AirBox& box);
OFS_CORE_EXPORT void setBox(QStringList& lines, int animation, int frame, BoxRef ref, const AirBox& box);
// The rest of a block is renumbered from 0. An emptied block goes with its header.
OFS_CORE_EXPORT void deleteBoxes(QStringList& lines, int animation, int frame, const QList<BoxRef>& refs);
OFS_CORE_EXPORT void deleteFrameBoxes(QStringList& lines, int animation, int frame);
OFS_CORE_EXPORT void deleteAnimationBoxes(QStringList& lines, int animation);
OFS_CORE_EXPORT BoxRef switchBoxType(QStringList& lines, int animation, int frame, BoxRef ref);
OFS_CORE_EXPORT void switchBoxDefault(QStringList& lines, int animation, int frame, BoxRef ref);
OFS_CORE_EXPORT void copyPreviousBoxes(QStringList& lines, int animation, int frame);
OFS_CORE_EXPORT int replicateBoxes(QStringList& lines, int animation, int frame);
OFS_CORE_EXPORT void pasteBoxes(QStringList& lines, int animation, int frame, int type, const QList<AirBox>& boxes);

enum class AlignOperation
{
    OffsetAxis,
    OffsetBoxes,
    ResizeTopLeft,     // left -= x, top -= y
    ResizeBottomRight, // right += x, bottom += y
    ScaleBoxes,        // x and y in percent, of the inclusive box (at least 1 unit wide and high)
};
OFS_CORE_EXPORT void align(QStringList& lines, const QList<int>& animations, AlignOperation operation, int x, int y);

// Removes commented-out animations and merges runs of identical regular boxes into one default
// block where no later frame changes what it shows (Fighter Factory did not check that).
OFS_CORE_EXPORT int optimize(QStringList& lines);

OFS_CORE_EXPORT QString boxText(int type, int index, const AirBox& box);
OFS_CORE_EXPORT QString blockHeaderText(int type, bool isDefault, int count);

} // namespace OFS::AirEdits
