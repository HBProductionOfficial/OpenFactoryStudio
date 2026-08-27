#include "core/widgets/WaveformView.h"

#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

#include <cmath>

namespace OFS {

using namespace Formats;

namespace {

constexpr int RulerHeight = 18;
constexpr double MinimumFramesPerPixel = 1.0 / 16.0;

} // namespace

WaveformView::WaveformView(QWidget* parent)
    : QWidget(parent)
    , m_scroll(new QScrollBar(Qt::Horizontal, this))
{
    setObjectName(QStringLiteral("waveform"));
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(m_scroll, &QScrollBar::valueChanged, this, [this](int value) { setFirstFrame(value); });
}

void WaveformView::setBuffer(const SampleBuffer& buffer)
{
    m_buffer = buffer;
    m_peaks.build(m_buffer);
    const qint64 frames = qint64(m_buffer.frameCount());
    m_cursor = std::clamp<qint64>(m_cursor, 0, frames);
    if (m_selection.end() > std::size_t(frames))
        m_selection = SampleRange{};
    if (!m_fitted && width() > 0) {
        m_fitted = true;
        zoomFit();
    }
    setFramesPerPixel(m_framesPerPixel);
    update();
}

QRect WaveformView::waveArea() const
{
    return QRect(0, RulerHeight, width(), std::max(1, height() - RulerHeight - m_scroll->sizeHint().height()));
}

double WaveformView::maximumFramesPerPixel() const
{
    return std::max(MinimumFramesPerPixel * 2, double(m_buffer.frameCount()) / std::max(1, width()));
}

void WaveformView::setFramesPerPixel(double frames)
{
    frames = std::clamp(frames, MinimumFramesPerPixel, std::max(MinimumFramesPerPixel, maximumFramesPerPixel()));
    if (frames != m_framesPerPixel) {
        m_framesPerPixel = frames;
        Q_EMIT zoomChanged(m_framesPerPixel);
    }
    setFirstFrame(m_firstFrame);
    updateScrollBar();
    update();
}

void WaveformView::zoomIn()
{
    const double anchor = m_cursor;
    const double offset = anchor - m_firstFrame;
    setFramesPerPixel(m_framesPerPixel / 2);
    setFirstFrame(anchor - offset / 2);
}

void WaveformView::zoomOut()
{
    const double anchor = m_cursor;
    const double offset = anchor - m_firstFrame;
    setFramesPerPixel(m_framesPerPixel * 2);
    setFirstFrame(anchor - offset * 2);
}

void WaveformView::zoomFit()
{
    m_firstFrame = 0;
    setFramesPerPixel(maximumFramesPerPixel());
}

void WaveformView::zoomReset()
{
    setFramesPerPixel(1.0);
}

void WaveformView::setFirstFrame(double frame)
{
    const double visible = width() * m_framesPerPixel;
    const double last = std::max(0.0, double(m_buffer.frameCount()) - visible);
    frame = std::clamp(frame, 0.0, last);
    if (frame == m_firstFrame)
        return;
    m_firstFrame = frame;
    updateScrollBar();
    update();
}

void WaveformView::updateScrollBar()
{
    const QSignalBlocker blocker(m_scroll);
    const double visible = width() * m_framesPerPixel;
    const int maximum = int(std::max(0.0, double(m_buffer.frameCount()) - visible));
    m_scroll->setRange(0, maximum);
    m_scroll->setPageStep(std::max(1, int(visible)));
    m_scroll->setSingleStep(std::max(1, int(visible / 10)));
    m_scroll->setValue(int(m_firstFrame));
}

qint64 WaveformView::frameAt(int x) const
{
    return std::clamp<qint64>(qint64(std::lround(m_firstFrame + x * m_framesPerPixel)), 0,
                              qint64(m_buffer.frameCount()));
}

double WaveformView::xAt(double frame) const
{
    return (frame - m_firstFrame) / m_framesPerPixel;
}

void WaveformView::setCursor(qint64 frame)
{
    frame = std::clamp<qint64>(frame, 0, qint64(m_buffer.frameCount()));
    if (frame == m_cursor)
        return;
    m_cursor = frame;
    update();
    Q_EMIT cursorChanged(m_cursor);
}

void WaveformView::setSelection(SampleRange range)
{
    if (range.end() > m_buffer.frameCount())
        range.count = range.start < m_buffer.frameCount() ? m_buffer.frameCount() - range.start : 0;
    if (range.start == m_selection.start && range.count == m_selection.count)
        return;
    m_selection = range;
    update();
    Q_EMIT selectionChanged(m_selection);
}

void WaveformView::setPlayPosition(qint64 frame)
{
    if (frame == m_play)
        return;
    m_play = frame;
    update();
}

void WaveformView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    const QRect area = waveArea();
    p.fillRect(rect(), QColor(30, 30, 30));
    p.fillRect(QRect(0, 0, width(), RulerHeight), QColor(45, 45, 45));
    p.setPen(QColor(190, 190, 190));
    const double rate = std::max<std::uint32_t>(1, m_buffer.sampleRate());
    const double secondsPerPixel = m_framesPerPixel / rate;
    double step = 0.001;
    for (double candidate : {0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 30.0, 60.0}) {
        step = candidate;
        if (candidate / secondsPerPixel >= 70)
            break;
    }
    const double firstSecond = m_firstFrame / rate;
    for (double t = std::floor(firstSecond / step) * step; xAt(t * rate) < width(); t += step) {
        const double x = xAt(t * rate);
        if (x < 0)
            continue;
        p.drawLine(QPointF(x, RulerHeight - 5), QPointF(x, RulerHeight));
        p.drawText(QPointF(x + 2, RulerHeight - 6), QString::number(t, 'f', step < 0.01 ? 3 : step < 1 ? 2 : 0));
    }
    if (!m_selection.empty()) {
        const double left = xAt(double(m_selection.start));
        const double right = xAt(double(m_selection.end()));
        p.fillRect(QRectF(left, area.top(), right - left, area.height()), QColor(70, 90, 140));
    }
    const int channels = std::max<int>(1, m_buffer.channels());
    const double laneHeight = double(area.height()) / channels;
    std::vector<PeakPair> columns(std::size_t(std::max(1, width())));
    for (int c = 0; c < channels; ++c) {
        const double middle = area.top() + laneHeight * (c + 0.5);
        const double half = laneHeight / 2 - 2;
        p.setPen(QColor(80, 80, 80));
        p.drawLine(QPointF(0, middle), QPointF(width(), middle));
        if (m_buffer.empty())
            continue;
        m_peaks.columns(m_buffer, std::uint16_t(c), m_firstFrame, m_framesPerPixel, columns);
        p.setPen(QColor(110, 200, 110));
        const double end = xAt(double(m_buffer.frameCount()));
        for (int x = 0; x < width() && x < end; ++x) {
            const PeakPair& peak = columns[std::size_t(x)];
            p.drawLine(QPointF(x + 0.5, middle - peak.max * half), QPointF(x + 0.5, middle - peak.min * half));
        }
    }
    p.setPen(QPen(QColor(255, 255, 255), 1));
    p.drawLine(QPointF(xAt(double(m_cursor)), area.top()), QPointF(xAt(double(m_cursor)), area.bottom()));
    if (m_play >= 0) {
        p.setPen(QPen(QColor(255, 220, 0), 1));
        p.drawLine(QPointF(xAt(double(m_play)), area.top()), QPointF(xAt(double(m_play)), area.bottom()));
    }
}

