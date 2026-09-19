#include "shell/AdvancedPaletteEditor.h"

#include "core/app/AppContext.h"
#include "core/palettes/PaletteOps.h"
#include "core/project/Project.h"
#include "core/settings/Config.h"
#include "core/sprites/PaletteEdits.h"
#include "core/sprites/SpriteEdits.h"
#include "core/sprites/SpriteExport.h"
#include "core/sprites/SpriteFile.h"
#include "core/theme/ThemeManager.h"
#include "core/widgets/Checkerboard.h"
#include "core/widgets/PaletteWidget.h"
#include "formats/codecs/Png.h"
#include "formats/image/ImageFile.h"
#include "formats/image/Quantize.h"
#include "formats/palette/PaletteFormats.h"
#include "shell/FileDialogs.h"
#include "shell/FileFilters.h"
#include "shell/dialogs/PaletteDialogs.h"
#include "shell/dialogs/SoundDialogs.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QRadioButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QToolButton>
#include <QUndoCommand>
#include <QUndoStack>
#include <QVBoxLayout>

#include <cmath>
#include <functional>

namespace OFS {

using namespace Formats;

namespace {

// Shows a column's image zoomed over a checkerboard and reports clicked pixels.
class ImageView : public QWidget
{
public:
    using QWidget::QWidget;

    void setImage(const QImage& image, double zoom)
    {
        m_image = image;
        m_zoom = zoom;
        const QSize size = image.isNull()
                               ? QSize(0, 0)
                               : QSize(int(std::ceil(image.width() * zoom)), int(std::ceil(image.height() * zoom)));
        setFixedSize(size.expandedTo(QSize(1, 1)));
        update();
    }
    std::function<void(QPoint)> clicked;

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), checkerboard(8, Qt::white, QColor(160, 160, 164)));
        if (!m_image.isNull())
            painter.drawImage(QRectF(0, 0, m_image.width() * m_zoom, m_image.height() * m_zoom), m_image);
    }
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && clicked && !m_image.isNull())
            clicked(QPoint(int(event->position().x() / m_zoom), int(event->position().y() / m_zoom)));
    }

private:
    QImage m_image;
    double m_zoom = 1.0;
};

QString caption(const SpritePalettePtr& palette)
{
    return QStringLiteral("%1,%2").arg(palette->palette.group).arg(palette->palette.number);
}

std::optional<Rgba> askColor(QWidget* parent, const QString& title, Rgba initial, bool alpha)
{
    QColorDialog dialog(QColor(initial.r, initial.g, initial.b, initial.a), parent);
    dialog.setWindowTitle(title);
    QColorDialog::ColorDialogOptions options = QColorDialog::DontUseNativeDialog;
    if (alpha)
        options |= QColorDialog::ShowAlphaChannel;
    dialog.setOptions(options);
    if (dialog.exec() != QDialog::Accepted)
        return std::nullopt;
    const QColor c = dialog.selectedColor();
    return Rgba{std::uint8_t(c.red()), std::uint8_t(c.green()), std::uint8_t(c.blue()),
                std::uint8_t(alpha ? c.alpha() : 255)};
}

Result<Image> readImage(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return Error{file.errorString().toStdString()};
    const QByteArray bytes = file.readAll();
    return decodeImageFile(ByteSpan(reinterpret_cast<const std::byte*>(bytes.constData()), std::size_t(bytes.size())));
}

Image showing(const Image& image, const Palette& palette, bool index0Transparent)
{
    if (!image.isIndexed())
        return image;
    Image copy = image;
    copy.setPalette(palette);
    return copy.toRgba(index0Transparent);
}

// Colors go in octree order (R, G and B bits interleaved), or median cut to 256 when there are more.
Image indexedWithOwnColors(const Image& rgba)
{
    std::vector<Rgba> colors = PaletteOps::usedColors(rgba);
    if (colors.size() > 256)
        return quantize(rgba);
    auto key = [](Rgba c) {
        quint64 k = 0;
        for (int bit = 7; bit >= 0; --bit)
            k = (k << 3) | (quint64((c.r >> bit) & 1) << 2) | (quint64((c.g >> bit) & 1) << 1) | ((c.b >> bit) & 1);
        return (k << 1) | (c.a ? 1 : 0);
    };
    std::sort(colors.begin(), colors.end(), [&key](Rgba a, Rgba b) { return key(a) < key(b); });
    Palette palette(colors.begin(), colors.end());
    palette.resize(256, Rgba{0, 0, 0, 0});
    Image indexed = PaletteOps::adapt(rgba, palette);
    std::vector<std::uint8_t> pixels(std::size_t(rgba.width()) * std::size_t(rgba.height()));
    const auto source = rgba.pixels();
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        Rgba c{source[i * 4], source[i * 4 + 1], source[i * 4 + 2], source[i * 4 + 3]};
        c.a = c.a < 128 ? 0 : 255;
        pixels[i] = std::uint8_t(std::find(colors.begin(), colors.end(), c) - colors.begin());
    }
    return Image::indexed(rgba.width(), rgba.height(), std::move(pixels), palette);
}

} // namespace

class ColumnCommand : public QUndoCommand
{
public:
    ColumnCommand(AdvancedPaletteEditor* editor, int which, const QString& text, AdvancedPaletteEditor::Column before,
                  AdvancedPaletteEditor::Column after)
        : QUndoCommand(text)
        , m_editor(editor)
        , m_which(which)
        , m_before(std::move(before))
        , m_after(std::move(after))
    {}

