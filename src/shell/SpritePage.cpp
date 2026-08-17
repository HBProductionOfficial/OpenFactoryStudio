#include "shell/SpritePage.h"

#include "core/settings/Config.h"
#include "core/sprites/SpriteEdits.h"
#include "core/sprites/SpriteFile.h"
#include "core/widgets/GraphicsView.h"

#include <QVBoxLayout>

namespace OFS {

SpritePage::SpritePage(SpriteFile* file, QWidget* parent)
    : QWidget(parent)
    , m_file(file)
    , m_view(new GraphicsView(this))
{
    m_view->setObjectName(QStringLiteral("spriteView"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);

    connect(m_file, &SpriteFile::documentChanged, this, [this] {
        const int count = m_file->document().spriteCount();
        if (m_current >= count)
            setCurrent(count - 1);
        else
            refresh();
    });
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
    m_current = index;
    refresh();
    Q_EMIT currentChanged(m_current);
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
    if (!m_settingsApplied) {
        m_settingsApplied = true;
        m_view->setZoomIndex(config.intValue(QStringLiteral("zoom_level"), 2));
        m_view->setOrigin(
            QPoint(config.intValue(QStringLiteral("origin_x"), 320), config.intValue(QStringLiteral("origin_y"), 360)));
        connect(m_view, &GraphicsView::zoomChanged, this,
                [&config](int index) { config.setValue(QStringLiteral("zoom_level"), index); });
        connect(m_view, &GraphicsView::originChanged, this, [&config](const QPoint& origin) {
            config.setValue(QStringLiteral("origin_x"), origin.x());
            config.setValue(QStringLiteral("origin_y"), origin.y());
        });
    }
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
    const Formats::Image rgba = m_file->document().imageRgba(index, nullptr, m_showTransparent);
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
        images << GraphicsView::Image{spriteImage(m_onion), place(m_onion), m_onionOpacity, false};
    if (m_current >= 0 && m_current < doc.spriteCount())
        images << GraphicsView::Image{spriteImage(m_current), place(m_current) + m_dragOffset, 1.0, true};
    m_view->setImages(images);
}

} // namespace OFS
