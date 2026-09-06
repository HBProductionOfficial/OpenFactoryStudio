#pragma once

#include <QList>

namespace OFS::Mugen::Runtime {

// Plays an animation the way the engine counts time: frame times in ticks (-1 lasts forever, 0 is
// skipped), looping back to LoopStart. Gives what AnimTime, AnimElem, AnimElemTime and AnimElemNo read.
class AnimPlayer
{
public:
    AnimPlayer() = default;
    AnimPlayer(const QList<int>& times, int loopStart);

    void reset();
    void setElement(int element, int elementTime = 0);
    void step();

    bool isEmpty() const { return m_times.isEmpty(); }
    int frameCount() const { return int(m_times.size()); }
    int frame() const { return m_frame; }
    int frameTime() const { return m_frameTime; }
    // Negative until the last tick, then 0 (and above for an animation that ends on an endless frame).
    int animTime() const { return m_time - m_totalTime; }
    int animElemTime(int element) const;
    int animElemNo(int offset) const;
    // -1 when it ends on an endless frame.
    int totalTime() const { return m_totalTime; }
    bool ended() const { return m_ended; }

private:
    void seek(int frame);
    void updateFrame();

    QList<int> m_times;
    int m_loopStart = 0;
    int m_totalTime = 0;
    int m_loopTime = 0;
    int m_preLoopTime = 0;
    int m_frame = 0;
    int m_frameTime = 0;
    int m_time = 0;
    bool m_ended = false;
};

} // namespace OFS::Mugen::Runtime
