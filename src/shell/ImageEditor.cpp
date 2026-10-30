#include "shell/ImageEditor.h"

#include "core/widgets/ImageCanvas.h"

#include <QCheckBox>
#include <QMouseEvent>
#include <QPainter>
#include <QSlider>
#include <QTimer>
#include <QToolBar>
#include <QUndoCommand>
#include <QUndoStack>
#include <QVBoxLayout>

#include <utility>

namespace OFS {

namespace {

// Pixels between two grid points, from the smaller to the larger, leaving out the larger.
QRect gridRect(const QPoint& a, const QPoint& b)
{
    return QRect(QPoint(std::min(a.x(), b.x()), std::min(a.y(), b.y())),
                 QPoint(std::max(a.x(), b.x()) - 1, std::max(a.y(), b.y()) - 1));
}

} // namespace

class RemapSlotsWidget : public QWidget
{
public:
    static constexpr int Cell = 12;

    explicit RemapSlotsWidget(ImageEditor* editor)
        : QWidget(editor)
        , m_editor(editor)
    {
        setFixedSize(ImageEditor::RemapSlotCount * Cell, 2 * Cell);
    }

protected:
    // Same look as Fighter Factory: every cell has its own sunken frame, so the slots stay apart. The top cell is the
    // colour to replace, the bottom one the colour that replaces it.
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        const QColor shadow(128, 128, 128);
        const QColor light(Qt::white);
        const QColor checker(160, 160, 164);
        const QColor current = palette().color(QPalette::Highlight);
        for (int i = 0; i < ImageEditor::RemapSlotCount; ++i) {
            const ImageEditor::RemapSlot& slot = m_editor->remapSlots()[std::size_t(i)];
            const bool isCurrent = i == m_editor->currentSlot();
            for (int row = 0; row < 2; ++row) {
                const QRect cell(i * Cell, row * Cell, Cell, Cell);
                p.fillRect(QRect(cell.left(), cell.top(), Cell, 1), isCurrent ? current : shadow);
                p.fillRect(QRect(cell.left(), cell.top(), 1, Cell), isCurrent ? current : shadow);
                p.fillRect(QRect(cell.left(), cell.bottom(), Cell, 1), isCurrent ? current : light);
                p.fillRect(QRect(cell.right(), cell.top(), 1, Cell), isCurrent ? current : light);
                const QRect inside = cell.adjusted(1, 1, -1, -1);
                const std::optional<PaintColor>& color = row == 0 ? slot.source : slot.replacement;
                if (color) {
                    p.fillRect(inside, m_editor->displayColor(*color));
                } else {
                    // Four squares with a thin gap between them.
                    const int x = inside.left(), y = inside.top();
                    p.fillRect(QRect(x, y, 4, 4), checker);
                    p.fillRect(QRect(x + 5, y, 5, 4), light);
                    p.fillRect(QRect(x, y + 5, 4, 5), light);
                    p.fillRect(QRect(x + 5, y + 5, 5, 5), checker);
                }
            }
        }
    }
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_editor->setCurrentSlot(int(event->position().x()) / Cell);
            update();
        }
    }

private:
    ImageEditor* m_editor;
};

// Undo and redo leave the selection alone because selecting is not an edit, same as Fighter Factory. Only a canvas size
// change drops it.
class ImageEditCommand : public QUndoCommand
{
public:
    ImageEditCommand(ImageEditor* editor, const QString& text, ImageDocument before, ImageDocument after)
        : QUndoCommand(text)
        , m_editor(editor)
        , m_before(std::move(before))
        , m_after(std::move(after))
    {}
    void undo() override { restore(m_before); }
    void redo() override
    {
        if (std::exchange(m_done, true))
            restore(m_after);
        else
            m_editor->setDocument(m_after);
    }

private:
    void restore(ImageDocument document)
    {
        const ImageDocument& now = m_editor->document();
        document.selection =
            now.width == document.width && now.height == document.height ? now.selection : std::vector<std::uint8_t>();
        // The palette is the one chosen in the panel, not part of the edit.
        if (now.indexed && document.indexed)
            document.palette = now.palette;
        m_editor->setDocument(document);
    }

    ImageEditor* m_editor;
    ImageDocument m_before;
    ImageDocument m_after;
    bool m_done = false;
};

