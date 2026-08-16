#include "shell/SpritePage.h"

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
        m_file->apply(tr("Move sprite"), doc);
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
