#include "core/bg/BgRenderer.h"

#include "core/anims/AirDocument.h"
#include "core/sprites/QtImages.h"
#include "core/sprites/SpriteDocument.h"

#include <cmath>

namespace OFS {

namespace {

// Closest QPainter match for the engine's source and destination alphas (0-256). Exact for plain
// alpha (s + d = 256) and full addition (d = 256).
std::pair<QPainter::CompositionMode, double> blend(int source, int dest)
{
    const double opacity = std::clamp(source, 0, 256) / 256.0;
    if (dest >= 256)
        return {QPainter::CompositionMode_Plus, opacity};
    return {QPainter::CompositionMode_SourceOver, opacity};
}

QList<double> tiles(double first, double step, double size, int flag, double coverStart, double coverEnd)
{
    if (flag == 0 || step <= 0)
        return {first};
    if (flag >= 2) {
        QList<double> list;
        for (int k = 0; k < std::min(flag, 4096); ++k)
            list << first + k * step;
        return list;
    }
    // Endless (1 or negative).
    const double startK = std::floor((coverStart - size - first) / step);
    const double endK = std::ceil((coverEnd - first) / step);
    QList<double> list;
    for (double k = startK; k <= endK && list.size() < 4096; ++k)
        list << first + k * step;
    return list;
}

} // namespace

BgRenderer::BgRenderer(const SpriteDocument* sprites, const AirDocument* animations)
    : m_sprites(sprites)
    , m_animations(animations)
{}

QImage BgRenderer::sprite(int group, int number, bool masked, QPoint* axis)
{
    const qint64 key = (qint64(group) << 33) | (qint64(number) << 1) | (masked ? 1 : 0);
    auto found = m_cache.constFind(key);
    if (found == m_cache.cend()) {
        std::pair<QImage, QPoint> entry;
        const int index = m_sprites ? m_sprites->findSprite(group, number) : -1;
        if (index >= 0) {
            const Formats::Image rgba = m_sprites->imageRgba(index, nullptr, !masked);
            if (!rgba.pixelBuffer().empty()) {
                entry.first = toQImage(rgba, QImage::Format_ARGB32_Premultiplied);
            }
            const auto& header = m_sprites->sprites()[index].header;
            entry.second = QPoint(header.axisX, header.axisY);
        }
        found = m_cache.insert(key, entry);
    }
    if (axis)
        *axis = found->second;
    return found->first;
}

QImage BgRenderer::project(const QImage& source, double topLeft, double topWidth, double bottomLeft, double bottomWidth,
                           int height, int* x)
{
    const double minX = std::floor(std::min(topLeft, bottomLeft));
    const double maxX = std::ceil(std::max(topLeft + topWidth, bottomLeft + bottomWidth));
    const int width = int(maxX - minX);
    *x = int(minX);
    if (source.isNull() || width <= 0 || height <= 0 || width > 16384 || height > 16384)
        return {};
    const QImage src = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QImage out(width, height, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    for (int row = 0; row < height; ++row) {
        const double t = (row + 0.5) / height;
        const double left = topLeft + (bottomLeft - topLeft) * t - minX;
        const double span = topWidth + (bottomWidth - topWidth) * t;
        if (span <= 0)
            continue;
        const int sourceRow = std::min(src.height() - 1, int(t * src.height()));
        const auto* in = reinterpret_cast<const QRgb*>(src.constScanLine(sourceRow));
        auto* line = reinterpret_cast<QRgb*>(out.scanLine(row));
        const int from = std::max(0, int(std::floor(left)));
        const int to = std::min(width, int(std::ceil(left + span)));
        for (int px = from; px < to; ++px) {
            const int sx = int((px + 0.5 - left) / span * src.width());
            if (sx >= 0 && sx < src.width())
                line[px] = in[sx];
        }
    }
    return out;
}

QList<BgDrawItem> BgRenderer::render(const BgSimulator& simulator, const BgView& view)
{
    QList<BgDrawItem> items;
    const double cx = view.screen.width() / 2;
    const QRectF cover = view.cover.isEmpty() ? QRectF(QPointF(0, 0), view.screen) : view.cover;
    const double camX = view.camera.x();
    const double camY = view.camera.y();
    for (int i = 0; i < simulator.elements().size(); ++i) {
        const BgElement& e = simulator.elements()[i];
        const BgState& s = simulator.states()[i];
        if (!simulator.shown(i) || e.type == BgElement::Type::Null || e.type == BgElement::Type::Dummy)
            continue;
        if (view.layer >= 0 && e.layer != view.layer)
            continue;

        int group = e.spriteGroup;
        int number = e.spriteNumber;
        QPointF frameOffset;
        bool flipH = false;
        bool flipV = false;
        bool animated = false;
        AirTransparency frameTransparency;
        if (e.type == BgElement::Type::Animation || (e.type == BgElement::Type::Parallax && s.action >= 0)) {
            const int a = m_animations ? m_animations->indexOf(s.action) : -1;
            if (a < 0)
                continue;
            const AirAnimation& animation = m_animations->animations()[a];
            const int f = BgSimulator::animationFrame(animation, s.actionTicks);
            if (f < 0 || animation.frames[f].group < 0)
                continue;
            const AirFrame& frame = animation.frames[f];
            group = frame.group;
            number = frame.number;
            frameOffset = QPointF(frame.x, frame.y);
            flipH = frame.flipH;
            flipV = frame.flipV;
            frameTransparency = frame.transparency;
            animated = true;
        }
        QPoint axis;
        QImage image = sprite(group, number, e.mask || animated, &axis);
        if (image.isNull())
            continue;
        if (flipH || flipV) {
            image = image.mirrored(flipH, flipV);
            if (flipH)
                axis.setX(image.width() - axis.x());
            if (flipV)
                axis.setY(image.height() - axis.y());
        }

        // Scale: scalestart and scaledelta (1.1), or yscalestart and yscaledelta in percent for parallax.
        double yPercent = e.type == BgElement::Type::Parallax ? e.yScaleStart + e.yScaleDelta * camY : 100;
        if (yPercent == 0)
            yPercent = 100;
        if (e.yScaleStart * yPercent < 0)
            yPercent = -yPercent;
        const double ys = std::abs(100.0 / yPercent * e.scaleStart.y() + e.scaleDelta.y() * camY * e.delta.y());
        const double xs = std::abs(e.scaleStart.x() + e.scaleDelta.x() * camX * e.delta.x());

        // Where the axis goes, relative to the screen centre (x) and top (y).
        const QPointF position = simulator.position(i);
        double x = position.x() - camX * e.delta.x() + frameOffset.x() * xs;
        const double y = position.y() - camY * e.delta.y() + frameOffset.y() * ys;
        const double w = image.width();
        const double h = image.height();

        double opacity = 1.0;
        QPainter::CompositionMode mode = QPainter::CompositionMode_SourceOver;
        switch (e.trans) {
        case BgElement::Trans::Add:
        case BgElement::Trans::Add1:
            mode = QPainter::CompositionMode_Plus;
            break;
        case BgElement::Trans::Sub:
            mode = QPainter::CompositionMode_Difference;
            break;
        case BgElement::Trans::AddAlpha:
            std::tie(mode, opacity) = blend(e.alphaSource, e.alphaDest);
            break;
        case BgElement::Trans::None:
            if (animated) {
                switch (frameTransparency.mode) {
                case AirTransparency::Mode::None:
                    break;
                case AirTransparency::Mode::Addition:
                    std::tie(mode, opacity) = blend(frameTransparency.source, frameTransparency.dest);
                    break;
                case AirTransparency::Mode::Addition50:
                    mode = QPainter::CompositionMode_Plus;
                    break;
                case AirTransparency::Mode::Subtraction:
                    mode = QPainter::CompositionMode_Difference;
                    break;
                }
            }
            break;
        }

        // "window" counts from the screen top-left and is inclusive, "maskwindow" from the top centre and
        // exclusive. Both follow the camera by the window delta.
        std::optional<QRectF> clip;
        if (e.hasWindow && view.windows) {
            const QRect r = e.window.normalized();
            const double wx = r.left() - camX * e.windowDelta.x() + (e.maskWindow ? cx : 0);
            const double wy = r.top() - camY * e.windowDelta.y();
            const double ww = e.maskWindow ? r.right() - r.left() : r.right() - r.left() + 1;
            const double wh = e.maskWindow ? r.bottom() - r.top() : r.bottom() - r.top() + 1;
            clip = QRectF(wx, wy, std::max(0.0, ww), std::max(0.0, wh));
        }
        auto add = [&](const QImage& picture, QPointF topLeft) {
            BgDrawItem item;
            item.element = i;
            item.image = picture;
            item.topLeft = topLeft;
            item.opacity = opacity;
            item.mode = mode;
            item.clip = clip;
            items << item;
        };

        const double stepX = xs * (animated ? e.tileSpacingX : w + e.tileSpacingX);
        const double stepY = ys * (animated ? e.tileSpacingY : h + e.tileSpacingY);
        const int tileX = animated && e.tileSpacingX == 0 ? 0 : e.tileX;
        const int tileY = animated && e.tileSpacingY == 0 ? 0 : e.tileY;
        const double top = y - axis.y() * ys;

        if (e.type == BgElement::Type::Parallax && (e.hasWidth || (e.hasXScale && e.xScaleTop != 0))) {
            double topScale = xs;
            double bottomScale = xs;
            double raster = 0; // pre-projected (xscale): the bottom moves by this share of x
            if (e.hasWidth && w > 0) {
                topScale = e.widthTop / w * xs;
                bottomScale = e.widthBottom / w * xs;
                x += std::abs(e.scaleStart.x()) * (-e.widthTop / 2 + axis.x() * e.widthTop / w);
            } else {
                raster = (e.xScaleBottom - e.xScaleTop) / e.xScaleTop;
            }
            const int rows = std::max(1, int(std::lround(h * ys)));
            const double tileStep = topScale * (w + e.tileSpacingX);
            const double firstLeft = cx + x - axis.x() * topScale;
            for (double left : tiles(firstLeft, tileStep, w * std::max(topScale, bottomScale), e.tileX, cover.left(),
                                     cover.right())) {
                const double topWidth = w * topScale;
                const double bottomWidth = w * bottomScale;
                const double bottomLeft = e.hasWidth && topScale != 0 ? cx + (left - cx) * (bottomScale / topScale)
                                                                      : left + raster * (left - firstLeft + x);
                int px = 0;
                const QImage projected = project(image, left, topWidth, bottomLeft, bottomWidth, rows, &px);
                if (!projected.isNull())
                    add(projected, QPointF(px, top));
            }
            continue;
        }

        if (xs != 1.0 || ys != 1.0) {
            const QSize size(std::max(1, int(std::lround(w * xs))), std::max(1, int(std::lround(h * ys))));
            image = image.scaled(size, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        }
        const double left = cx + x - axis.x() * xs;
        for (double ty : tiles(top, stepY, image.height(), tileY, cover.top(), cover.bottom())) {
            for (double tx : tiles(left, stepX, image.width(), tileX, cover.left(), cover.right()))
                add(image, QPointF(tx, ty));
        }
    }
    return items;
}

} // namespace OFS
