#include "core/widgets/WaveformView.h"

#include "core/GameTime.h"
#include "core/widgets/Rulers.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace OFS {

using namespace Formats;

namespace {

constexpr int RulerHeight = 20;
constexpr double MinimumFramesPerPixel = 1.0 / 16.0;

const QColor CentreLine(150, 150, 150);
const QColor Wave(230, 200, 40);
const QColor SelectionFill(240, 140, 30);
const QColor WaveInSelection(60, 40, 10);
const QColor CursorColor(40, 120, 255);

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
    if (!m_fitted && width() > 0 && frames > 0) {
        m_fitted = true;
        zoomFit();
    }
    updateScrollBar();
    update();
}

QRect WaveformView::waveArea() const
{
    return QRect(0, RulerHeight, width(), std::max(1, height() - RulerHeight - m_scroll->sizeHint().height()));
}

double WaveformView::fitFramesPerPixel() const
{
    return std::max(MinimumFramesPerPixel, double(m_buffer.frameCount()) / std::max(1, width()));
}

void WaveformView::setFramesPerPixel(double frames)
{
    // Zooming out may show space after the end of the sound.
    frames = std::clamp(frames, MinimumFramesPerPixel, std::max(MinimumFramesPerPixel, fitFramesPerPixel() * 64));
    if (frames != m_framesPerPixel) {
        m_framesPerPixel = frames;
        Q_EMIT zoomChanged(m_framesPerPixel);
    }
    updateScrollBar();
    update();
}

void WaveformView::zoomIn()
{
    setFramesPerPixel(m_framesPerPixel / 2);
}

void WaveformView::zoomOut()
{
    setFramesPerPixel(m_framesPerPixel * 2);
}

void WaveformView::zoomFit()
{
    m_firstFrame = 0;
    setFramesPerPixel(fitFramesPerPixel());
}

void WaveformView::zoomReset()
{
    setFramesPerPixel(1.0);
}

void WaveformView::setFirstFrame(double frame)
{
    frame = std::clamp(frame, 0.0, std::max(0.0, double(m_buffer.frameCount()) - 1));
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
    m_scroll->setRange(0, int(std::max(0.0, double(m_buffer.frameCount()) - visible)));
    m_scroll->setPageStep(std::max(1, int(visible)));
    m_scroll->setSingleStep(std::max(1, int(visible / 10)));
    m_scroll->setValue(int(m_firstFrame));
}

qint64 WaveformView::frameAt(int x) const
{
    return std::clamp<qint64>(qint64(std::floor(m_firstFrame + x * m_framesPerPixel)), 0,
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
    if (range.start > m_buffer.frameCount())
        range = SampleRange{m_buffer.frameCount(), 0};
    if (range.end() > m_buffer.frameCount())
        range.count = m_buffer.frameCount() - range.start;
    if (range.start == m_selection.start && range.count == m_selection.count)
        return;
    m_selection = range;
    update();
    Q_EMIT selectionChanged(m_selection);
}

void WaveformView::setWaveColor(const QColor& color)
{
    m_waveColor = color.isValid() ? color : Wave;
    update();
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
    const Rulers::Colours colours = Rulers::Colours::from(palette());
    p.fillRect(rect(), palette().color(QPalette::Base));
    p.fillRect(QRect(0, 0, width(), RulerHeight), colours.band);
    p.setPen(colours.ink);
    const double rate = std::max<std::uint32_t>(1, m_buffer.sampleRate());
    const double framesPerTick = rate / double(TicksPerSecond);
    const double pixelsPerTick = framesPerTick / m_framesPerPixel;
    int step = 1;
    for (int candidate : {1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000}) {
        step = candidate;
        if (candidate * pixelsPerTick >= 28)
            break;
    }
    const qint64 firstTick = qint64(std::floor(m_firstFrame / framesPerTick / step)) * step;
    for (qint64 tick = firstTick;; tick += step) {
        const double x = xAt(double(tick * framesPerTick));
        if (x > width())
            break;
        if (x < 0)
            continue;
        p.drawLine(QPointF(x, RulerHeight - 6), QPointF(x, RulerHeight));
        p.drawText(QPointF(x + 2, RulerHeight - 7), QString::number(tick));
    }
    const double soundEnd = xAt(double(m_buffer.frameCount()));
    const double selectionLeft = xAt(double(m_selection.start));
    const double selectionRight = xAt(double(m_selection.end()));
    if (!m_selection.empty())
        p.fillRect(QRectF(selectionLeft, area.top(), selectionRight - selectionLeft, area.height()), SelectionFill);
    // One band per channel. Positive samples go above the centre line.
    const int channels = std::max<int>(1, m_buffer.channels());
    const double band = double(area.height()) / channels;
    std::vector<PeakPair> columns(std::size_t(std::max(1, width())));
    for (int c = 0; c < channels; ++c) {
        const double middle = area.top() + band * (c + 0.5);
        const double half = band / 2 - 2;
        p.setPen(CentreLine);
        p.drawLine(QPointF(0, middle), QPointF(std::min<double>(width(), soundEnd), middle));
        if (m_buffer.empty())
            continue;
        m_peaks.columns(m_buffer, std::uint16_t(c), m_firstFrame, m_framesPerPixel, columns);
        QList<QLineF> outside;
        QList<QLineF> inside;
        outside.reserve(std::max(0, width()));
        for (int x = 0; x < width() && x < soundEnd; ++x) {
            const PeakPair& peak = columns[std::size_t(x)];
            const bool selected = !m_selection.empty() && x >= selectionLeft && x < selectionRight;
            (selected ? inside : outside)
                << QLineF(x + 0.5, middle - peak.max * half, x + 0.5, middle - peak.min * half);
        }
        p.setPen(m_waveColor);
        p.drawLines(outside);
        p.setPen(WaveInSelection);
        p.drawLines(inside);
        if (band > 8 && c > 0) {
            const QColor base = palette().color(QPalette::Base);
            const QColor ink = palette().color(QPalette::Text);
            p.setPen(QColor((base.red() * 2 + ink.red()) / 3, (base.green() * 2 + ink.green()) / 3,
                            (base.blue() * 2 + ink.blue()) / 3));
            p.drawLine(QPointF(0, area.top() + band * c), QPointF(width(), area.top() + band * c));
        }
    }
    if (soundEnd < width()) {
        p.setPen(CentreLine);
        p.drawLine(QPointF(soundEnd, area.top()), QPointF(soundEnd, area.bottom()));
    }
    p.setPen(QPen(CursorColor, 1));
    p.drawLine(QPointF(xAt(double(m_cursor)), area.top()), QPointF(xAt(double(m_cursor)), area.bottom()));
    if (m_play >= 0) {
        p.setPen(QPen(palette().color(QPalette::Text), 1));
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
    updateScrollBar();
}

void WaveformView::mousePressEvent(QMouseEvent* event)
{
    if (!waveArea().contains(event->position().toPoint()))
        return QWidget::mousePressEvent(event);
    const qint64 frame = frameAt(event->position().toPoint().x());
    m_pressPos = event->position().toPoint();
    m_firstAtPress = m_firstFrame;
    if (m_tool == Tool::Zoom) {
        // About the frame under the mouse.
        const double x = event->position().x();
        const double anchor = m_firstFrame + x * m_framesPerPixel;
        event->button() == Qt::RightButton ? zoomOut() : zoomIn();
        setFirstFrame(anchor - x * m_framesPerPixel);
        return;
    }
    if (event->button() != Qt::LeftButton)
        return QWidget::mousePressEvent(event);
    m_dragging = true;
    if (m_tool == Tool::Select) {
        m_anchor = (event->modifiers() & Qt::ShiftModifier) ? m_cursor : frame;
        setCursor(frame);
        setSelection(SampleRange{std::size_t(std::min(m_anchor, frame)), std::size_t(std::abs(frame - m_anchor))});
    }
}

void WaveformView::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_dragging)
        return;
    const int x = event->position().toPoint().x();
    if (m_tool == Tool::Select) {
        const qint64 frame = frameAt(x);
        setSelection(SampleRange{std::size_t(std::min(m_anchor, frame)), std::size_t(std::abs(frame - m_anchor))});
    } else if (m_tool == Tool::MoveView) {
        setFirstFrame(m_firstAtPress - (x - m_pressPos.x()) * m_framesPerPixel);
    }
}

