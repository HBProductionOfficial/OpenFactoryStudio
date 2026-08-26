// Image editor mode of the Sprites editor: its panel controls, layers list, colour swatches and actions.

#include "shell/SpritesEditor.h"

#include "core/actions/ActionRegistry.h"
#include "core/app/AppContext.h"
#include "core/settings/Config.h"
#include "core/sprites/ImageDocument.h"
#include "core/sprites/SpriteEdits.h"
#include "core/sprites/SpriteExport.h"
#include "core/sprites/SpriteFile.h"
#include "core/widgets/ImageCanvas.h"
#include "shell/FileDialogs.h"
#include "shell/ImageEditor.h"
#include "shell/SpritePage.h"
#include "shell/dialogs/SpriteDialogs.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QRegularExpression>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QToolButton>

#include <cstring>

namespace OFS {

namespace {

// Controls only align mode shows, and controls only image editor mode shows.
const char* const AlignControls[] = {
    "spr_name_lb",  "spr_name",       "spr_prev_grp",  "spr_grp_lb",   "spr_grp",        "spr_next_grp",
    "spr_num_lb",   "spr_num",        "spr_xaxis_lb",  "spr_xaxis",    "spr_yaxis_lb",   "spr_yaxis",
    "spr_pos_left", "spr_pos_up",     "spr_pos_right", "spr_pos_down", "spr_dec_load",   "spr_trans_color",
    "spr_info_lb",  "spr_onion_skin", "spr_onion_sel", "spr_onion_lb", "spr_delete_grp", "spr_rename_grp",
    "spr_openimg",  "spr_saveimgas",
};
const char* const ImageControls[] = {
    "spr_saveimg",          "spr_layer_save",     "spr_layer_play",       "spr_explorer",        "spr_layers_menu",
    "spr_layers_blendmode", "spr_layers_opacity", "spr_layers_list",      "spr_tool_canvasmove", "spr_tool_selmove",
    "spr_tool_rectsel",     "spr_tool_freesel",   "spr_tool_magicwand",   "spr_tool_picker",     "spr_tool_pen",
    "spr_tool_eraser",      "spr_tool_remap_pen", "spr_tool_paintbucket", "spr_foreground",      "spr_background",
};

struct ToolAction
{
    const char* id;
    ImageEditor::Tool tool;
};
const ToolAction ToolActions[] = {
    {"spr_tool_canvasmove", ImageEditor::Tool::CanvasMove},
    {"spr_tool_selmove", ImageEditor::Tool::SelectionMove},
    {"spr_tool_rectsel", ImageEditor::Tool::RectSelect},
    {"spr_tool_freesel", ImageEditor::Tool::FreeSelect},
    {"spr_tool_magicwand", ImageEditor::Tool::MagicWand},
    {"spr_tool_picker", ImageEditor::Tool::Picker},
    {"spr_tool_pen", ImageEditor::Tool::Pen},
    {"spr_tool_eraser", ImageEditor::Tool::Eraser},
    {"spr_tool_remap_pen", ImageEditor::Tool::RemapPen},
    {"spr_tool_paintbucket", ImageEditor::Tool::PaintBucket},
};

const QString ImageMimeType = QStringLiteral("application/x-openfactorystudio-image");
Formats::Image& copiedImage()
{
    static Formats::Image image;
    return image;
}

QImage toQImage(const Formats::Image& image, const Formats::Palette& palette)
{
    Formats::Image rgba = image;
    if (rgba.isIndexed()) {
        rgba.setPalette(palette);
        rgba = rgba.toRgba(false);
    }
    return QImage(rgba.pixels().data(), rgba.width(), rgba.height(), rgba.width() * 4, QImage::Format_RGBA8888).copy();
}

Formats::Image fromQImage(const QImage& image)
{
    const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    std::vector<std::uint8_t> pixels(std::size_t(rgba.width()) * std::size_t(rgba.height()) * 4);
    for (int y = 0; y < rgba.height(); ++y)
        std::memcpy(pixels.data() + std::size_t(y) * std::size_t(rgba.width()) * 4, rgba.constScanLine(y),
                    std::size_t(rgba.width()) * 4);
    return Formats::Image::rgba(rgba.width(), rgba.height(), std::move(pixels));
}

} // namespace

bool SpritesEditor::imageMode() const
{
    return m_page && m_page->imageMode();
}

bool SpritesEditor::confirmLeave()
{
    if (!imageMode())
        return true;
    const bool modified = m_page->imageEditor()->isModified();
    if (!m_page->confirmDiscardImage())
        return false;
    if (modified)
        m_page->reloadImage();
    return true;
}

void SpritesEditor::bindImageControls(QWidget* panel)
{
    ActionRegistry& actions = m_context.actions();
    m_layers = panel ? panel->findChild<QListView*>(QStringLiteral("spr_layers_list")) : nullptr;
    m_blend = panel ? panel->findChild<QComboBox*>(QStringLiteral("spr_layers_blendmode")) : nullptr;
    m_opacity = panel ? panel->findChild<QSpinBox*>(QStringLiteral("spr_layers_opacity")) : nullptr;
    m_foreground = panel ? panel->findChild<QWidget*>(QStringLiteral("spr_foreground")) : nullptr;
    m_background = panel ? panel->findChild<QWidget*>(QStringLiteral("spr_background")) : nullptr;

    if (m_layers) {
        if (!m_layerModel) {
            m_layerModel = new QStandardItemModel(this);
            connect(m_layerModel, &QStandardItemModel::itemChanged, this, [this](QStandardItem* item) {
                if (m_updating || !imageMode())
                    return;
                ImageEditor* editor = m_page->imageEditor();
                ImageDocument doc = editor->document();
                const int layer = int(doc.layers.size()) - 1 - item->row();
                if (layer < 0 || layer >= int(doc.layers.size()))
                    return;
                const bool visible = item->checkState() == Qt::Checked;
                if (doc.layers[layer].visible != visible) {
                    doc.layers[layer].visible = visible; // not an undo step
                    editor->change(doc);
                } else if (doc.layers[layer].name != item->text() && !item->text().isEmpty()) {
                    doc.layers[layer].name = item->text();
                    editor->apply(tr("Rename layer"), doc);
                } else {
                    refreshLayers();
                }
            });
        }
        m_layers->setModel(m_layerModel);
        m_layers->setSelectionMode(QAbstractItemView::SingleSelection);
        m_layers->setEditTriggers(QAbstractItemView::DoubleClicked);
        connect(m_layers->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
                [this](const QModelIndex& index) {
                    if (m_updating || !imageMode() || !index.isValid())
                        return;
                    ImageEditor* editor = m_page->imageEditor();
                    ImageDocument doc = editor->document();
                    const int layer = int(doc.layers.size()) - 1 - index.row();
                    if (layer >= 0 && layer < int(doc.layers.size()) && layer != doc.current) {
                        doc.current = layer;
                        editor->change(doc);
                    }
                });
    }
    if (m_blend) {
        m_blend->clear();
        m_blend->addItems(blendModeNames());
        connect(m_blend, &QComboBox::currentIndexChanged, this, [this](int mode) {
            if (m_updating || !imageMode() || mode < 0)
                return;
            ImageEditor* editor = m_page->imageEditor();
            ImageDocument doc = editor->document();
            if (doc.layers.isEmpty() || doc.layer().blend == mode)
                return;
            doc.layer().blend = mode;
            editor->apply(tr("Blend mode"), doc);
        });
    }
    if (m_opacity) {
        m_opacity->setRange(0, 100);
        connect(m_opacity, &QSpinBox::valueChanged, this, [this](int opacity) {
            if (m_updating || !imageMode())
                return;
            ImageEditor* editor = m_page->imageEditor();
            ImageDocument doc = editor->document();
            if (doc.layers.isEmpty() || doc.layer().opacity == opacity)
                return;
            doc.layer().opacity = opacity;
            editor->apply(tr("Opacity"), doc);
        });
    }
    for (QWidget* swatch : {m_foreground.data(), m_background.data()}) {
        if (swatch)
            swatch->installEventFilter(this);
    }
    if (auto* button = panel ? panel->findChild<QToolButton*>(QStringLiteral("spr_layers_menu")) : nullptr) {
        auto* menu = new QMenu(button);
        for (const char* id : {"spr_layer_add", "spr_layer_delete", "spr_layer_duplicate", "", "spr_layer_mergevis",
                               "spr_layer_mergeall", "", "spr_layer_save", "spr_layer_play"}) {
            if (!*id)
                menu->addSeparator();
            else if (QAction* a = actions.action(QString::fromLatin1(id)))
                menu->addAction(a);
        }
        if (QAction* duplicate = actions.action(QStringLiteral("spr_layer_duplicate")))
            button->setDefaultAction(duplicate);
        button->setMenu(menu);
        button->setPopupMode(QToolButton::InstantPopup);
    }
    updateMode();
    refreshLayers();
}

void SpritesEditor::updateMode()
{
    if (!m_panel)
        return;
    const bool image = imageMode();
    for (const char* name : AlignControls) {
        for (QWidget* w : m_panel->findChildren<QWidget*>(QLatin1String(name)))
            w->setVisible(!image);
    }
    for (const char* name : ImageControls) {
        for (QWidget* w : m_panel->findChildren<QWidget*>(QLatin1String(name)))
            w->setVisible(image);
    }
}

void SpritesEditor::imageModeSwitched()
{
    if (imageMode()) {
        m_page->imageEditor()->setTool(ImageEditor::Tool::CanvasMove);
        syncTool();
    } else {
        m_explorer = false;
    }
    updateMode();
    refreshLayers();
    refreshExplorer();
    Q_EMIT imageModeChanged(imageMode());
    Q_EMIT explorerChanged();
}

void SpritesEditor::syncTool()
{
    if (!m_page)
        return;
    for (const ToolAction& t : ToolActions) {
        if (t.tool == m_page->imageEditor()->tool()) {
            if (QAction* a = m_context.actions().action(QString::fromLatin1(t.id)))
                a->setChecked(true);
        }
    }
}

void SpritesEditor::refreshLayers()
{
    if (!m_layerModel)
        return;
    const bool updating = std::exchange(m_updating, true);
    const ImageDocument* doc = imageMode() ? &m_page->imageEditor()->document() : nullptr;
    const int count = doc ? int(doc->layers.size()) : 0;
    m_layerModel->setRowCount(count);
    for (int row = 0; row < count; ++row) {
        const ImageLayer& layer = doc->layers[count - 1 - row];
        QStandardItem* item = m_layerModel->item(row);
        if (!item) {
            item = new QStandardItem;
            item->setCheckable(true);
            item->setEditable(true);
            m_layerModel->setItem(row, item);
        }
        if (item->text() != layer.name)
            item->setText(layer.name);
        const Qt::CheckState state = layer.visible ? Qt::Checked : Qt::Unchecked;
        if (item->checkState() != state)
            item->setCheckState(state);
    }
    if (m_layers && count > 0) {
        const QModelIndex current = m_layerModel->index(count - 1 - doc->current, 0);
        if (m_layers->currentIndex() != current)
            m_layers->setCurrentIndex(current);
    }
    const bool rgba = doc && !doc->indexed && count > 0;
    if (m_blend) {
        m_blend->setEnabled(rgba);
        m_blend->setCurrentIndex(count > 0 ? doc->layer().blend : 0);
    }
    if (m_opacity) {
        m_opacity->setEnabled(rgba);
        m_opacity->setValue(count > 0 ? doc->layer().opacity : 100);
    }
    m_updating = updating;
}

bool SpritesEditor::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Paint && m_page && (watched == m_foreground || watched == m_background)) {
        auto* swatch = static_cast<QWidget*>(watched);
        const ImageEditor* editor = m_page->imageEditor();
        QPainter p(swatch);
        p.fillRect(swatch->rect(),
                   editor->displayColor(watched == m_foreground ? editor->foreground() : editor->background()));
        p.setPen(swatch->palette().color(QPalette::Mid));
        p.drawRect(swatch->rect().adjusted(0, 0, -1, -1));
        return true;
    }
    return QObject::eventFilter(watched, event);
}