ImageEditor::ImageEditor(QWidget* parent)
    : QWidget(parent)
    , m_undo(new QUndoStack(this))
    , m_canvas(new ImageCanvas(this))
    , m_options(new QToolBar(this))
    , m_size(new QSlider(Qt::Horizontal, this))
    , m_contiguousBox(new QCheckBox(tr("Contiguous"), this))
    , m_remap(new RemapSlotsWidget(this))
    , m_playTimer(new QTimer(this))
{
    setObjectName(QStringLiteral("imageEditor"));
    m_canvas->setObjectName(QStringLiteral("imageCanvas"));
    m_options->setObjectName(QStringLiteral("spr_tool_options"));
    m_size->setObjectName(QStringLiteral("toolSize"));
    m_size->setRange(1, MaxBrushSize);
    m_size->setFixedWidth(256);
    m_contiguousBox->setObjectName(QStringLiteral("toolContiguous"));
    m_remap->setObjectName(QStringLiteral("remapSlots"));
    m_options->addWidget(m_remap);
    m_options->addWidget(m_size);
    m_options->addWidget(m_contiguousBox);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_options);
    layout->addWidget(m_canvas, 1);

    connect(m_size, &QSlider::valueChanged, this, [this](int value) {
        if (sizeIndex(m_tool) >= 0)
            setBrushSize(m_tool, value);
    });
    connect(m_contiguousBox, &QCheckBox::toggled, this, [this](bool on) { setContiguous(m_tool, on); });
    connect(m_canvas, &ImageCanvas::pressed, this, &ImageEditor::press);
    connect(m_canvas, &ImageCanvas::dragged, this, &ImageEditor::drag);
    connect(m_canvas, &ImageCanvas::released, this, &ImageEditor::release);
    m_playTimer->setInterval(100);
    connect(m_playTimer, &QTimer::timeout, this, &ImageEditor::playStep);
    m_canvas->setDocument(&m_document);
    updateOptions();
}

ImageEditor::~ImageEditor() = default;

void ImageEditor::play()
{
    if (m_document.layers.isEmpty())
        return;
    m_playDocument = m_document;
    m_playLayer = int(m_document.layers.size());
    m_playTimer->start();
    playStep();
}

void ImageEditor::playStep()
{
    if (--m_playLayer < 0) {
        m_playTimer->stop();
        m_canvas->setDocument(&m_document);
        return;
    }
    for (int i = 0; i < int(m_playDocument.layers.size()); ++i)
        m_playDocument.layers[i].visible = i == m_playLayer;
    m_canvas->setDocument(&m_playDocument);
}

bool ImageEditor::isPlaying() const
{
    return m_playTimer->isActive();
}

void ImageEditor::open(const Formats::Image& image, const QString& name)
{
    m_document = ImageDocument::fromImage(image, name);
    m_undo->clear();
    m_modified = false;
    m_canvas->setDocument(&m_document);
    Q_EMIT documentChanged();
}

void ImageEditor::apply(const QString& text, const ImageDocument& after)
{
    m_modified = true;
    m_undo->push(new ImageEditCommand(this, text, m_document, after));
}

void ImageEditor::change(const ImageDocument& after)
{
    setDocument(after);
}

void ImageEditor::setDocument(const ImageDocument& document)
{
    m_document = document;
    refresh();
}

void ImageEditor::setPalette(const Formats::Palette& colors)
{
    if (m_document.palette == colors)
        return;
    m_document.palette = colors;
    m_before.palette = colors; // a stroke going on
    refresh();
    m_remap->update();
    Q_EMIT colorsChanged();
}

void ImageEditor::refresh()
{
    m_playTimer->stop();
    m_canvas->setDocument(&m_document);
    Q_EMIT documentChanged();
}

int ImageEditor::sizeIndex(Tool tool) const
{
    switch (tool) {
    case Tool::FreeSelect:
        return 0;
    case Tool::Pen:
        return 1;
    case Tool::Eraser:
        return 2;
    case Tool::RemapPen:
        return 3;
    default:
        return -1;
    }
}

int ImageEditor::brushSize(Tool tool) const
{
    const int i = sizeIndex(tool);
    return i >= 0 ? m_sizes[std::size_t(i)] : 1;
}

void ImageEditor::setBrushSize(Tool tool, int size)
{
    const int i = sizeIndex(tool);
    if (i < 0)
        return;
    m_sizes[std::size_t(i)] = std::clamp(size, 1, MaxBrushSize);
    if (tool == m_tool)
        updateOptions();
}

bool ImageEditor::contiguous(Tool tool) const
{
    return tool == Tool::PaintBucket ? m_contiguous[1] : m_contiguous[0];
}

void ImageEditor::setContiguous(Tool tool, bool on)
{
    if (tool == Tool::MagicWand)
        m_contiguous[0] = on;
    else if (tool == Tool::PaintBucket)
        m_contiguous[1] = on;
}

void ImageEditor::setTool(Tool tool)
{
    m_tool = tool;
    updateOptions();
    Q_EMIT toolChanged();
}