void WaveformView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !m_dragging)
        return QWidget::mouseReleaseEvent(event);
    m_dragging = false;
    const QPoint delta = event->position().toPoint() - m_pressPos;
    if (m_selection.empty())
        return;
    switch (m_tool) {
    case Tool::MoveSelection: {
        const qint64 shift = qint64(std::lround(delta.x() * m_framesPerPixel));
        if (shift != 0) {
            const qint64 rest = qint64(m_buffer.frameCount() - m_selection.count);
            Q_EMIT moveRequested(m_selection, std::clamp<qint64>(qint64(m_selection.start) + shift, 0, rest));
        }
        break;
    }
    case Tool::HorizontalResize: {
        const qint64 frames = qint64(m_selection.count) + qint64(std::lround(delta.x() * m_framesPerPixel));
        if (frames != qint64(m_selection.count) && frames > 0)
            Q_EMIT stretchRequested(m_selection, frames);
        break;
    }
    case Tool::VerticalResize: {
        // Dragging up makes it louder. Half a band up doubles the amplitude.
        const int channels = std::max<int>(1, m_buffer.channels());
        const double half = double(waveArea().height()) / channels / 2;
        const double factor = std::max(0.0, 1.0 - delta.y() / std::max(1.0, half));
        if (delta.y() != 0)
            Q_EMIT amplitudeRequested(m_selection, factor);
        break;
    }
    default:
        break;
    }
}

void WaveformView::keyPressEvent(QKeyEvent* event)
{
    // Same as Fighter Factory: Left/Right step the cursor (Shift moves the selection end), and
    // holding Ctrl switches to the Move view offset tool until it is released.
    if (event->key() == Qt::Key_Control && !event->isAutoRepeat() && !m_toolBeforeCtrl) {
        m_toolBeforeCtrl = m_tool;
        m_tool = Tool::MoveView;
        return;
    }
    const bool left = event->key() == Qt::Key_Left;
    if ((left || event->key() == Qt::Key_Right) && !(event->modifiers() & Qt::ControlModifier)) {
        const qint64 frames = qint64(m_buffer.frameCount());
        if (event->modifiers() & Qt::ShiftModifier) {
            const qint64 end =
                std::clamp(qint64(m_selection.end()) + (left ? -1 : 1), qint64(m_selection.start), frames);
            setSelection(SampleRange{m_selection.start, std::size_t(end - qint64(m_selection.start))});
        } else {
            Q_EMIT cursorStepped(std::clamp(m_cursor + (left ? -1 : 1), qint64(0), frames));
        }
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void WaveformView::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Control && !event->isAutoRepeat() && m_toolBeforeCtrl) {
        m_tool = *m_toolBeforeCtrl;
        m_toolBeforeCtrl.reset();
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void WaveformView::wheelEvent(QWheelEvent* event)
{
    const int steps = m_wheel.take(event);
    if (steps == 0)
        return;
    setFramesPerPixel(m_framesPerPixel * std::pow(0.5, steps));
    event->accept();
}

} // namespace OFS