void SpritesEditor::copyImage()
{
    if (!imageMode())
        return;
    const ImageDocument& doc = m_page->imageEditor()->document();
    if (doc.layers.isEmpty() || doc.width <= 0)
        return;
    copiedImage() = ImageEdits::copy(doc);
    copiedImage().setPalette(doc.palette);
    auto* data = new QMimeData;
    data->setImageData(toQImage(copiedImage(), doc.palette));
    data->setData(ImageMimeType, QByteArray("1"));
    QApplication::clipboard()->setMimeData(data);
}

void SpritesEditor::pasteImage()
{
    if (!imageMode())
        return;
    const QMimeData* data = QApplication::clipboard()->mimeData();
    Formats::Image image;
    if (data && data->hasFormat(ImageMimeType) && !copiedImage().pixelBuffer().empty())
        image = copiedImage();
    else if (data && data->hasImage())
        image = fromQImage(qvariant_cast<QImage>(data->imageData()));
    if (image.pixelBuffer().empty())
        return;
    ImageEditor* editor = m_page->imageEditor();
    ImageDocument doc = editor->document();
    ImageEdits::paste(doc, image);
    editor->apply(tr("Paste"), doc);
}

bool SpritesEditor::performImage(const QString& id)
{
    if (id == QLatin1String("spr_explorer")) {
        // The explorer takes the place of the palettes in image editor mode.
        m_explorer = imageMode() && !m_explorer;
        if (QAction* a = m_context.actions().action(id))
            a->setChecked(m_explorer);
        refreshExplorer();
        Q_EMIT explorerChanged();
        return true;
    }
    if (id == QLatin1String("thumb_save")) {
        if (!explorerShown())
            return false;
        saveExplorerImages();
        return true;
    }
    if (id == QLatin1String("spr_edit_img")) {
        // The action shows the mode, which stays when the user keeps unsaved changes.
        m_page->setImageMode(!m_page->imageMode());
        if (QAction* a = m_context.actions().action(id))
            a->setChecked(m_page->imageMode());
        return true;
    }
    for (const ToolAction& t : ToolActions) {
        if (id == QLatin1String(t.id)) {
            if (imageMode())
                m_page->imageEditor()->setTool(t.tool);
            return true;
        }
    }
    static const QStringList imageActions = {
        QStringLiteral("spr_saveimg"),        QStringLiteral("spr_layer_add"),
        QStringLiteral("spr_layer_delete"),   QStringLiteral("spr_layer_duplicate"),
        QStringLiteral("spr_layer_mergevis"), QStringLiteral("spr_layer_mergeall"),
        QStringLiteral("spr_layer_save"),     QStringLiteral("spr_layer_play"),
        QStringLiteral("spr_view_original"),  QStringLiteral("spr_image_properties"),
        QStringLiteral("spr_select_all"),     QStringLiteral("spr_select_none"),
        QStringLiteral("spr_select_invert"),  QStringLiteral("spr_image_rotate"),
        QStringLiteral("spr_image_fliph"),    QStringLiteral("spr_image_flipv"),
    };
    if (!imageActions.contains(id))
        return false;
    if (!imageMode())
        return true;
    ImageEditor* editor = m_page->imageEditor();
    ImageDocument doc = editor->document();
    QWidget* parent = m_page->window();
    if (doc.layers.isEmpty() || doc.width <= 0 || doc.height <= 0)
        return true;

    if (id == QLatin1String("spr_saveimg")) {
        m_page->saveImage();
    } else if (id == QLatin1String("spr_layer_add")) {
        ImageEdits::addLayer(doc);
        editor->apply(tr("Add layer"), doc);
    } else if (id == QLatin1String("spr_layer_delete")) {
        if (doc.layers.size() > 1) {
            ImageEdits::deleteLayer(doc);
            editor->apply(tr("Delete layer"), doc);
        }
    } else if (id == QLatin1String("spr_layer_duplicate")) {
        ImageEdits::duplicateLayer(doc);
        editor->apply(tr("Duplicate layer"), doc);
    } else if (id == QLatin1String("spr_layer_mergevis")) {
        if (doc.layers.size() > 1) {
            ImageEdits::mergeVisible(doc);
            editor->apply(tr("Merge visible"), doc);
        }
    } else if (id == QLatin1String("spr_layer_mergeall")) {
        if (doc.layers.size() > 1) {
            ImageEdits::mergeAll(doc);
            editor->apply(tr("Merge all"), doc);
        }
    } else if (id == QLatin1String("spr_layer_save")) {
        saveLayersAsSprites();
    } else if (id == QLatin1String("spr_layer_play")) {
        editor->play();
    } else if (id == QLatin1String("spr_view_original")) {
        QAction* a = m_context.actions().action(id);
        editor->canvas()->setOriginalShown(a && a->isChecked());
    } else if (id == QLatin1String("spr_image_properties")) {
        ImagePropertiesDialog dialog(doc.indexed, doc.width, doc.height, parent);
        if (dialog.exec() != QDialog::Accepted)
            return true;
        ImageEdits::setIndexed(doc, dialog.indexed());
        if (dialog.canvasWidth() != doc.width || dialog.canvasHeight() != doc.height)
            ImageEdits::resizeCanvas(doc, dialog.canvasWidth(), dialog.canvasHeight(), editor->background());
        if (doc.indexed != editor->document().indexed || doc.width != editor->document().width ||
            doc.height != editor->document().height)
            editor->apply(tr("Properties"), doc);
    } else if (id == QLatin1String("spr_select_all")) {
        ImageEdits::selectAll(doc);
        editor->change(doc);
    } else if (id == QLatin1String("spr_select_none")) {
        ImageEdits::selectNone(doc);
        editor->change(doc);
    } else if (id == QLatin1String("spr_select_invert")) {
        ImageEdits::invertSelection(doc);
        editor->change(doc);
    } else if (id == QLatin1String("spr_image_rotate")) {
        RotateDialog dialog(parent);
        if (dialog.exec() != QDialog::Accepted || dialog.angle() == 0.0)
            return true;
        static const ImageEdits::RotateFilter filters[] = {ImageEdits::RotateFilter::Nearest,
                                                           ImageEdits::RotateFilter::Bilinear,
                                                           ImageEdits::RotateFilter::Epx, ImageEdits::RotateFilter::Hq};
        ImageEdits::rotate(doc, dialog.angle(), filters[std::clamp(dialog.filter(), 0, 3)], editor->background());
        editor->apply(tr("Rotate"), doc);
    } else if (id == QLatin1String("spr_image_fliph") || id == QLatin1String("spr_image_flipv")) {
        const bool horizontal = id == QLatin1String("spr_image_fliph");
        ImageEdits::flip(doc, horizontal);
        editor->apply(horizontal ? tr("Flip horizontally") : tr("Flip vertically"), doc);
    }
    return true;
}

