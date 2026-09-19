#include "core/widgets/ImageCanvas.h"
#include "core/sprites/QtImages.h"
#include "core/widgets/Checkerboard.h"

#include "core/widgets/RenderSurface.h"

#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <cmath>

namespace OFS {

ImageCanvas::ImageCanvas(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_surface = Rendering::attachSurface(this, [this](QPainter& p) { paintContent(p); });
}

void ImageCanvas::setDocument(const ImageDocument* document)
{
    const bool resized =
        !m_document || !document || document->width != m_document->width || document->height != m_document->height;
    m_document = document;
    if (resized && document)
        m_center = QPointF(document->width / 2.0, document->height / 2.0);
    refresh();
}

void ImageCanvas::refresh()
{
    m_composite = QImage();
    m_dimmed = QImage();
    if (m_document && m_document->width > 0 && m_document->height > 0) {
        const Formats::Image flat = ImageEdits::flatten(*m_document);
        Formats::Image rgba = flat;
        if (rgba.isIndexed()) {
            rgba.setPalette(m_document->palette);
            rgba = rgba.toRgba(true);
        }
        m_composite = toQImage(rgba);
        // Unselected pixels are drawn darker (about 61 %).
        if (m_document->hasSelection()) {
            m_dimmed = m_composite.convertToFormat(QImage::Format_ARGB32);
            for (int y = 0; y < m_dimmed.height(); ++y) {
                auto* line = reinterpret_cast<QRgb*>(m_dimmed.scanLine(y));
                for (int x = 0; x < m_dimmed.width(); ++x) {
                    if (m_document->selection[std::size_t(y) * std::size_t(m_document->width) + std::size_t(x)])
                        continue;
                    const QRgb c = line[x];
                    line[x] = qRgba(qRed(c) * 61 / 100, qGreen(c) * 61 / 100, qBlue(c) * 61 / 100, qAlpha(c));
                }
            }
        }
        m_zoom = std::clamp(m_zoom, std::min(minimumZoom(), MaximumZoom), MaximumZoom);
    }
    update();
}

double ImageCanvas::minimumZoom() const
{
    if (!m_document || m_document->width <= 0 || m_document->height <= 0)
        return 1.0;
    return std::min(MaximumZoom, 50.0 / std::min(m_document->width, m_document->height));
}

void ImageCanvas::setZoom(double zoom)
{
    zoom = std::clamp(zoom, minimumZoom(), MaximumZoom);
    if (std::abs(zoom - m_zoom) < 1e-9)
        return;
    m_zoom = zoom;
    update();
    Q_EMIT zoomChanged(m_zoom);
}

void ImageCanvas::zoomIn()
{
    setZoom(std::floor(m_zoom + 1e-9) + 1);
}

void ImageCanvas::zoomOut()
{
    setZoom(std::ceil(m_zoom - 1e-9) - 1);
}

void ImageCanvas::zoomReset()
{
    setZoom(1.0);
}

void ImageCanvas::zoomFit()
{
    if (!m_document || m_document->width <= 0)
        return;
    const QRectF area = editArea();
    m_center = QPointF(m_document->width / 2.0, m_document->height / 2.0);
    const double fit = std::min(area.width() / m_document->width, area.height() / m_document->height);
    m_zoom = -1;
    setZoom(std::min(MaximumZoom, fit));
}

void ImageCanvas::setBrushSize(int size)
{
    m_brushSize = size;
    update();
}

void ImageCanvas::setOriginalShown(bool shown)
{
    m_original = shown;
    update();
}

QRectF ImageCanvas::editArea() const
{
    const QRectF all = rect();
    return m_original ? QRectF(all.center().x(), all.top(), all.width() / 2.0, all.height()) : all;
}

QRectF ImageCanvas::canvasArea() const
{
    if (!m_document)
        return {};
    const QRectF area = editArea();
    const QPointF topLeft = area.center() - m_center * m_zoom;
    return QRectF(topLeft, QSizeF(m_document->width, m_document->height) * m_zoom);
}

QPointF ImageCanvas::toWidget(const QPointF& imagePoint) const
{
    return canvasArea().topLeft() + imagePoint * m_zoom;
}

QPoint ImageCanvas::pixelAt(const QPointF& point) const
{
    const QPointF image = (point - canvasArea().topLeft()) / m_zoom;
    return QPoint(int(std::floor(image.x())), int(std::floor(image.y())));
}

QPoint ImageCanvas::gridPointAt(const QPointF& point) const
{
    const QPointF image = (point - canvasArea().topLeft()) / m_zoom;
    return QPoint(int(std::lround(image.x())), int(std::lround(image.y())));
}

void ImageCanvas::drawCanvas(QPainter& p, const QRectF& target) const
{
    if (m_composite.isNull())
        return;
    const double cell = 8 * target.width() / m_document->width;
    p.save();
    p.setClipRect(target);
    QBrush board = checkerboard(8, QColor(192, 192, 192), Qt::white);
    board.setTransform(QTransform::fromScale(cell / 8, cell / 8));
    p.setBrushOrigin(target.topLeft());
    p.fillRect(target, board);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.drawImage(target, m_dimmed.isNull() ? m_composite : m_dimmed);
    p.restore();
    p.setPen(Qt::black);
    p.drawRect(target.adjusted(-1, -1, 0, 0));
}

void ImageCanvas::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    if (m_surface) {
        Rendering::update(m_surface);
        return;
    }
    QPainter p(this);
    paintContent(p);
}

