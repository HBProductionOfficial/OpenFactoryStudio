#pragma once

#include "core/CoreExport.h"
#include "formats/image/Color.h"

#include <QList>
#include <QWidget>

namespace OFS {

// A palette as a grid of 16 color cells per row. Without colors it shows 256 black cells.
class OFS_CORE_EXPORT PaletteWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PaletteWidget(QWidget* parent = nullptr);

    void setColors(const Formats::Palette& colors);
    const Formats::Palette& colors() const { return m_colors; }
    int slotCount() const;

    const QList<int>& selection() const { return m_selection; }
    void setSelection(QList<int> slots);
    void clearSelection() { setSelection({}); }

    int slotAt(const QPoint& pos) const;
    QRect cellRect(int slot) const;

    // Options > Graphics > Colors fit available space: as many columns as fit, at least 16.
    void setFitWidth(bool fit);
    int columns() const;

    void setDragEnabled(bool enabled) { m_dragEnabled = enabled; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

Q_SIGNALS:
    void selectionChanged();
    void slotPressed(int slot, Qt::MouseButton button);
    void slotDoubleClicked(int slot);
    void slotMoved(int from, int to);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    QPoint gridOrigin() const;

    bool m_fitWidth = false;

    Formats::Palette m_colors;
    QList<int> m_selection;
    int m_anchor = -1;
    int m_pressed = -1;
    QPoint m_pressPos;
    bool m_dragging = false;
    int m_drop = -1;
    bool m_dragEnabled = true;
};

} // namespace OFS