void ImageEditor::updateOptions()
{
    const bool sized = sizeIndex(m_tool) >= 0;
    const bool contiguousTool = m_tool == Tool::MagicWand || m_tool == Tool::PaintBucket;
    for (QAction* a : m_options->actions()) {
        QWidget* w = m_options->widgetForAction(a);
        if (w == m_size)
            a->setVisible(sized);
        else if (w == m_contiguousBox)
            a->setVisible(contiguousTool);
        else if (w == m_remap)
            a->setVisible(m_tool == Tool::RemapPen);
    }
    if (sized) {
        const QSignalBlocker blocker(m_size);
        m_size->setValue(brushSize(m_tool));
    }
    if (contiguousTool) {
        const QSignalBlocker blocker(m_contiguousBox);
        m_contiguousBox->setChecked(contiguous(m_tool));
    }
    m_canvas->setPanning(m_tool == Tool::CanvasMove);
    m_canvas->setBrushSize(sized ? brushSize(m_tool) : 0);
    // The brush tools show their size on the pixels, the others a small cross.
    ImageCanvas::Pointer pointer = sized ? ImageCanvas::Pointer::Square : ImageCanvas::Pointer::Cross;
    if (m_tool == Tool::CanvasMove)
        pointer = ImageCanvas::Pointer::Hand;
    else if (m_tool == Tool::SelectionMove)
        pointer = ImageCanvas::Pointer::Move;
    m_canvas->setPointer(pointer);
}

void ImageEditor::setForeground(const PaintColor& color)
{
    m_foreground = color;
    Q_EMIT colorsChanged();
}

void ImageEditor::setBackground(const PaintColor& color)
{
    m_background = color;
    Q_EMIT colorsChanged();
}

QColor ImageEditor::displayColor(const PaintColor& color) const
{
    // An 8-bit canvas shows an index through the current palette, so a palette change shows at once.
    Formats::Rgba rgba = color.rgba;
    if (m_document.indexed && color.index >= 0 && color.index < int(m_document.palette.size()))
        rgba = color.index == 0 ? Formats::Rgba{0, 0, 0, 0} : m_document.palette[std::size_t(color.index)];
    if (rgba.a == 0)
        return QColor(Qt::gray);
    return QColor(rgba.r, rgba.g, rgba.b);
}

QList<QPair<PaintColor, PaintColor>> ImageEditor::remapPairs(bool reverse) const
{
    // Colours as the current palette shows them, for canvases that compare colours instead of indexes.
    auto shown = [this](PaintColor color) {
        if (color.index >= 0 && color.index < int(m_document.palette.size())) {
            color.rgba = m_document.palette[std::size_t(color.index)];
            color.rgba.a = color.index == 0 ? 0 : 255;
        }
        return color;
    };
    // A slot without the colour to write turns its pixels transparent, same as Fighter Factory.
    const PaintColor transparent{0, Formats::Rgba{0, 0, 0, 0}};
    QList<QPair<PaintColor, PaintColor>> pairs;
    for (const RemapSlot& slot : m_slots) {
        const std::optional<PaintColor>& from = reverse ? slot.replacement : slot.source;
        const std::optional<PaintColor>& to = reverse ? slot.source : slot.replacement;
        if (from)
            pairs << qMakePair(shown(*from), to ? shown(*to) : transparent);
    }
    return pairs;
}

PaintColor ImageEditor::sample(const QPoint& pixel) const
{
    PaintColor color = ImageEdits::pick(m_document, pixel);
    if (m_document.indexed) {
        color.rgba = Formats::Rgba{0, 0, 0, 0};
        if (color.index > 0 && color.index < int(m_document.palette.size())) {
            color.rgba = m_document.palette[std::size_t(color.index)];
            color.rgba.a = 255;
        }
    }
    return color;
}

void ImageEditor::setCurrentSlot(int slot)
{
    m_currentSlot = std::clamp(slot, 0, RemapSlotCount - 1);
    m_remap->update();
}

void ImageEditor::setSlotColor(bool replacement, const PaintColor& color)
{
    RemapSlot& slot = m_slots[std::size_t(m_currentSlot)];
    (replacement ? slot.replacement : slot.source) = color;
    m_remap->update();
}