    void undo() override { m_editor->setColumn(m_which, m_before); }
    void redo() override { m_editor->setColumn(m_which, m_after); }

private:
    AdvancedPaletteEditor* m_editor;
    int m_which;
    AdvancedPaletteEditor::Column m_before;
    AdvancedPaletteEditor::Column m_after;
};

AdvancedPaletteEditor::AdvancedPaletteEditor(AppContext& context, SpriteFile* file, int sprite, int palette,
                                             QWidget* parent)
    : QDialog(parent)
    , m_context(context)
    , m_file(file)
{
    setObjectName(QStringLiteral("PaletteEditor"));
    setWindowTitle(tr("Advanced palette editor"));
    setModal(true);
    for (std::size_t i = 0; i < 2; ++i)
        m_undo[i] = new QUndoStack(this);

    auto* functions = new QGridLayout;
    functions->setSpacing(0);
    struct Function
    {
        const char* name;
        const char* icon;
        const char* text;
    };
    static const Function list[] = {
        {"openPal", "advpal_open", QT_TR_NOOP("Open palette")},
        {"savePal", "advpal_save", QT_TR_NOOP("Save current palette")},
        {"savGrpIdx", "advpal_save_new", QT_TR_NOOP("Add to sprites editor")},
        {"zoomOut", "advpal_zoom_out", QT_TR_NOOP("Decrease zoom")},
        {"zoomIn", "advpal_zoom_in", QT_TR_NOOP("Increase zoom")},
        {"openImg", "advpal_open_img", QT_TR_NOOP("Open image")},
        {"saveImg", "advpal_save_img", QT_TR_NOOP("Save image")},
        {"undo", "advpal_undo", QT_TR_NOOP("Undo")},
        {"redo", "advpal_redo", QT_TR_NOOP("Redo")},
        {"genPal", "advpal_gen_pal_imgs", QT_TR_NOOP("Generate palette from images")},
        {"insPal", "advpal_insert_pal_imgs", QT_TR_NOOP("Insert the current palette in the images")},
        {"optPal", "advpal_optimize", QT_TR_NOOP("Optimized palette for the current image")},
        {"grayScale", "advpal_grayscale", QT_TR_NOOP("Generate a minimal grayscale palette")},
        {"corGrad", "advpal_gradient", QT_TR_NOOP("Interval of selected colors in gradient")},
        {"setCor", "advpal_set_color", QT_TR_NOOP("Sets the color of the selected slots")},
        {"delDupl", "advpal_del_duplicate", QT_TR_NOOP("Remove the duplicate colors")},
        {"addDiff", "advpal_add_diff", QT_TR_NOOP("Add different colors from the other palette")},
        {"adaptImg", "advpal_adapt_image", QT_TR_NOOP("Adapt image to the palette")},
        {"autoOrg", "advpal_auto_sort", QT_TR_NOOP("Sort colors by tones")},
        {"swPos", "advpal_switch_pos", QT_TR_NOOP("Exchange position of selected colors")},
        {"invPal", "advpal_invert_all", QT_TR_NOOP("Invert all colors")},
        {"commChanges", "advpal_commit_changes", QT_TR_NOOP("Commit changes")},
        {"commSprChanges", "advpal_commit_spr_changes", QT_TR_NOOP("Commit sprite changes")},
    };
    for (int i = 0; i < int(std::size(list)); ++i) {
        const QString name = QString::fromLatin1(list[i].name);
        QToolButton* button = toolButton(name, QString::fromLatin1(list[i].icon), tr(list[i].text));
        connect(button, &QToolButton::clicked, this, [this, name] { run(name); });
        functions->addWidget(button, i / 7, i % 7);
    }

    m_effects = new QComboBox(this);
    m_effects->setObjectName(QStringLiteral("effects"));
    m_effects->addItems(PaletteOps::effectNames());
    QToolButton* effectApply =
        toolButton(QStringLiteral("effApply"), QStringLiteral("advpal_effect_apply"), tr("Apply"));
    connect(effectApply, &QToolButton::clicked, this, [this] { run(QStringLiteral("effApply")); });
    auto* effectRow = new QHBoxLayout;
    effectRow->addWidget(m_effects, 1);
    effectRow->addWidget(effectApply);

    auto* selectionGroup = new QGroupBox(tr("Color selection:"), this);
    selectionGroup->setObjectName(QStringLiteral("color_sel"));
    m_mask = new QCheckBox(tr("Mask"), selectionGroup);
    m_mask->setObjectName(QStringLiteral("mask"));
    auto* maskColor = new QToolButton(selectionGroup);
    maskColor->setObjectName(QStringLiteral("maskCor"));
    maskColor->setText(tr("Mask's color"));
    maskColor->setToolTip(tr("Mask's color"));
    m_picker = toolButton(QStringLiteral("pickCor"), QStringLiteral("advpal_pick_color"), tr("Color picker"));
    m_picker->setParent(selectionGroup);
    m_picker->setCheckable(true);
    connect(m_mask, &QCheckBox::toggled, this, [this] {
        refreshColumn(Character);
        refreshColumn(ImageOrPalette);
    });
    connect(maskColor, &QToolButton::clicked, this, [this] {
        const QColor color =
            QColorDialog::getColor(m_maskColor, this, tr("Mask's color"), QColorDialog::DontUseNativeDialog);
        if (color.isValid()) {
            m_maskColor = color;
            refreshColumn(Character);
            refreshColumn(ImageOrPalette);
        }
    });
    auto* selectionLayout = new QHBoxLayout(selectionGroup);
    selectionLayout->addWidget(m_mask);
    selectionLayout->addWidget(maskColor);
    selectionLayout->addStretch(1);
    selectionLayout->addWidget(m_picker);

    // The sliders only preview on the selected slots until Apply. Reset drops the preview.
    auto slider = [this](QGroupBox* group, const char* name, int range, QLabel** label, const char* labelName) {
        auto* s = new QSlider(Qt::Horizontal, group);
        s->setObjectName(QLatin1String(name));
        s->setRange(-range, range);
        *label = new QLabel(QStringLiteral("0"), group);
        (*label)->setObjectName(QLatin1String(labelName));
        (*label)->setMinimumWidth(32);
        connect(s, &QSlider::valueChanged, this, [this, label = *label](int value) {
            label->setText(QString::number(value));
            if (!m_resettingSliders)
                preview();
        });
        return s;
    };
    auto* hueGroup = new QGroupBox(tr("Hue / saturation:"), this);
    hueGroup->setObjectName(QStringLiteral("hue_grp"));
    m_hue = slider(hueGroup, "hueHue", 360, &m_hueLabel, "hueLb");
    m_saturation = slider(hueGroup, "hueSat", 100, &m_saturationLabel, "satLb");
    QToolButton* hueReset = toolButton(QStringLiteral("hueNo"), QStringLiteral("advpal_huesat_reset"), tr("Reset"));
    QToolButton* hueApply = toolButton(QStringLiteral("hueYes"), QStringLiteral("advpal_huesat_apply"), tr("Apply"));
    auto* hueLayout = new QGridLayout(hueGroup);
    hueLayout->addWidget(hueReset, 0, 3);
    hueLayout->addWidget(hueApply, 0, 4);
    hueLayout->addWidget(new QLabel(QStringLiteral("H"), hueGroup), 1, 0);
    hueLayout->addWidget(m_hue, 1, 1, 1, 3);
    hueLayout->addWidget(m_hueLabel, 1, 4);
    hueLayout->addWidget(new QLabel(QStringLiteral("S"), hueGroup), 2, 0);
    hueLayout->addWidget(m_saturation, 2, 1, 1, 3);
    hueLayout->addWidget(m_saturationLabel, 2, 4);
    auto* rgbGroup = new QGroupBox(tr("RGB:"), this);
    rgbGroup->setObjectName(QStringLiteral("rgb_grp"));
    m_red = slider(rgbGroup, "rgbR", 255, &m_redLabel, "rLb");
    m_green = slider(rgbGroup, "rgbG", 255, &m_greenLabel, "gLb");
    m_blue = slider(rgbGroup, "rgbB", 255, &m_blueLabel, "bLb");
    QToolButton* rgbReset = toolButton(QStringLiteral("rgbNo"), QStringLiteral("advpal_rgb_reset"), tr("Reset"));
    QToolButton* rgbApply = toolButton(QStringLiteral("rgbYes"), QStringLiteral("advpal_rgb_apply"), tr("Apply"));
    auto* rgbLayout = new QGridLayout(rgbGroup);
    rgbLayout->addWidget(rgbReset, 0, 3);
    rgbLayout->addWidget(rgbApply, 0, 4);
    const std::array<std::pair<QSlider*, QLabel*>, 3> rgb{
        {{m_red, m_redLabel}, {m_green, m_greenLabel}, {m_blue, m_blueLabel}}};
    for (int i = 0; i < 3; ++i) {
        rgbLayout->addWidget(new QLabel(QStringLiteral("RGB").mid(i, 1), rgbGroup), i + 1, 0);
        rgbLayout->addWidget(rgb[std::size_t(i)].first, i + 1, 1, 1, 3);
        rgbLayout->addWidget(rgb[std::size_t(i)].second, i + 1, 4);
    }
    for (QToolButton* reset : {hueReset, rgbReset})
        connect(reset, &QToolButton::clicked, this, &AdvancedPaletteEditor::dropPreview);
    for (QToolButton* apply : {hueApply, rgbApply})
        connect(apply, &QToolButton::clicked, this, &AdvancedPaletteEditor::commitPreview);

    auto* left = new QVBoxLayout;
    left->addLayout(functions);
    left->addSpacing(8);
    left->addLayout(effectRow);
    left->addWidget(selectionGroup);
    left->addWidget(hueGroup);
    left->addWidget(rgbGroup);
    left->addStretch(1);

    m_characterOption = new QRadioButton(tr("Character"), this);
    m_characterOption->setObjectName(QStringLiteral("charOpt"));
    m_characterOption->setChecked(true);
    m_imageOption = new QRadioButton(tr("Image or palette"), this);
    m_imageOption->setObjectName(QStringLiteral("imgpalOpt"));
    connect(m_characterOption, &QRadioButton::toggled, this, [this] { dropPreview(); });
    m_spriteSelector = new QScrollBar(Qt::Horizontal, this);
    m_spriteSelector->setObjectName(QStringLiteral("imgSel"));
    m_spriteLabel = new QLabel(this);
    m_spriteLabel->setObjectName(QStringLiteral("imgSelLb"));
    m_paletteSelector = new QScrollBar(Qt::Horizontal, this);
    m_paletteSelector->setObjectName(QStringLiteral("palSel"));
    m_paletteLabel = new QLabel(this);
    m_paletteLabel->setObjectName(QStringLiteral("palSelLb"));
    static const char* const areaNames[] = {"charArea", "imgpalArea"};
    static const char* const gridNames[] = {"charPal", "imgpalPal"};
    for (int i = 0; i < 2; ++i) {
        auto* area = new QScrollArea(this);
        area->setObjectName(QLatin1String(areaNames[i]));
        area->setAlignment(Qt::AlignCenter);
        auto* view = new ImageView(area);
        view->clicked = [this, i](QPoint pixel) {
            const Column& c = m_columns[std::size_t(i)];
            if (!m_picker->isChecked() || !c.image.isIndexed() || pixel.x() < 0 || pixel.y() < 0 ||
                pixel.x() >= c.image.width() || pixel.y() >= c.image.height())
                return;
            const int index =
                c.image.pixels()[std::size_t(pixel.y()) * std::size_t(c.image.width()) + std::size_t(pixel.x())];
            m_grids[std::size_t(i)]->setSelection(m_grids[std::size_t(i)]->selection() + QList<int>{index});
        };
        area->setWidget(view);
        m_areas[std::size_t(i)] = area;
        m_views[std::size_t(i)] = view;
        auto* grid = new PaletteWidget(this);
        grid->setObjectName(QLatin1String(gridNames[i]));
        grid->setFitWidth(m_context.config().boolValue(QStringLiteral("pal_fit_space"), false));
        connect(grid, &PaletteWidget::selectionChanged, this, [this, i] {
            if (m_mask->isChecked())
                refreshColumn(i);
        });
        connect(grid, &PaletteWidget::slotDoubleClicked, this, [this, i](int slot) {
            dropPreview();
            Column after = m_columns[std::size_t(i)];
            if (slot >= int(after.palette.size()))
                return;
            const std::optional<Rgba> color = askColor(this, tr("Color"), after.palette[std::size_t(slot)], true);
            if (!color)
                return;
            after.palette[std::size_t(slot)] = *color;
            change(i, tr("Palette color"), after);
        });
        connect(grid, &PaletteWidget::slotMoved, this, [this, i](int from, int to) {
            dropPreview();
            Column after = m_columns[std::size_t(i)];
            if (from >= int(after.palette.size()) || to >= int(after.palette.size()))
                return;
            const Rgba color = after.palette[std::size_t(from)];
            after.palette.erase(after.palette.begin() + from);
            after.palette.insert(after.palette.begin() + to, color);
            change(i, tr("Move color"), after);
        });
        m_grids[std::size_t(i)] = grid;
    }
    auto transfer = [this](const char* name, const QString& icon, const QString& text) {
        QToolButton* button = toolButton(QString::fromLatin1(name), icon, text);
        button->setMinimumSize(51, 32);
        return button;
    };
    QToolButton* colorsLeft =
        transfer("corRL", QStringLiteral("advpal_colto_left"), tr("Send selected colors to the left palette"));
    QToolButton* colorsRight =
        transfer("corLR", QStringLiteral("advpal_colto_right"), tr("Send selected colors to the right palette"));
    QToolButton* selectionLeft =
        transfer("selRL", QStringLiteral("advpal_selto_left"), tr("Send selection to the left palette"));
    QToolButton* selectionRight =
        transfer("selLR", QStringLiteral("advpal_selto_right"), tr("Send selection to the right palette"));
    connect(colorsLeft, &QToolButton::clicked, this, [this] { transferColors(ImageOrPalette, Character); });
    connect(colorsRight, &QToolButton::clicked, this, [this] { transferColors(Character, ImageOrPalette); });
    connect(selectionLeft, &QToolButton::clicked, this,
            [this] { m_grids[Character]->setSelection(m_grids[ImageOrPalette]->selection()); });
    connect(selectionRight, &QToolButton::clicked, this,
            [this] { m_grids[ImageOrPalette]->setSelection(m_grids[Character]->selection()); });

    auto* columns = new QGridLayout;
    columns->addWidget(m_characterOption, 0, 0);
    columns->addWidget(m_imageOption, 0, 2);
    auto* spriteRow = new QHBoxLayout;
    spriteRow->addWidget(m_spriteSelector, 1);
    spriteRow->addWidget(m_spriteLabel);
    columns->addLayout(spriteRow, 1, 0);
    columns->addWidget(m_areas[0], 2, 0);
    columns->addWidget(m_areas[1], 1, 2, 2, 1);
    auto* paletteRow = new QHBoxLayout;
    paletteRow->addWidget(m_paletteSelector, 1);
    paletteRow->addWidget(m_paletteLabel);
    columns->addLayout(paletteRow, 3, 0);
    auto* transfers = new QVBoxLayout;
    transfers->addStretch(1);
    for (QToolButton* b : {colorsLeft, colorsRight, selectionLeft, selectionRight})
        transfers->addWidget(b);
    transfers->addStretch(1);
    columns->addWidget(m_grids[0], 4, 0);
    columns->addLayout(transfers, 4, 1);
    columns->addWidget(m_grids[1], 4, 2);
    columns->setRowStretch(2, 1);
    columns->setColumnStretch(0, 1);
    columns->setColumnStretch(2, 1);

    auto* layout = new QHBoxLayout(this);
    layout->addLayout(left);
    layout->addLayout(columns, 1);

    const SpriteDocument* doc = m_file ? &m_file->document() : nullptr;
    const int sprites = doc ? doc->spriteCount() : 0;
    const int palettes = doc ? int(doc->palettes().size()) : 0;
    m_spriteSelector->setRange(0, std::max(0, sprites - 1));
    m_spriteSelector->setEnabled(sprites > 0);
    m_paletteSelector->setRange(0, std::max(0, palettes - 1));
    m_paletteSelector->setEnabled(palettes > 0);
    if (doc && sprite >= 0 && sprite < sprites) {
        const int own = PaletteEdits::paletteOf(*doc, sprite);
        if (own >= 0)
            palette = own;
    }
    loadPalette(palette >= 0 && palette < palettes ? palette : (palettes > 0 ? 0 : -1));
    loadSprite(sprite >= 0 && sprite < sprites ? sprite : (sprites > 0 ? 0 : -1));
    m_spriteSelector->setValue(std::max(0, m_sprite));
    m_paletteSelector->setValue(std::max(0, m_palette));
    connect(m_spriteSelector, &QScrollBar::valueChanged, this, [this](int value) {
        if (value == m_sprite)
            return;
        if (!m_imageChanged || confirmDiscard())
            loadSprite(value);
        else
            m_spriteSelector->setValue(m_sprite);
    });
    connect(m_paletteSelector, &QScrollBar::valueChanged, this, [this](int value) {
        if (value == m_palette)
            return;
        if (!m_paletteChanged || confirmDiscard())
            loadPalette(value);
        else
            m_paletteSelector->setValue(m_palette);
    });
    refreshColumn(ImageOrPalette);

    const QRect geometry =
        Config::decodeRect(m_context.config().value(QStringLiteral("advpal_geometry")), QRect(442, 222, 865, 623));
    resize(geometry.size());
    move(geometry.topLeft());
}

