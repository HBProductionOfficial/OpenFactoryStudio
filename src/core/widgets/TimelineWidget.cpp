#include "core/widgets/TimelineWidget.h"

#include "core/widgets/Rulers.h"

#include <QMouseEvent>
#include <QPainter>

#include <cmath>

namespace OFS {

namespace {

constexpr int NameWidth = 120;
constexpr int RulerHeight = 18;

int length(const TimelineWidget::Frame& frame)
{
    return frame.time < 0 ? TimelineWidget::EndlessLength : std::max(1, frame.time);
}

} // namespace

TimelineWidget::TimelineWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("timeline"));
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus); // a click takes the keyboard from the panel, like Fighter Factory
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(60);
}

void TimelineWidget::setAnimation(const QString& name, const QList<Frame>& frames)
{
    m_name = name;
    m_frames = frames;
    update();
}

void TimelineWidget::setPlayhead(int tick)
{
    if (tick == m_playhead)
        return;
    m_playhead = tick;
    if (m_tickMode) {
        // Keep the play-head in view, a page at a time.
        const int visible = std::max(1, int((width() - trackLeft() - 4) / tickWidth()));
        if (tick < m_firstTick || tick >= m_firstTick + visible)
            m_firstTick = std::max(0, tick - (tick < m_firstTick ? 0 : visible / 4));
    }
    update();
}

void TimelineWidget::setTickMode(bool on)
{
    m_tickMode = on;
    m_firstTick = 0;
    update();
}

int TimelineWidget::tickAt(int x) const
{
    if (x < trackLeft())
        return -1;
    return m_firstTick + int((x - trackLeft()) / tickWidth());
}

int TimelineWidget::trackLeft() const
{
    return NameWidth;
}

double TimelineWidget::tickWidth() const
{
    if (m_tickMode)
        return 6.0;
    int total = 0;
    for (const Frame& frame : m_frames)
        total += length(frame);
    const int track = std::max(1, width() - trackLeft() - 4);
    return std::clamp(total > 0 ? double(track) / total : 8.0, 2.0, 8.0);
}

int TimelineWidget::frameAt(int x) const
{
    const double unit = tickWidth();
    double left = trackLeft();
    for (int i = 0; i < m_frames.size(); ++i) {
        const double right = left + length(m_frames.at(i)) * unit;
        if (x >= left && x < right)
            return i;
        left = right;
    }
    return -1;
}

void TimelineWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    const Rulers::Colours colours = Rulers::Colours::from(palette());
    p.fillRect(rect(), palette().color(QPalette::Base));
    const double unit = tickWidth();
    const int left = trackLeft();
    p.fillRect(QRect(0, 0, width(), RulerHeight), colours.band);
    p.setPen(colours.ink);
    int step = 5;
    while (step * unit < 24)
        step += 5;
    const int first = m_tickMode ? m_firstTick : 0;
    for (int tick = first; left + (tick - first) * unit < width(); ++tick) {
        const double x = left + (tick - first) * unit;
        if (tick % step == 0) {
            p.drawLine(QPointF(x, RulerHeight - 6), QPointF(x, RulerHeight));
            p.drawText(QPointF(x + 2, RulerHeight - 6), QString::number(tick));
        } else if (unit >= 4) {
            p.drawLine(QPointF(x, RulerHeight - 3), QPointF(x, RulerHeight));
        }
    }
    const QRect body(0, RulerHeight, width(), height() - RulerHeight);
    p.drawText(QRect(4, body.top(), left - 8, body.height()), Qt::AlignVCenter | Qt::AlignLeft,
               p.fontMetrics().elidedText(m_name, Qt::ElideRight, left - 8));
    double x = left;
    for (int i = 0; i < m_frames.size(); ++i) {
        const Frame& frame = m_frames.at(i);
        const QRectF block(x, body.top() + 6, length(frame) * unit, body.height() - 12);
        const QColor color =
            frame.loopStart ? QColor(70, 150, 70) : (i % 2 ? QColor(150, 128, 40) : QColor(120, 100, 30));
        p.fillRect(block, color);
        p.setPen(QColor(20, 20, 20));
        p.drawRect(block);
        if (frame.time < 0) {
            p.setPen(QColor(230, 230, 230));
            p.drawText(block, Qt::AlignCenter, QString(QChar(0x221E))); // infinity
        }
        x += block.width();
    }
    p.setPen(QPen(QColor(255, 220, 0), 2));
    p.drawLine(QPointF(playheadX(), 0), QPointF(playheadX(), height()));
}

double TimelineWidget::playheadX() const
{
    return trackLeft() + (m_playhead - (m_tickMode ? m_firstTick : 0)) * tickWidth();
}

void TimelineWidget::scrubTo(int x)
{
    const int target = m_tickMode ? tickAt(x) : frameAt(x);
    if (target < 0 || target == m_scrubbed)
        return;
    m_scrubbed = target;
    if (m_tickMode)
        Q_EMIT tickClicked(target);
    else
        Q_EMIT frameClicked(target);
}

void TimelineWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_scrubbed = -1;
        scrubTo(event->position().toPoint().x());
    }
    QWidget::mousePressEvent(event);
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* event)
{
    const int x = event->position().toPoint().x();
    if (event->buttons() & Qt::LeftButton)
        scrubTo(x);
    // Same as Fighter Factory: left-right arrows over the play-head only.
    const bool onHead = std::abs(x - playheadX()) <= 4;
    if (onHead != (cursor().shape() == Qt::SizeHorCursor))
        setCursor(onHead ? Qt::SizeHorCursor : Qt::ArrowCursor);
    QWidget::mouseMoveEvent(event);
}

} // namespace OFS
