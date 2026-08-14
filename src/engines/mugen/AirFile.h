#pragma once

#include <QList>
#include <QString>

namespace OFS::Mugen {

// One element of an animation: "group, index, x, y, time, flip".
struct AirFrame
{
    int group = 0;
    int index = 0;
    int x = 0;
    int y = 0;
    int time = 0;
    QString flip;  // "", "H", "V" or "HV" as written
    int line = -1;
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

} // namespace OFS::Mugen