AdvancedPaletteEditor::~AdvancedPaletteEditor()
{
    m_context.config().setValue(QStringLiteral("advpal_geometry"), Config::encodeRect(QRect(pos(), size())));
}

QToolButton* AdvancedPaletteEditor::toolButton(const QString& name, const QString& icon, const QString& text)
{
    auto* button = new QToolButton(this);
    button->setObjectName(name);
    button->setToolTip(text);
    button->setText(text);
    button->setIcon(m_context.theme().iconSet().icon(icon));
    button->setIconSize(QSize(24, 24));
    button->setMinimumSize(34, 34);
    button->setAutoRaise(true);
    return button;
}

int AdvancedPaletteEditor::checkedColumn() const
{
    return m_imageOption->isChecked() ? ImageOrPalette : Character;
}

void AdvancedPaletteEditor::setCheckedColumn(int which)
{
    (which == ImageOrPalette ? m_imageOption : m_characterOption)->setChecked(true);
}

QString AdvancedPaletteEditor::folder() const
{
    auto* project = m_file ? qobject_cast<Project*>(m_file->parent()) : nullptr;
    return project ? project->directory() : QString();
}

void AdvancedPaletteEditor::change(int which, const QString& text, const Column& after)
{
    m_undo[std::size_t(which)]->push(new ColumnCommand(this, which, text, m_columns[std::size_t(which)], after));
}

