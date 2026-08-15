#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace OFS::Mugen {

// One element of an animation: "group, index, x, y, time, flip".
struct AirFrame
{
    int group = 0;
    int index = 0;
    int x = 0;
    int y = 0;
    int time = 0;
    QString flip; // "", "H", "V" or "HV" as written
    QStringList extra;
    QString indent;
    QString comment;
    int line = -1;

    // "G,I, X,Y, T" plus ", flip" and any further fields, keeping the indent and comment.
    QString text() const;
};

struct AirAction
{
    int number = 0;
    QString name; // the comment line above the header
    int line = -1;
    QList<AirFrame> frames;

    QString caption() const;
    bool flipsHorizontally(int element) const;
    bool flipsVertically(int element) const;
};

// Collision boxes, loop starts and interpolation lines are skipped.
QList<AirAction> readAnimations(const QString& text);

const AirAction* findAnimation(const QList<AirAction>& actions, int number);

// An element with time -1 lasts forever, so later ones never start.
constexpr qint64 Never = -1;
qint64 elementStart(const AirAction& action, int element);
// After the end it stays on the last element (no looping).
int elementAt(const AirAction& action, qint64 tick);

} // namespace OFS::Mugen