void SpritesEditor::saveLayersAsSprites()
{
    // Each visible layer named "g,i" goes into that sprite. Sprites other than the current one are replaced after
    // asking. Sprites that do not exist yet are created after the current one.
    ImageEditor* editor = m_page->imageEditor();
    const ImageDocument& image = editor->document();
    SpriteFile* file = m_page->file();
    SpriteDocument doc = file->document();
    const int current = m_page->current();
    if (current < 0 || current >= doc.spriteCount())
        return;
    static const QRegularExpression identifier(QStringLiteral("^\\s*(\\d+)\\s*,\\s*(\\d+)\\s*$"));
    const QPoint shift = image.origin - m_page->savedOrigin();
    const QPoint axis(doc.sprites()[current].header.axisX + shift.x(), doc.sprites()[current].header.axisY + shift.y());
    bool yesToAll = false;
    bool noToAll = false;
    bool changed = false;
    bool currentWritten = false;
    int insertAfter = current;
    for (int i = int(image.layers.size()) - 1; i >= 0; --i) {
        const ImageLayer& layer = image.layers[i];
        const QRegularExpressionMatch match = identifier.match(layer.name);
        if (!layer.visible || !match.hasMatch())
            continue;
        const int group = match.captured(1).toInt();
        const int number = match.captured(2).toInt();
        if (group > 65535 || number > 65535)
            continue;
        Formats::Image pixels = layer.image;
        if (pixels.isIndexed())
            pixels.setPalette(image.palette);
        const int index = doc.findSprite(group, number);
        if (index == current) {
            SpriteEdits::setImage(doc, index, std::move(pixels));
            SpriteEdits::setAxis(doc, index, axis.x(), axis.y());
            currentWritten = true;
        } else if (index >= 0) {
            if (noToAll)
                continue;
            if (!yesToAll) {
                QMessageBox box(
                    QMessageBox::Question, tr("Replace sprite"),
                    tr("The sprite %1 already exists. Do you want to replace it?").arg(layer.name.trimmed()),
                    QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::No | QMessageBox::NoToAll,
                    m_page->window());
                box.setDefaultButton(QMessageBox::No);
                const int answer = box.exec();
                yesToAll = answer == QMessageBox::YesToAll;
                noToAll = answer == QMessageBox::NoToAll;
                if (answer != QMessageBox::Yes && !yesToAll)
                    continue;
            }
            SpriteEdits::setImage(doc, index, std::move(pixels));
        } else {
            const int added = SpriteEdits::duplicate(doc, insertAfter);
            SpriteEdits::setIdentifiers(doc, added, group, number);
            SpriteEdits::setImage(doc, added, std::move(pixels));
            SpriteEdits::setAxis(doc, added, axis.x(), axis.y());
            insertAfter = added;
        }
        changed = true;
    }
    if (changed)
        m_page->applyFromImage(tr("Save visible as sprites"), doc, currentWritten);
}