void AdvancedPaletteEditor::setColumn(int which, const Column& state)
{
    Column& column = m_columns[std::size_t(which)];
    if (which == Character) {
        m_paletteChanged = m_paletteChanged || state.palette != column.palette;
        const auto a = state.image.pixels();
        const auto b = column.image.pixels();
        m_imageChanged = m_imageChanged || state.image.width() != column.image.width() ||
                         state.image.height() != column.image.height() ||
                         !std::equal(a.begin(), a.end(), b.begin(), b.end());
    }
    column = state;
    refreshColumn(which);
}

QImage AdvancedPaletteEditor::render(int which) const
{
    const Column& column = m_columns[std::size_t(which)];
    if (column.image.isNull())
        return {};
    Image rgba = showing(column.image, column.palette, which == Character);
    if (m_mask->isChecked() && column.image.isIndexed()) {
        std::vector<bool> masked(256, false);
        for (int slot : m_grids[std::size_t(which)]->selection())
            masked[std::size_t(slot)] = true;
        const auto indexes = column.image.pixels();
        auto pixels = rgba.pixels();
        for (std::size_t i = 0; i < indexes.size(); ++i) {
            if (!masked[indexes[i]])
                continue;
            pixels[i * 4] = std::uint8_t(m_maskColor.red());
            pixels[i * 4 + 1] = std::uint8_t(m_maskColor.green());
            pixels[i * 4 + 2] = std::uint8_t(m_maskColor.blue());
            pixels[i * 4 + 3] = 255;
        }
    }
    return QImage(rgba.pixels().data(), rgba.width(), rgba.height(), rgba.width() * 4, QImage::Format_RGBA8888).copy();
}

