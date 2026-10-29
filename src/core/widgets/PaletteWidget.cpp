#include "core/widgets/PaletteWidget.h"

#include <QApplication>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

#include <algorithm>
#include <vector>

namespace OFS {

namespace {

constexpr int kColumns = 16;
constexpr int kPitch = 12; // logical px per cell, the white grid line included

} // namespace

PaletteWidget::PaletteWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(minimumSizeHint());
}

void PaletteWidget::setColors(const Formats::Palette& colors)
{
    m_colors = colors;
    const int count = slotCount();
    QList<int> kept = m_selection;
    kept.removeIf([count](int slot) { return slot >= count; });
    if (kept != m_selection) {
        m_selection = kept;
        Q_EMIT selectionChanged();
    }
    update();
}

int PaletteWidget::slotCount() const
{
    return m_colors.empty() ? 256 : int(std::min<std::size_t>(m_colors.size(), 256));
}

void PaletteWidget::setSelection(QList<int> slots)
{
    const int count = slotCount();
    slots.removeIf([count](int slot) { return slot < 0 || slot >= count; });
    std::sort(slots.begin(), slots.end());
    slots.erase(std::unique(slots.begin(), slots.end()), slots.end());
    if (slots == m_selection)
        return;
    m_selection = slots;
    update();
    Q_EMIT selectionChanged();
}

void PaletteWidget::setFitWidth(bool fit)
{
    if (m_fitWidth == fit)
        return;
    m_fitWidth = fit;
    update();
}

int PaletteWidget::columns() const
{
    return m_fitWidth ? std::max(kColumns, (width() - 1) / kPitch) : kColumns;
}

QPoint PaletteWidget::gridOrigin() const
{
    if (m_fitWidth)
        return QPoint(0, 0);
    return QPoint(std::max(0, (width() - kColumns * kPitch - 1) / 2), 0);
}

QRect PaletteWidget::cellRect(int slot) const
{
    const QPoint origin = gridOrigin();
    const int n = columns();
    return QRect(origin.x() + (slot % n) * kPitch + 1, origin.y() + (slot / n) * kPitch + 1, kPitch - 1, kPitch - 1);
}

int PaletteWidget::slotAt(const QPoint& pos) const
{
    const QPoint p = pos - gridOrigin();
    if (p.x() < 0 || p.y() < 0)
        return -1;
    const int n = columns();
    const int column = p.x() / kPitch;
    const int slot = (p.y() / kPitch) * n + column;
    return column < n && slot < slotCount() ? slot : -1;
}

int PaletteWidget::toolTipSlotAt(const QPoint& pos) const
{
    const QPoint p = pos - gridOrigin();
    if (p.y() < 0)
        return -1;
    const int n = columns();
    const int column = std::clamp(p.x() < 0 ? 0 : p.x() / kPitch, 0, n - 1);
    const int slot = (p.y() / kPitch) * n + column;
    return slot < slotCount() ? slot : -1;
}

QString PaletteWidget::colorText(int slot) const
{
    if (slot < 0 || slot >= slotCount())
        return {};
    const Formats::Rgba c = m_colors.empty() ? Formats::Rgba{} : m_colors[std::size_t(slot)];
    return QStringLiteral("%1, %2, %3, %4").arg(c.r).arg(c.g).arg(c.b).arg(c.a);
}

bool PaletteWidget::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip) {
        const auto* help = static_cast<QHelpEvent*>(event);
        const int slot = toolTipSlotAt(help->pos());
        if (slot < 0) {
            QToolTip::hideText();
            event->ignore();
            return true;
        }
        // The tip stays while the pointer is on the same cell, side margins included.
        const QPoint origin = gridOrigin();
        const int n = columns();
        const int column = slot % n;
        const int left = column == 0 ? 0 : origin.x() + column * kPitch;
        const int right = column == n - 1 ? width() : origin.x() + (column + 1) * kPitch;
        const QRect area(left, origin.y() + (slot / n) * kPitch, right - left, kPitch);
        QToolTip::showText(help->globalPos(), colorText(slot), this, area);
        return true;
    }
    return QWidget::event(event);
}

QSize PaletteWidget::sizeHint() const
{
    return QSize(kColumns * kPitch + 1 + 48, 16 * kPitch + 1);
}