void WaveformView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    const int bar = m_scroll->sizeHint().height();
    m_scroll->setGeometry(0, height() - bar, width(), bar);
    if (!m_fitted && !m_buffer.empty()) {
        m_fitted = true;
        zoomFit();
    }
    setFramesPerPixel(m_framesPerPixel);
}

void WaveformView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !waveArea().contains(event->position().toPoint()))
        return QWidget::mousePressEvent(event);
    const qint64 frame = frameAt(event->position().toPoint().x());
    m_selecting = true;
    if (event->modifiers() & Qt::ShiftModifier) {
        m_anchor = m_cursor;
    } else {
        m_anchor = frame;
        setSelection(SampleRange{});
    }
    setCursor(frame);
    if (event->modifiers() & Qt::ShiftModifier)
        setSelection(SampleRange{std::size_t(std::min(m_anchor, frame)), std::size_t(std::abs(frame - m_anchor))});
}

void WaveformView::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_selecting)
        return;
    const qint64 frame = frameAt(event->position().toPoint().x());
    setSelection(SampleRange{std::size_t(std::min(m_anchor, frame)), std::size_t(std::abs(frame - m_anchor))});
}

void WaveformView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        m_selecting = false;
    QWidget::mouseReleaseEvent(event);
}

void WaveformView::wheelEvent(QWheelEvent* event)
{
    const int steps = event->angleDelta().y() / 120;
    if (steps == 0)
        return;
    if (event->modifiers() & Qt::ControlModifier) {
        // About the frame under the mouse.
        const double x = event->position().x();
        const double anchor = m_firstFrame + x * m_framesPerPixel;
        setFramesPerPixel(m_framesPerPixel * std::pow(0.5, steps));
        setFirstFrame(anchor - x * m_framesPerPixel);
    } else {
        setFirstFrame(m_firstFrame - steps * width() * m_framesPerPixel / 8);
    }
    event->accept();
}

} // namespace OFS