bool SpritesEditor::explorerShown() const
{
    return m_explorer && imageMode();
}

void SpritesEditor::bindExplorer(QWidget* panel)
{
    m_explorerGroups = panel ? panel->findChild<QComboBox*>(QStringLiteral("thumb_grp_list")) : nullptr;
    m_explorerView = panel ? panel->findChild<QListView*>(QStringLiteral("thumb_view")) : nullptr;
    if (m_explorerView) {
        if (!m_explorerModel)
            m_explorerModel = new QStandardItemModel(this);
        m_explorerView->setModel(m_explorerModel);
        m_explorerView->setViewMode(QListView::IconMode);
        m_explorerView->setMovement(QListView::Static);
        m_explorerView->setResizeMode(QListView::Adjust);
        m_explorerView->setWrapping(true);
        m_explorerView->setSpacing(4);
        m_explorerView->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_explorerView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    }
    if (m_explorerGroups)
        connect(m_explorerGroups, &QComboBox::currentIndexChanged, this, [this] {
            if (!m_updating)
                refreshExplorer();
        });
    const struct
    {
        const char* name;
        bool all;
        bool aligned;
    } buttons[] = {{"thumb_action1", false, false},
                   {"thumb_action2", true, false},
                   {"thumb_action3", false, true},
                   {"thumb_action4", true, true}};
    for (const auto& b : buttons) {
        auto* button = panel ? panel->findChild<QToolButton*>(QLatin1String(b.name)) : nullptr;
        if (!button)
            continue;
        connect(button, &QToolButton::clicked, this, [this, all = b.all, aligned = b.aligned] {
            if (explorerShown() && m_explorerActive)
                addExplorerLayers(all, aligned);
        });
    }
    refreshExplorer();
}