void AdvancedPaletteEditor::refreshColumn(int which)
{
    const Column& column = m_columns[std::size_t(which)];
    m_grids[std::size_t(which)]->setColors(column.palette);
    static_cast<ImageView*>(m_views[std::size_t(which)])->setImage(render(which), m_zoom[std::size_t(which)]);
    if (which == Character) {
        const SpriteDocument* doc = m_file ? &m_file->document() : nullptr;
        m_spriteLabel->setText(
            doc && m_sprite >= 0 && m_sprite < doc->spriteCount()
                ? QStringLiteral("%1,%2").arg(doc->sprites()[m_sprite].group()).arg(doc->sprites()[m_sprite].number())
                : QString());
        m_paletteLabel->setText(doc && m_palette >= 0 && m_palette < doc->palettes().size()
                                    ? caption(doc->palettes()[m_palette])
                                    : QString());
    }
}

void AdvancedPaletteEditor::loadSprite(int sprite)
{
    m_sprite = sprite;
    Column& column = m_columns[Character];
    const SpriteDocument* doc = m_file ? &m_file->document() : nullptr;
    column.image = doc && sprite >= 0 && sprite < doc->spriteCount() ? doc->image(sprite) : Image();
    column.reference = showing(column.image, column.palette, true);
    m_imageChanged = false;
    m_undo[Character]->clear();
    refreshColumn(Character);
}