void ImageCanvas::paintContent(QPainter& p)
{
    QLinearGradient back(0, 0, 0, height());
    back.setColorAt(0, m_workspaceTop);
    back.setColorAt(1, m_workspaceBottom);
    p.fillRect(rect(), back);
    if (!m_document)
        return;
    if (m_original) {
        const QRectF left(0, 0, width() / 2.0, height());
        const QSizeF size(m_document->width, m_document->height);
        drawCanvas(p, QRectF(left.center() - QPointF(size.width() / 2, size.height() / 2), size));
    }
    p.save();
    p.setClipRect(editArea());
    drawCanvas(p, canvasArea());
    if (m_brushSize > 0 && m_document->contains(m_hover.x(), m_hover.y())) {
        p.setPen(QPen(Qt::white, 1, Qt::DotLine));
        p.drawRect(QRectF(toWidget(m_hover), QSizeF(m_brushSize, m_brushSize) * m_zoom));
    }
    p.restore();
}

void ImageCanvas::mousePressEvent(QMouseEvent* event)
{
    if (!m_document)
        return;
    m_pressed = true;
    m_dragPan = m_panning || (event->modifiers() & Qt::ControlModifier);
    if (m_dragPan) {
        m_panStart = event->position();
        m_centerAtStart = m_center;
        return;
    }
    Q_EMIT pressed(pixelAt(event->position()), gridPointAt(event->position()), event->button(), event->modifiers());
}

void ImageCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_document)
        return;
    const QPoint hover = pixelAt(event->position());
    if (hover != m_hover && m_brushSize > 0) {
        m_hover = hover;
        update();
    }
    m_hover = hover;
    if (!m_pressed)
        return;
    if (m_dragPan) {
        m_center = m_centerAtStart - (event->position() - m_panStart) / m_zoom;
        update();
        return;
    }
    Q_EMIT dragged(hover, gridPointAt(event->position()));
}

void ImageCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (!m_pressed)
        return;
    m_pressed = false;
    if (m_dragPan) {
        m_dragPan = false;
        return;
    }
    Q_EMIT released(pixelAt(event->position()), gridPointAt(event->position()));
}

void ImageCanvas::wheelEvent(QWheelEvent* event)
{
    const int steps = m_wheel.take(event);
    if (steps == 0)
        return;
    setZoom(m_zoom * std::pow(2.0, steps));
    event->accept();
}

void ImageCanvas::leaveEvent(QEvent* event)
{
    m_hover = QPoint(-1, -1);
    update();
    QWidget::leaveEvent(event);
}

} // namespace OFS