void SpritesEditor::setExplorerActive(bool active)
{
    m_explorerActive = active;
    refreshExplorer();
}

void SpritesEditor::refreshExplorer()
{
    if (!m_explorerModel || !explorerShown() || !m_explorerActive)
        return;
    if (m_explorerView && m_explorerView->model() != m_explorerModel)
        m_explorerView->setModel(m_explorerModel);
    const SpriteDocument& doc = m_page->file()->document();
    const bool updating = std::exchange(m_updating, true);
    QList<int> groups;
    for (const SpriteEntry& entry : doc.sprites()) {
        if (!groups.contains(entry.group()))
            groups << entry.group();
    }
    std::sort(groups.begin(), groups.end());
    int group = groups.isEmpty() ? -1 : groups.first();
    if (m_explorerGroups) {
        if (m_explorerGroups->currentIndex() >= 0 && groups.contains(m_explorerGroups->currentText().toInt()))
            group = m_explorerGroups->currentText().toInt();
        m_explorerGroups->clear();
        for (int g : std::as_const(groups))
            m_explorerGroups->addItem(QString::number(g));
        m_explorerGroups->setCurrentIndex(int(groups.indexOf(group)));
    }
    const int size = std::max(16, m_context.config().intValue(QStringLiteral("thumb_size"), 64));
    m_explorerModel->clear();
    if (m_explorerView)
        m_explorerView->setIconSize(QSize(size, size));
    for (int i = 0; i < doc.spriteCount(); ++i) {
        const SpriteEntry& entry = doc.sprites()[i];
        if (entry.group() != group)
            continue;
        const Formats::Image rgba = doc.imageRgba(i);
        QPixmap thumbnail;
        if (!rgba.pixelBuffer().empty()) {
            const QImage image(rgba.pixels().data(), rgba.width(), rgba.height(), rgba.width() * 4,
                               QImage::Format_RGBA8888);
            thumbnail = QPixmap::fromImage(image.scaled(size, size, Qt::KeepAspectRatio, Qt::FastTransformation));
        }
        auto* item = new QStandardItem(QIcon(thumbnail), QString::number(entry.number()));
        item->setData(i, Qt::UserRole);
        m_explorerModel->appendRow(item);
    }
    m_updating = updating;
}