void AdvancedPaletteEditor::loadPalette(int palette)
{
    m_palette = palette;
    Column& column = m_columns[Character];
    const SpriteDocument* doc = m_file ? &m_file->document() : nullptr;
    column.palette =
        doc && palette >= 0 && palette < doc->palettes().size() ? doc->palettes()[palette]->palette.colors : Palette();
    column.reference = showing(column.image, column.palette, true);
    m_paletteChanged = false;
    m_undo[Character]->clear();
    refreshColumn(Character);
}

bool AdvancedPaletteEditor::confirmDiscard()
{
    QMessageBox box(QMessageBox::Question, tr("Discard changes?"),
                    tr("Do you want to discard the changes not committed to the sprites?"),
                    QMessageBox::Yes | QMessageBox::No, this);
    box.setDefaultButton(QMessageBox::No);
    return box.exec() == QMessageBox::Yes;
}

void AdvancedPaletteEditor::reject()
{
    dropPreview();
    if (hasChanges() && !confirmDiscard())
        return;
    QDialog::reject();
}

void AdvancedPaletteEditor::preview()
{
    const int which = checkedColumn();
    if (m_previewBase && m_previewColumn != which) {
        // Undo a preview left on the other column. The sliders keep their values.
        m_columns[std::size_t(m_previewColumn)].palette = *m_previewBase;
        refreshColumn(m_previewColumn);
        m_previewBase.reset();
    }
    if (!m_previewBase) {
        m_previewBase = m_columns[std::size_t(which)].palette;
        m_previewColumn = which;
    }
    Palette shown = *m_previewBase;
    const QList<int> slots = m_grids[std::size_t(which)]->selection();
    PaletteOps::adjustHueSaturation(shown, slots, m_hue->value(), m_saturation->value());
    PaletteOps::adjustRgb(shown, slots, m_red->value(), m_green->value(), m_blue->value());
    m_columns[std::size_t(which)].palette = shown;
    refreshColumn(which);
}

void AdvancedPaletteEditor::dropPreview()
{
    m_resettingSliders = true;
    for (QSlider* s : {m_hue, m_saturation, m_red, m_green, m_blue})
        s->setValue(0);
    m_resettingSliders = false;
    if (!m_previewBase)
        return;
    m_columns[std::size_t(m_previewColumn)].palette = *m_previewBase;
    refreshColumn(m_previewColumn);
    m_previewBase.reset();
}

void AdvancedPaletteEditor::commitPreview()
{
    if (!m_previewBase)
        return;
    const int which = m_previewColumn;
    Column after = m_columns[std::size_t(which)];
    m_columns[std::size_t(which)].palette = *m_previewBase;
    m_previewBase.reset();
    m_resettingSliders = true;
    for (QSlider* s : {m_hue, m_saturation, m_red, m_green, m_blue})
        s->setValue(0);
    m_resettingSliders = false;
    change(which, tr("Adjust colors"), after);
}

void AdvancedPaletteEditor::transferColors(int from, int to)
{
    dropPreview();
    const QList<int> sources = m_grids[std::size_t(from)]->selection();
    const QList<int> targets = m_grids[std::size_t(to)]->selection();
    Column after = m_columns[std::size_t(to)];
    const Palette& source = m_columns[std::size_t(from)].palette;
    bool changed = false;
    for (qsizetype i = 0; i < std::min(sources.size(), targets.size()); ++i) {
        if (sources[i] < int(source.size()) && targets[i] < int(after.palette.size())) {
            after.palette[std::size_t(targets[i])] = source[std::size_t(sources[i])];
            changed = true;
        }
    }
    if (changed)
        change(to, tr("Send colors"), after);
}

