#include "shell/SpritePage.h"

#include "core/settings/Config.h"
#include "core/sprites/PaletteEdits.h"
#include "core/sprites/SpriteEdits.h"
#include "core/sprites/SpriteFile.h"
#include "core/widgets/GraphicsView.h"
#include "shell/ImageEditor.h"

#include <QMessageBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <cmath>

namespace OFS {

SpritePage::SpritePage(SpriteFile* file, QWidget* parent)
    : QWidget(parent)
    , m_file(file)
    , m_stack(new QStackedWidget(this))
    , m_view(new GraphicsView(m_stack))
    , m_editor(new ImageEditor(m_stack))
{
    m_view->setObjectName(QStringLiteral("spriteView"));
    m_stack->addWidget(m_view);
    m_stack->addWidget(m_editor);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_stack);

    connect(m_file, &SpriteFile::documentChanged, this, [this] {
        const int count = m_file->document().spriteCount();
        const bool moved = m_current >= count;
        if (moved)
            m_current = std::max(0, count - 1);
        refresh();
        if (imageMode() && !m_savingImage) {
            const bool has = m_current < count;
            const SpriteEntry* entry = has ? &m_file->document().sprites()[m_current] : nullptr;
            if (!entry || entry->pixels != m_openedPixels || entry->palette != m_openedPalette)
                reloadImage();
        }
        if (moved)
            Q_EMIT currentChanged(m_current);
        const int palettes = int(m_file->document().palettes().size());
        if (m_palette >= palettes || (m_palette < 0 && palettes > 0)) {
            m_palette = std::min(std::max(m_palette, 0), palettes - 1);
            Q_EMIT currentPaletteChanged(m_palette);
        }
    });
    connect(m_view, &GraphicsView::pressed, this, [this](const QPointF& units) {
        const SpriteDocument& doc = m_file->document();
        if (!m_picker || m_current < 0 || m_current >= doc.spriteCount())
            return;
        const auto& header = doc.sprites()[m_current].header;
        const int x = int(std::floor(units.x())) + header.axisX;
        const int y = int(std::floor(units.y())) + header.axisY;
        const Formats::Image image = doc.image(m_current);
        if (!image.isIndexed() || x < 0 || y < 0 || x >= image.width() || y >= image.height())
            return;
        Q_EMIT colorPicked(image.pixels()[std::size_t(y) * std::size_t(image.width()) + std::size_t(x)]);
    });
    connect(m_view, &GraphicsView::cursorMoved, this, [this](const QPoint& units) {
        m_cursor = units;
        Q_EMIT statusChanged();
    });
    connect(m_view, &GraphicsView::guidesChanged, this, &SpritePage::statusChanged);
    connect(m_file, &SpriteFile::documentChanged, this, &SpritePage::statusChanged);
    // Dragging the sprite moves it on screen, which moves the axis the other way.
    connect(m_view, &GraphicsView::imageDragged, this, [this](int dx, int dy) {
        m_dragOffset += QPoint(dx, dy);
        refresh();
    });
    connect(m_view, &GraphicsView::dragFinished, this, [this] {
        const QPoint offset = std::exchange(m_dragOffset, QPoint());
        if (offset.isNull() || m_current < 0 || m_current >= m_file->document().spriteCount())
            return;
        SpriteDocument doc = m_file->document();
        SpriteEdits::moveAxes(doc, {m_current}, -offset.x(), -offset.y());
        m_file->applyAxisChange(m_current, doc);
    });
    m_palette = m_file->document().palettes().isEmpty() ? -1 : 0;
    refresh();
}

void SpritePage::setCurrent(int index)
{
    const int count = m_file ? m_file->document().spriteCount() : 0;
    index = count > 0 ? std::clamp(index, 0, count - 1) : 0;
    if (index == m_current && !m_view->images().isEmpty()) {
        refresh();
        return;
    }
    if (index != m_current && !confirmDiscardImage()) {
        Q_EMIT currentChanged(m_current);
        return;
    }
    m_current = index;
    refresh();
    if (imageMode())
        reloadImage();
    Q_EMIT currentChanged(m_current);
}