QSize PaletteWidget::minimumSizeHint() const
{
    return QSize(kColumns * kPitch + 1, 16 * kPitch + 1);
}

void PaletteWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    const int count = slotCount();
    const int n = columns();
    const int rows = (count + n - 1) / n;
    const QPoint origin = gridOrigin();
    const int fullRows = count / n;
    painter.fillRect(QRect(origin, QSize(n * kPitch + 1, fullRows * kPitch + 1)), Qt::white);
    if (rows > fullRows)
        painter.fillRect(QRect(origin.x(), origin.y() + fullRows * kPitch, (count % n) * kPitch + 1, kPitch + 1),
                         Qt::white);
    const QColor gray(Qt::gray);
    std::vector<bool> selected(std::size_t(std::max(count, 0)), false);
    for (int slot : m_selection) {
        if (slot >= 0 && slot < count)
            selected[std::size_t(slot)] = true;
    }
    for (int slot = 0; slot < count; ++slot) {
        const QRect cell = cellRect(slot);
        QColor color(Qt::black);
        if (!m_colors.empty()) {
            const Formats::Rgba c = m_colors[std::size_t(slot)];
            color = QColor(c.r, c.g, c.b, c.a);
            const int half = cell.width() / 2;
            painter.fillRect(cell, Qt::white);
            painter.fillRect(QRect(cell.topLeft(), QSize(half, half)), gray);
            painter.fillRect(QRect(cell.left() + half, cell.top() + half, cell.width() - half, cell.height() - half),
                             gray);
        }
        painter.fillRect(cell, color);
        if (selected[std::size_t(slot)]) {
            // Black or white so it shows on any color. Fighter Factory's faded outline vanished on white.
            const bool light = color.alpha() < 128 || qGray(color.rgb()) >= 128;
            painter.setPen(QPen(light ? Qt::black : Qt::white, 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(cell.adjusted(2, 2, -3, -3));
        }
    }
    if (m_dragging && m_drop >= 0 && m_drop != m_pressed) {
        painter.setPen(QPen(palette().highlight().color(), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(cellRect(m_drop).adjusted(-1, -1, 0, 0));
    }
}

void PaletteWidget::mousePressEvent(QMouseEvent* event)
{
    const int slot = slotAt(event->position().toPoint());
    if (slot < 0) {
        QWidget::mousePressEvent(event);
        return;
    }
    if (event->button() == Qt::LeftButton) {
        QList<int> slots = m_selection;
        if (event->modifiers() & Qt::ControlModifier) {
            if (slots.contains(slot))
                slots.removeAll(slot);
            else
                slots << slot;
        } else if ((event->modifiers() & Qt::ShiftModifier) && m_anchor >= 0) {
            slots.clear();
            for (int i = std::min(m_anchor, slot); i <= std::max(m_anchor, slot); ++i)
                slots << i;
        } else {
            slots = {slot};
        }
        m_anchor = slot;
        m_pressed = slot;
        m_pressPos = event->position().toPoint();
        m_dragging = false;
        setSelection(slots);
    }
    Q_EMIT slotPressed(slot, event->button());
}

void PaletteWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!(event->buttons() & Qt::LeftButton) || m_pressed < 0 || !m_dragEnabled)
        return;
    const QPoint pos = event->position().toPoint();
    if (!m_dragging && (pos - m_pressPos).manhattanLength() < QApplication::startDragDistance())
        return;
    m_dragging = true;
    m_drop = slotAt(pos);
    update();
}

void PaletteWidget::mouseReleaseEvent(QMouseEvent* event)
{
    const int from = m_pressed;
    const int to = m_dragging ? slotAt(event->position().toPoint()) : -1;
    const bool dragged = m_dragging;
    m_pressed = -1;
    m_dragging = false;
    m_drop = -1;
    if (dragged) {
        update();
        if (to >= 0 && to != from) {
            clearSelection();
            Q_EMIT slotMoved(from, to);
        }
    }
}

void PaletteWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    const int slot = slotAt(event->position().toPoint());
    if (slot < 0 || event->button() != Qt::LeftButton)
        return;
    m_anchor = slot;
    setSelection({slot});
    Q_EMIT slotDoubleClicked(slot);
}

} // namespace OFS