void AdvancedPaletteEditor::run(const QString& function)
{
    dropPreview();
    const int which = checkedColumn();
    Column after = m_columns[std::size_t(which)];
    const QList<int> slots = m_grids[std::size_t(which)]->selection();
    auto keepSelection = [this, which, slots] {
        m_grids[std::size_t(which)]->setSelection(slots);
    };
    // The image as it looks now becomes the reference for later adapting.
    auto updateReference = [which](Column& c) {
        c.reference = showing(c.image, c.palette, which == Character);
    };
    if (function == QLatin1String("undo")) {
        m_undo[std::size_t(which)]->undo();
    } else if (function == QLatin1String("redo")) {
        m_undo[std::size_t(which)]->redo();
    } else if (function == QLatin1String("zoomIn") || function == QLatin1String("zoomOut")) {
        double& zoom = m_zoom[std::size_t(which)];
        zoom = std::clamp(function == QLatin1String("zoomIn") ? zoom * 2 : zoom / 2, 0.25, 8.0);
        refreshColumn(which);
    } else if (function == QLatin1String("openPal")) {
        const QString path = FileDialogs::getOpenFileName(this, tr("Open palette"), folder(), paletteFileFilters());
        if (path.isEmpty())
            return;
        auto colors = PaletteEdits::readFile(path);
        if (!colors) {
            QMessageBox::warning(this, tr("Open palette"), QString::fromStdString(colors.error().message));
            return;
        }
        after.palette = std::move(colors.value());
        change(which, tr("Open palette"), after);
    } else if (function == QLatin1String("savePal")) {
        const QString path =
            FileDialogs::getSaveFileName(this, tr("Save palette"), folder(), FileFilters::adobeColorTable());
        if (path.isEmpty())
            return;
        QString error;
        SpriteDocument scratch;
        scratch.palettes() << SpriteDocument::makePalette(0, 0, after.palette);
        if (PaletteEdits::saveAct(scratch, {0}, path, &error).isEmpty())
            QMessageBox::warning(this, tr("Save palette"), error);
    } else if (function == QLatin1String("savGrpIdx")) {
        if (!m_file)
            return;
        AddItemsDialog dialog(AddItemsDialog::Kind::EditorPalettes, this);
        if (dialog.exec() != QDialog::Accepted)
            return;
        Palette colors = after.palette;
        if (dialog.reversed())
            std::reverse(colors.begin(), colors.end());
        SpriteDocument doc = m_file->document();
        PaletteEdits::insert(doc, int(doc.palettes().size()) - 1, dialog.group(), dialog.number(), std::move(colors));
        m_file->apply(tr("Add palette"), doc);
        m_paletteSelector->setRange(0, int(doc.palettes().size()) - 1);
        m_paletteSelector->setEnabled(true);
    } else if (function == QLatin1String("openImg")) {
        openImage();
    } else if (function == QLatin1String("saveImg")) {
        if (after.image.isNull())
            return;
        const QString path = FileDialogs::getSaveFileName(this, tr("Save image"), folder(), FileFilters::saveImages());
        if (path.isEmpty())
            return;
        Image image = after.image;
        if (image.isIndexed())
            image.setPalette(after.palette);
        QString error;
        if (QFileInfo(path).suffix().compare(QLatin1String("png"), Qt::CaseInsensitive) == 0 && image.isIndexed()) {
            // 8-bit PNG with the whole palette and its alpha in tRNS.
            PngEncodeOptions options;
            options.paletteAlpha = true;
            auto bytes = encodePng(image, options);
            QSaveFile out(path);
            if (!bytes || !out.open(QIODevice::WriteOnly) ||
                out.write(reinterpret_cast<const char*>(bytes.value().data()), qint64(bytes.value().size())) !=
                    qint64(bytes.value().size()) ||
                !out.commit())
                error = bytes ? out.errorString() : QString::fromStdString(bytes.error().message);
        } else {
            writeImageFile(path, image, &error);
        }
        if (!error.isEmpty())
            QMessageBox::warning(this, tr("Save image"), error);
    } else if (function == QLatin1String("genPal")) {
        const QStringList files =
            FileDialogs::getOpenFileNames(this, tr("Open image"), folder(), FileFilters::openImages());
        if (files.isEmpty())
            return;
        QList<Image> images;
        for (const QString& path : files) {
            auto image = readImage(path);
            if (image)
                images << image.value();
        }
        if (images.isEmpty())
            return;
        after.palette = PaletteOps::paletteFromImages(images);
        change(which, tr("Generate palette from images"), after);
    } else if (function == QLatin1String("insPal")) {
        const QStringList files =
            FileDialogs::getOpenFileNames(this, tr("Open image"), folder(), FileFilters::openImages());
        if (files.isEmpty())
            return;
        if (QMessageBox::question(this, tr("Insert the current palette in the images"),
                                  tr("The %n chosen image(s) will be rewritten with the current palette. Continue?",
                                     nullptr, int(files.size())),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
        QStringList errors;
        for (const QString& path : files) {
            auto image = readImage(path);
            if (!image) {
                errors << QStringLiteral("%1: %2").arg(QFileInfo(path).fileName(),
                                                       QString::fromStdString(image.error().message));
                continue;
            }
            QString error;
            if (!writeImageFile(path, PaletteOps::adapt(image.value(), after.palette), &error))
                errors << error;
        }
        if (!errors.isEmpty())
            QMessageBox::warning(this, tr("Insert the current palette in the images"), errors.join(QLatin1Char('\n')));
    } else if (function == QLatin1String("optPal")) {
        if (after.image.isNull())
            return;
        if (after.image.isIndexed())
            after.image.setPalette(after.palette);
        PaletteOps::optimize(after.image, after.palette);
        updateReference(after);
        change(which, tr("Optimized palette"), after);
    } else if (function == QLatin1String("grayScale")) {
        if (after.image.isNull())
            return;
        if (after.image.isIndexed())
            after.image.setPalette(after.palette);
        PaletteOps::grayscale(after.image, after.palette);
        updateReference(after);
        change(which, tr("Grayscale palette"), after);
    } else if (function == QLatin1String("corGrad")) {
        if (slots.isEmpty())
            return;
        const std::optional<Rgba> start =
            askColor(this, tr("Start color"), after.palette[std::size_t(slots.first())], false);
        if (!start)
            return;
        const std::optional<Rgba> end = askColor(this, tr("End color"), Rgba{255, 255, 255, 255}, false);
        if (!end)
            return;
        PaletteOps::gradient(after.palette, slots, *start, *end);
        change(which, tr("Gradient"), after);
        keepSelection();
    } else if (function == QLatin1String("setCor")) {
        if (slots.isEmpty())
            return;
        const std::optional<Rgba> color = askColor(this, tr("Color"), after.palette[std::size_t(slots.first())], true);
        if (!color)
            return;
        for (int slot : slots)
            after.palette[std::size_t(slot)] = *color;
        change(which, tr("Palette color"), after);
        keepSelection();
    } else if (function == QLatin1String("delDupl")) {
        const std::optional<Rgba> base = askColor(this, tr("Select Color"), Rgba{255, 0, 255, 255}, false);
        if (!base)
            return;
        const PaletteOps::SlotMap map = PaletteOps::removeDuplicates(after.palette, *base);
        PaletteOps::remapImage(after.image, map);
        change(which, tr("Remove the duplicate colors"), after);
    } else if (function == QLatin1String("addDiff")) {
        if (PaletteOps::addDifferent(after.palette, m_columns[std::size_t(1 - which)].palette) > 0)
            change(which, tr("Add different colors"), after);
    } else if (function == QLatin1String("adaptImg")) {
        if (after.reference.isNull())
            return;
        after.image = PaletteOps::adapt(after.reference, after.palette);
        updateReference(after);
        change(which, tr("Adapt image to the palette"), after);
    } else if (function == QLatin1String("autoOrg")) {
        const PaletteOps::SlotMap map = PaletteOps::sortByTone(after.palette);
        PaletteOps::remapImage(after.image, map);
        change(which, tr("Sort colors by tones"), after);
    } else if (function == QLatin1String("swPos")) {
        if (slots.size() < 2)
            return;
        PaletteOps::reverseSlots(after.palette, slots);
        change(which, tr("Exchange colors"), after);
        keepSelection();
    } else if (function == QLatin1String("invPal")) {
        PaletteOps::invert(after.palette);
        change(which, tr("Invert all colors"), after);
    } else if (function == QLatin1String("effApply")) {
        if (slots.isEmpty())
            return;
        PaletteOps::applyEffect(after.palette, slots, PaletteOps::Effect(std::clamp(m_effects->currentIndex(), 0, 11)));
        change(which, m_effects->currentText(), after);
        keepSelection();
    } else if (function == QLatin1String("commChanges")) {
        commitPalette();
    } else if (function == QLatin1String("commSprChanges")) {
        commitSprite();
    }
}

void AdvancedPaletteEditor::openImage()
{
    const int which = checkedColumn();
    const QString path = FileDialogs::getOpenFileName(this, tr("Open image"), folder(), FileFilters::openImages());
    if (path.isEmpty())
        return;
    auto image = readImage(path);
    if (!image) {
        QMessageBox::warning(this, tr("Open image"), QString::fromStdString(image.error().message));
        return;
    }
    Column after = m_columns[std::size_t(which)];
    if (image.value().isIndexed()) {
        after.image = image.value();
        after.palette = after.image.palette();
        after.palette.resize(std::max<std::size_t>(after.palette.size(), 256), Rgba{0, 0, 0, 0});
    } else {
        QMessageBox box(QMessageBox::Question, tr("Generate optimized palette?"),
                        tr("This image isn't indexed. Do you want to generate an optimized palette for it?"),
                        QMessageBox::Yes | QMessageBox::No, this);
        box.setDefaultButton(QMessageBox::No);
        if (box.exec() == QMessageBox::Yes) {
            after.image = indexedWithOwnColors(image.value());
            after.palette = after.image.palette();
        } else {
            after.image = image.value();
        }
    }
    after.reference = showing(after.image, after.palette, false);
    change(which, tr("Open image"), after);
}

void AdvancedPaletteEditor::commitPalette()
{
    if (!m_file || m_palette < 0 || m_palette >= m_file->document().palettes().size())
        return;
    SpriteDocument doc = m_file->document();
    PaletteEdits::setColors(doc, m_palette, m_columns[Character].palette);
    m_file->apply(tr("Commit changes"), doc);
    m_paletteChanged = false;
}

void AdvancedPaletteEditor::commitSprite()
{
    if (!m_file || m_sprite < 0 || m_sprite >= m_file->document().spriteCount())
        return;
    const Column& column = m_columns[Character];
    if (column.image.isNull())
        return;
    CommitSpriteDialog dialog(CommitSpriteDialog::captions(m_file->document()), this);
    const int result = dialog.exec();
    if (result != QDialog::Accepted && result != CommitSpriteDialog::Ignored)
        return;
    SpriteDocument doc = m_file->document();
    dialog.commit(doc, m_sprite, column.image, column.palette, result);
    m_file->apply(tr("Commit sprite changes"), doc);
    m_imageChanged = false;
    m_paletteSelector->setRange(0, std::max(0, int(doc.palettes().size()) - 1));
}

} // namespace OFS
