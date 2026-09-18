#pragma once

#include <QWheelEvent>

namespace OFS {

// Adds up the partial notches (deltas below 120) of high-resolution wheels and touchpads.
class WheelSteps
{
public:
    int take(const QWheelEvent* event)
    {
        m_rest += event->angleDelta().y();
        const int steps = m_rest / QWheelEvent::DefaultDeltasPerStep;
        m_rest -= steps * QWheelEvent::DefaultDeltasPerStep;
        return steps;
    }

private:
    int m_rest = 0;
};

} // namespace OFS
