#pragma once

#include "core/CoreExport.h"
#include "core/bg/BgDocument.h"

#include <QList>
#include <QPointF>

namespace OFS {

struct AirAnimation;

struct OFS_CORE_EXPORT BgState
{
    bool visible = true;
    bool enabled = true;
    QPointF position;
    QPointF velocity;
    int action = -1;
    int actionTicks = 0;
    // Sine waves on x and y: amplitude, period in ticks, time in the period.
    double radius[2] = {0, 0};
    int period[2] = {0, 0};
    int sineTime[2] = {0, 0};
};

// Runs a background group tick by tick like the engine: controllers act first, then elements move.
// Linked elements (positionlink) follow another element's start, delta, velocity and sine.
class OFS_CORE_EXPORT BgSimulator
{
public:
    explicit BgSimulator(const BgGroup& group);

    void reset();
    void step();
    void runTo(int tick);
    int tick() const { return m_tick; }
    const QList<BgState>& states() const { return m_states; }
    const QList<BgElement>& elements() const { return m_elements; }

    QPointF sineOffset(int element) const;
    // Start + movement + sine, for a camera at (0, 0).
    QPointF position(int element) const;
    bool shown(int element) const;

    static int animationFrame(const AirAnimation& animation, int ticks);

private:
    bool active(const BgController& c, int loop) const;
    void run(const BgController& c, const QList<int>& targets);

    BgGroup m_group;
    QList<BgElement> m_elements;
    QList<int> m_links;
    QList<BgState> m_states;
    int m_tick = 0;
};

} // namespace OFS