QList<int> SpritesEditor::explorerSprites(bool all) const
{
    QList<int> sprites;
    if (!m_explorerModel || !m_explorerView)
        return sprites;
    for (int row = 0; row < m_explorerModel->rowCount(); ++row) {
        const QModelIndex index = m_explorerModel->index(row, 0);
        if (all || m_explorerView->selectionModel()->isSelected(index))
            sprites << index.data(Qt::UserRole).toInt();
    }
    return sprites;
}

void SpritesEditor::addExplorerLayers(bool all, bool aligned)
{
    // Each sprite becomes a layer named "g,i", at the canvas origin or with its axis on the current sprite's axis.
    const QList<int> sprites = explorerSprites(all);
    const SpriteDocument& doc = m_page->file()->document();
    const int current = m_page->current();
    if (sprites.isEmpty() || current < 0 || current >= doc.spriteCount())
        return;
    QList<ImageEdits::LayerSource> sources;
    for (int index : sprites) {
        const SpriteEntry& entry = doc.sprites()[index];
        Formats::Image image = doc.image(index);
        if (image.pixelBuffer().empty())
            continue;
        sources << ImageEdits::LayerSource{QStringLiteral("%1,%2").arg(entry.group()).arg(entry.number()),
                                           std::move(image), QPoint(entry.header.axisX, entry.header.axisY)};
    }
    ImageEditor* editor = m_page->imageEditor();
    ImageDocument image = editor->document();
    if (sources.isEmpty() || image.layers.isEmpty())
        return;
    const SpriteEntry& entry = doc.sprites()[current];
    const QPoint axis = image.origin - m_page->savedOrigin() + QPoint(entry.header.axisX, entry.header.axisY);
    ImageEdits::addLayers(image, sources, aligned, axis, editor->background());
    editor->apply(tr("Add layers"), image);
}