bool SpritePage::imageMode() const
{
    return m_stack->currentWidget() == m_editor;
}

bool SpritePage::setImageMode(bool on)
{
    if (on == imageMode())
        return true;
    if (!on && !confirmDiscardImage())
        return false;
    if (on) {
        reloadImage();
        m_stack->setCurrentWidget(m_editor);
    } else {
        m_stack->setCurrentWidget(m_view);
        refresh();
    }
    Q_EMIT imageModeChanged(on);
    return true;
}

bool SpritePage::confirmDiscardImage()
{
    if (!imageMode() || !m_editor->isModified())
        return true;
    QMessageBox box(QMessageBox::Question, tr("Discard changes?"),
                    tr("Do you want to discard the changes made in the current sprite?"),
                    QMessageBox::Yes | QMessageBox::No, window());
    box.setDefaultButton(QMessageBox::Yes);
    return box.exec() == QMessageBox::Yes;
}

void SpritePage::reloadImage()
{
    const SpriteDocument& doc = m_file->document();
    m_savedOrigin = QPoint();
    if (m_current < 0 || m_current >= doc.spriteCount()) {
        m_openedPixels = nullptr;
        m_openedPalette = nullptr;
        m_editor->open(Formats::Image(), QString());
        return;
    }
    // The base layer is named after the sprite: "g,i".
    const SpriteEntry& entry = doc.sprites()[m_current];
    m_openedPixels = entry.pixels;
    m_openedPalette = entry.palette;
    m_editor->open(doc.image(m_current), QStringLiteral("%1,%2").arg(entry.group()).arg(entry.number()));
}

void SpritePage::saveImage(bool allInstances)
{
    const ImageDocument& image = m_editor->document();
    if (!imageMode() || m_current < 0 || m_current >= m_file->document().spriteCount() || image.width <= 0 ||
        image.height <= 0)
        return;
    Formats::Image flat = ImageEdits::flatten(image);
    if (flat.isIndexed())
        flat.setPalette(image.palette);
    SpriteDocument doc = m_file->document();
    SpriteEdits::setImage(doc, m_current, std::move(flat), allInstances);
    // The canvas grew around the sprite's image: the axis moves with it.
    const QPoint shift = image.origin - m_savedOrigin;
    if (!shift.isNull())
        SpriteEdits::moveAxes(doc, allInstances ? doc.linkedSprites(m_current) : QList<int>{m_current}, shift.x(),
                              shift.y());
    applyFromImage(tr("Save image changes"), doc, true);
    m_editor->markSaved();
}

void SpritePage::applyFromImage(const QString& text, const SpriteDocument& doc, bool currentSaved)
{
    m_savingImage = true;
    m_file->apply(text, doc);
    m_savingImage = false;
    if (m_current >= 0 && m_current < m_file->document().spriteCount()) {
        const SpriteEntry& entry = m_file->document().sprites()[m_current];
        m_openedPixels = entry.pixels;
        m_openedPalette = entry.palette;
    }
    if (currentSaved)
        m_savedOrigin = m_editor->document().origin;
}