void ImageEditor::press(const QPoint& pixel, const QPoint& grid, Qt::MouseButton button,
                        Qt::KeyboardModifiers modifiers)
{
    m_before = m_document;
    m_button = button;
    m_start = (m_tool == Tool::RectSelect) ? grid : pixel;
    m_last = pixel;
    m_stroking = false;
    const bool left = button == Qt::LeftButton;
    switch (m_tool) {
    case Tool::CanvasMove:
    case Tool::SelectionMove:
    case Tool::RectSelect:
        break;
    case Tool::FreeSelect: {
        ImageDocument doc = m_document;
        ImageEdits::paintSelection(doc, pixel, pixel, brushSize(m_tool), left);
        change(doc);
        break;
    }
    case Tool::MagicWand: {
        ImageEdits::SelectMode mode = ImageEdits::SelectMode::Replace;
        if (!left)
            mode = ImageEdits::SelectMode::AllExcept;
        else if (modifiers & Qt::ShiftModifier)
            mode = ImageEdits::SelectMode::Add;
        else if (modifiers & Qt::AltModifier)
            mode = ImageEdits::SelectMode::Subtract;
        ImageDocument doc = m_document;
        ImageEdits::selectColor(doc, pixel, contiguous(m_tool), mode);
        change(doc);
        break;
    }
    case Tool::Picker:
        if (m_document.contains(pixel.x(), pixel.y()) && button != Qt::MiddleButton) {
            const PaintColor color = sample(pixel);
            left ? setForeground(color) : setBackground(color);
        }
        break;
    case Tool::Pen:
        m_stroking = ImageEdits::paint(m_document, pixel, pixel, brushSize(m_tool), left ? m_foreground : m_background);
        refresh();
        break;
    case Tool::Eraser:
        if (left) {
            m_stroking = ImageEdits::paint(m_document, pixel, pixel, brushSize(m_tool), m_background);
            refresh();
        }
        break;
    case Tool::RemapPen:
        if (modifiers & Qt::ShiftModifier) {
            if (m_document.contains(pixel.x(), pixel.y()))
                setSlotColor(!left, sample(pixel));
            m_button = Qt::NoButton;
        } else {
            m_stroking = ImageEdits::remap(m_document, pixel, pixel, brushSize(m_tool), remapPairs(!left));
            refresh();
        }
        break;
    case Tool::PaintBucket: {
        // Any button fills with the foreground colour, same as Fighter Factory.
        ImageDocument doc = m_document;
        if (ImageEdits::fill(doc, pixel, m_foreground, contiguous(m_tool)))
            apply(tr("Paint bucket"), doc);
        break;
    }
    }
}

void ImageEditor::drag(const QPoint& pixel, const QPoint& grid)
{
    const bool left = m_button == Qt::LeftButton;
    switch (m_tool) {
    case Tool::SelectionMove: {
        if (m_button == Qt::NoButton)
            break;
        ImageDocument doc = m_before;
        ImageEdits::moveSelection(doc, pixel.x() - m_start.x(), pixel.y() - m_start.y(), m_background);
        m_document = doc;
        refresh();
        break;
    }
    case Tool::RectSelect: // the selection changes in place (copying the layers each move is slow)
        ImageEdits::selectRect(m_document, gridRect(m_start, grid));
        refresh();
        break;
    case Tool::FreeSelect:
        ImageEdits::paintSelection(m_document, m_last, pixel, brushSize(m_tool), left);
        refresh();
        break;
    case Tool::Pen:
        if (m_button == Qt::NoButton)
            break;
        m_stroking |=
            ImageEdits::paint(m_document, m_last, pixel, brushSize(m_tool), left ? m_foreground : m_background);
        refresh();
        break;
    case Tool::Eraser:
        if (left) {
            m_stroking |= ImageEdits::paint(m_document, m_last, pixel, brushSize(m_tool), m_background);
            refresh();
        }
        break;
    case Tool::RemapPen:
        if (m_button != Qt::NoButton) {
            m_stroking |= ImageEdits::remap(m_document, m_last, pixel, brushSize(m_tool), remapPairs(!left));
            refresh();
        }
        break;
    default:
        break;
    }
    m_last = pixel;
}

void ImageEditor::release(const QPoint& pixel, const QPoint& grid)
{
    switch (m_tool) {
    case Tool::SelectionMove: {
        // Any button moves the selection, same as Fighter Factory.
        if (m_button == Qt::NoButton)
            break;
        ImageDocument after = m_before;
        if (ImageEdits::moveSelection(after, pixel.x() - m_start.x(), pixel.y() - m_start.y(), m_background)) {
            m_document = m_before;
            apply(tr("Move selection"), after);
        } else {
            setDocument(m_before);
        }
        break;
    }
    case Tool::RectSelect: {
        ImageDocument doc = m_document;
        ImageEdits::selectRect(doc, gridRect(m_start, grid));
        change(doc);
        break;
    }
    case Tool::Pen:
    case Tool::Eraser:
    case Tool::RemapPen:
        if (m_stroking) {
            const ImageDocument after = m_document;
            m_document = m_before;
            apply(m_tool == Tool::Pen ? tr("Pen") : m_tool == Tool::Eraser ? tr("Eraser") : tr("Remap pen"), after);
        }
        break;
    default:
        break;
    }
    m_stroking = false;
    m_button = Qt::NoButton;
}

} // namespace OFS