void SpritesEditor::saveExplorerImages()
{
    // "<name>_<index>.<suffix>" for every selected sprite.
    const QList<int> sprites = explorerSprites(false);
    if (sprites.isEmpty())
        return;
    SpriteFile* file = m_page->file();
    Config& config = m_context.config();
    QWidget* parent = m_page->window();
    const QString folder = config.value(QStringLiteral("spr_path"), QFileInfo(file->path()).absolutePath());
    const QString path = FileDialogs::getSaveFileName(
        parent, tr("Save sprites"), folder,
        tr("Portable Network Graphic (*.png);;Graphics Interchange Format (*.gif);;Personal Computer eXchange "
           "(*.pcx);;Bitmap (*.bmp)"));
    if (path.isEmpty())
        return;
    const QFileInfo info(path);
    config.setValue(QStringLiteral("spr_path"), info.absolutePath());
    const QString suffix = info.suffix().isEmpty() ? QStringLiteral("png") : info.suffix();
    const QString base = info.suffix().isEmpty() ? info.fileName() : info.completeBaseName();
    const SpriteDocument& doc = file->document();
    for (int index : sprites) {
        const QString out =
            info.dir().filePath(QStringLiteral("%1_%2.%3").arg(base).arg(doc.sprites()[index].number()).arg(suffix));
        QString error;
        if (!writeImageFile(out, doc.image(index), &error)) {
            QMessageBox::warning(parent, tr("Save sprites"), error);
            return;
        }
    }
}

} // namespace OFS
