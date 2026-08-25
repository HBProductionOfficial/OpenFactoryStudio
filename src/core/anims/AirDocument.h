#pragma once

#include "core/CoreExport.h"

#include <QList>
#include <QRect>
#include <QString>
#include <QStringList>

namespace OFS {

// A Clsn box in sprite units from the player axis.
struct OFS_CORE_EXPORT AirBox
{
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    int index = 0;
    int line = -1;

    AirBox normalized() const;
    friend bool operator==(const AirBox& a, const AirBox& b)
    {
        return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
    }
};

struct OFS_CORE_EXPORT AirBoxBlock
{
    int type = 1; // 1 attack, 2 collision
    bool isDefault = false;
    int line = -1;
    QList<AirBox> boxes;
};

struct OFS_CORE_EXPORT AirTransparency
{
    enum class Mode
    {
        None,
        Addition,
        Addition50, // "A1"
        Subtraction,
    };
    Mode mode = Mode::None;
    int source = 256;
    int dest = 256;

    // "", "A", "A1", "AS<s>D<d>" or "S".
    QString text() const;
    static AirTransparency parse(const QString& field);
    friend bool operator==(const AirTransparency& a, const AirTransparency& b) { return a.text() == b.text(); }
};

// One element (frame) with the box, LoopStart and Interpolate lines written above it.
struct OFS_CORE_EXPORT AirFrame
{
    enum Interpolation
    {
        InterpolateOffset = 1,
        InterpolateBlend = 2,
        InterpolateScale = 4,
        InterpolateAngle = 8,
    };

    int group = -1;
    int number = 0;
    int x = 0;
    int y = 0;
    int time = 0;
    bool flipH = false;
    bool flipV = false;
    AirTransparency transparency;
    double xScale = 1.0;
    double yScale = 1.0;
    double angle = 0.0;
    bool loopStart = false;
    int interpolate = 0;

    QList<AirBoxBlock> blocks;
    int line = -1;
    int sectionStart = -1;
    QList<int> loopStartLines;
    QList<int> interpolateLines;
    QString indent;
    QString comment;

    // Element line as FFS writes it: "g,i, x,y, t", the optional fields up to the last non-default
    // one, then the comment.
    QString text() const;
    bool hasBoxes(int type) const;
};

struct OFS_CORE_EXPORT AirAnimation
{
    int number = 0;
    QString name; // the comment line right above the header
    int nameLine = -1;
    int headerLine = -1;
    int lastLine = -1;
    int endLine = -1;
    QList<AirFrame> frames;

    QString caption() const;
    // Boxes as MUGEN uses them: own regular blocks, else own default, else the last earlier default.
    QList<AirBox> boxes(int frame, int type, bool* isDefault = nullptr, bool* inherited = nullptr) const;
    // Sum of the times up to the frame's end, -1 once an endless frame came before.
    int endTick(int frame) const;
    int loopStartFrame() const;
};

class OFS_CORE_EXPORT AirDocument
{
public:
    static AirDocument parse(const QStringList& lines);
    static AirDocument parse(const QString& text);

    const QList<AirAnimation>& animations() const { return m_animations; }
    int count() const { return int(m_animations.size()); }
    int indexOf(int number) const;
    bool locate(int line, int* animation, int* frame) const;
    struct Problem
    {
        int line = 0;
        QString message;
    };
    const QList<Problem>& problems() const { return m_problems; }

private:
    QList<AirAnimation> m_animations;
    QList<Problem> m_problems;
};

} // namespace OFS
