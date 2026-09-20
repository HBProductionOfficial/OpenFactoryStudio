#pragma once

#include "core/sprites/ImageDocument.h"

#include <QWidget>

#include <array>
#include <optional>

class QCheckBox;
class QSlider;
class QTimer;
class QToolBar;
class QUndoStack;

namespace OFS {

class ImageCanvas;
class RemapSlotsWidget;

// Image editor for one sprite with its own undo history. It stays modified from the first edit until saved, even after
// undo.
class ImageEditor : public QWidget
{
    Q_OBJECT

public:
    enum class Tool
    {
        CanvasMove,
        SelectionMove,
        RectSelect,
        FreeSelect,
        MagicWand,
        Picker,
        Pen,
        Eraser,
        RemapPen,
        PaintBucket,
    };

    explicit ImageEditor(QWidget* parent = nullptr);
    ~ImageEditor() override;

    void open(const Formats::Image& image, const QString& name);
    const ImageDocument& document() const { return m_document; }
    bool isModified() const { return m_modified; }
    void markSaved() { m_modified = false; }
    ImageCanvas* canvas() const { return m_canvas; }
    QUndoStack* undoStack() const { return m_undo; }

    void apply(const QString& text, const ImageDocument& after);
    void change(const ImageDocument& after);

    Tool tool() const { return m_tool; }
    void setTool(Tool tool);
    PaintColor foreground() const { return m_foreground; }
    PaintColor background() const { return m_background; }
    void setForeground(const PaintColor& color);
    void setBackground(const PaintColor& color);
    QColor displayColor(const PaintColor& color) const;
    PaintColor sample(const QPoint& pixel) const;
    int brushSize(Tool tool) const;
    void setBrushSize(Tool tool, int size);
    bool contiguous(Tool tool) const;
    void setContiguous(Tool tool, bool contiguous);
    static constexpr int MaxBrushSize = 128;
    static constexpr int RemapSlotCount = 32;
    struct RemapSlot
    {
        std::optional<PaintColor> source;
        std::optional<PaintColor> replacement;
    };
    const std::array<RemapSlot, RemapSlotCount>& remapSlots() const { return m_slots; }
    int currentSlot() const { return m_currentSlot; }
    void setCurrentSlot(int slot);
    void setSlotColor(bool replacement, const PaintColor& color);

    // Layers > Play: each layer alone, top first, about 0.1 s each.
    void play();
    bool isPlaying() const;

Q_SIGNALS:
    void documentChanged();
    void colorsChanged();
    void toolChanged();

private:
    void press(const QPoint& pixel, const QPoint& grid, Qt::MouseButton button, Qt::KeyboardModifiers modifiers);
    void drag(const QPoint& pixel, const QPoint& grid);
    void release(const QPoint& pixel, const QPoint& grid);
    void updateOptions();
    void refresh();
    void playStep();
    int sizeIndex(Tool tool) const;

    friend class ImageEditCommand;
    void setDocument(const ImageDocument& document);

    ImageDocument m_document;
    bool m_modified = false;
    QUndoStack* m_undo;
    ImageCanvas* m_canvas;
    QToolBar* m_options;
    QSlider* m_size;
    QCheckBox* m_contiguousBox;
    RemapSlotsWidget* m_remap;
    Tool m_tool = Tool::CanvasMove;
    PaintColor m_foreground{0, {0, 0, 0, 255}};
    PaintColor m_background{0, {0, 0, 0, 255}};
    std::array<int, 4> m_sizes{1, 1, 1, 1};       // free selection, pen, eraser, remap pen
    std::array<bool, 2> m_contiguous{true, true}; // wand, bucket
    std::array<RemapSlot, RemapSlotCount> m_slots;
    int m_currentSlot = 0;
    QTimer* m_playTimer;
    ImageDocument m_playDocument;
    int m_playLayer = -1;

    ImageDocument m_before;
    Qt::MouseButton m_button = Qt::NoButton;
    QPoint m_start;
    QPoint m_last;
    bool m_stroking = false;
};

} // namespace OFS