void SpritePage::applySettings(Config& config)
{
    m_view->setZoomLevels(config.intValue(QStringLiteral("zoom_mode"), 1) == 0 ? GraphicsView::smallZoomRange()
                                                                               : GraphicsView::largeZoomRange());
    m_view->setZoomCenter(config.intValue(QStringLiteral("zoom_center"), 0) == 1 ? GraphicsView::ZoomCenter::Workspace
                                                                                 : GraphicsView::ZoomCenter::Axis);
    const QPoint offset(config.intValue(QStringLiteral("background_x"), -426),
                        config.intValue(QStringLiteral("background_y"), -416));
    switch (config.intValue(QStringLiteral("background_type"), 1)) {
    case 0:
        m_view->setBackground(GraphicsView::Background::Color);
        break;
    case 2:
        m_view->setBackgroundPicture(QImage(config.value(QStringLiteral("background_file"))), offset);
        m_view->setBackground(GraphicsView::Background::Picture);
        break;
    default:
        m_view->setBackgroundPicture(GraphicsView::defaultBackground(), offset);
        m_view->setBackground(GraphicsView::Background::Picture);
        break;
    }
    m_view->setRulersVisible(config.boolValue(QStringLiteral("show_rulers"), true));
    m_view->setMainCrossVisible(config.boolValue(QStringLiteral("show_cross1"), true));
    m_view->setGuidesVisible(config.boolValue(QStringLiteral("show_guides"), false));
    m_view->setImagesOverGuides(config.boolValue(QStringLiteral("sprite_over_guides"), false));
    m_view->applyGraphicsSettings(config);
    m_onionEffect = config.intValue(QStringLiteral("onion_effect"), 2);
    if (!m_settingsApplied) {
        m_settingsApplied = true;
        m_view->setZoomIndex(config.intValue(QStringLiteral("zoom_level"), 6));
        m_view->setOrigin(
            QPoint(config.intValue(QStringLiteral("origin_x"), 570), config.intValue(QStringLiteral("origin_y"), 622)));
        connect(m_view, &GraphicsView::zoomChanged, this,
                [&config](int index) { config.setValue(QStringLiteral("zoom_level"), index); });
        connect(m_view, &GraphicsView::originChanged, this, [&config](const QPoint& origin) {
            config.setValue(QStringLiteral("origin_x"), origin.x());
            config.setValue(QStringLiteral("origin_y"), origin.y());
        });
    }
}

void SpritePage::setCurrentPalette(int index)
{
    const int count = int(m_file->document().palettes().size());
    index = count > 0 ? std::clamp(index, 0, count - 1) : -1;
    if (index == m_palette)
        return;
    m_palette = index;
    Q_EMIT currentPaletteChanged(m_palette);
}

void SpritePage::setDisplayPalette(SpritePalettePtr palette)
{
    if (palette == m_displayPalette)
        return;
    m_displayPalette = std::move(palette);
    refresh();
}

void SpritePage::setColorPicker(bool on)
{
    m_picker = on;
    refresh(); // the sprite cannot be dragged while picking
}

void SpritePage::setShowTransparent(bool show)
{
    m_showTransparent = show;
    refresh();
}

void SpritePage::setOnionSkin(int index, double opacity)
{
    m_onion = index;
    m_onionOpacity = opacity;
    refresh();
}

QImage SpritePage::spriteImage(int index) const
{
    const SpriteDocument& doc = m_file->document();
    const bool display = index == m_current && m_displayPalette && PaletteEdits::isIndexed(doc.sprites()[index]);
    const Formats::Image rgba =
        doc.imageRgba(index, display ? &m_displayPalette->palette.colors : nullptr, m_showTransparent);
    if (rgba.pixelBuffer().empty())
        return {};
    return QImage(rgba.pixels().data(), rgba.width(), rgba.height(), rgba.width() * 4, QImage::Format_RGBA8888).copy();
}

void SpritePage::refresh()
{
    QList<GraphicsView::Image> images;
    const SpriteDocument& doc = m_file->document();
    auto place = [&doc](int index) {
        const auto& header = doc.sprites()[index].header;
        return QPoint(-header.axisX, -header.axisY);
    };
    if (m_onion >= 0 && m_onion < doc.spriteCount() && m_onion != m_current)
        images << GraphicsView::Image{GraphicsView::onionEffect(spriteImage(m_onion), m_onionEffect),
                                      place(m_onion),
                                      m_onionOpacity,
                                      false,
                                      {},
                                      {},
                                      {}};
    if (m_current >= 0 && m_current < doc.spriteCount())
        images << GraphicsView::Image{
            spriteImage(m_current), place(m_current) + m_dragOffset, 1.0, !m_picker, {}, {}, {}};
    m_view->setImages(images);
}

} // namespace OFS
