#include "engines/mugen/runtime/AnimPlayer.h"

#include <algorithm>

namespace OFS::Mugen::Runtime {

AnimPlayer::AnimPlayer(const QList<int>& times, int loopStart)
    : m_times(times)
    , m_loopStart(loopStart >= 0 && loopStart < times.size() ? loopStart : 0)
{
    if (m_times.isEmpty()) {
        m_totalTime = 0;
    } else if (m_times.last() == -1) {
        m_totalTime = -1;
    } else {
        int before = 0;
        for (int i = 0; i < m_times.size(); ++i) {
            const int t = m_times[i];
            if (t == -1) {
                m_totalTime = 0;
                m_loopTime = -before;
                m_preLoopTime = 0;
            }
            m_totalTime += t;
            if (i < m_loopStart) {
                m_preLoopTime += t;
                before += t;
            } else {
                m_loopTime += t;
            }
        }
        if (m_totalTime == -1)
            m_preLoopTime = 0;
    }
    reset();
}

void AnimPlayer::reset()
{
    m_frame = 0;
    m_frameTime = 0;
    m_time = 0;
    m_ended = false;
    updateFrame();
}

void AnimPlayer::setElement(int element, int elementTime)
{
    if (m_times.isEmpty())
        return;
    m_frame = std::max(0, element - 1);
    if (m_frame >= m_times.size())
        m_frame = 0; // the engine goes back to the first element
    const int length = m_times[m_frame];
    if (elementTime < 0 || (length != -1 && elementTime >= length))
        elementTime = 0;
    m_frameTime = elementTime;
    m_ended = false;
    m_time = 0;
    m_time = -animElemTime(m_frame + 1) + m_frameTime;
}

int AnimPlayer::animElemTime(int element) const
{
    if (element > m_times.size())
        return std::min(0, animTime());
    int t = m_time;
    for (int i = 0; i < std::max(0, element) - 1; ++i)
        t -= std::max(0, m_times[i]);
    return t;
}

int AnimPlayer::animElemNo(int offset) const
{
    if (m_times.isEmpty())
        return 0;
    int i = m_frame;
    int previous = 0;
    const int count = int(m_times.size());
    if (offset <= 0) {
        int time = offset + m_frameTime;
        bool looped = false;
        for (;;) {
            if (time >= 0)
                return i + 1;
            --i;
            if (i < 0 || (m_frame >= m_loopStart && i < m_loopStart)) {
                if (time == previous)
                    break;
                previous = time;
                looped = true;
                i = count - 1;
            }
            time += std::max(0, m_times[i]);
            if (looped && i == count - 1 && m_times[i] == -1)
                return i + 1;
        }
    } else {
        int time = offset + m_frameTime;
        for (;;) {
            time -= std::max(0, m_times[i]);
            if (time < 0 || (i == count - 1 && m_times[i] == -1))
                return i + 1;
            ++i;
            if (i >= count) {
                if (time == previous)
                    break;
                previous = time;
                i = m_loopStart;
            }
        }
    }
    return count;
}

void AnimPlayer::seek(int frame)
{
    // Frames with no time are skipped. An endless last frame stops the search.
    const int count = int(m_times.size());
    bool second = false;
    for (;;) {
        m_frame = std::max(0, frame);
        while (m_frame < count && m_times[m_frame] <= 0) {
            if (m_frame == count - 1 && m_times[m_frame] == -1)
                break;
            ++m_frame;
        }
        if (m_frame < count)
            break;
        if (second) {
            m_frame = count - 1;
            break;
        }
        second = true;
        frame = m_loopStart;
    }
    m_frame = std::clamp(m_frame, 0, std::max(0, count - 1));
}

void AnimPlayer::updateFrame()
{
    if (m_times.isEmpty() || m_totalTime <= 0)
        return;
    if (m_time >= m_totalTime) {
        m_frameTime = 0;
        m_frame = m_loopStart;
    }
    seek(m_frame);
    if (m_preLoopTime < 0 && m_time >= m_totalTime + m_preLoopTime && m_time >= m_totalTime - m_loopTime &&
        (m_time == m_totalTime + m_preLoopTime || m_time == m_totalTime - m_loopTime)) {
        m_frameTime = 0;
        m_frame = 0;
    }
}

void AnimPlayer::step()
{
    if (m_times.isEmpty())
        return;
    updateFrame();
    const int count = int(m_times.size());
    auto next = [this, count] {
        if (m_totalTime != -1 || m_frame < count - 1) {
            m_frameTime = 0;
            for (;;) {
                ++m_frame;
                if ((m_totalTime == -1 && m_frame == count - 1) || m_frame >= count || m_times[m_frame] > 0)
                    break;
            }
        }
    };
    if (m_times[m_frame] <= 0)
        next();
    if (m_frame < count) {
        ++m_frameTime;
        if (m_frameTime >= m_times[m_frame]) {
            next();
            if (m_frame >= count)
                m_frame = m_loopStart;
        }
    } else {
        m_frame = m_loopStart;
    }
    if (m_totalTime != -1 && m_time >= m_totalTime)
        m_time = m_totalTime - m_loopTime;
    ++m_time;
    if (m_totalTime != -1 && m_time >= m_totalTime)
        m_ended = true;
}

} // namespace OFS::Mugen::Runtime
