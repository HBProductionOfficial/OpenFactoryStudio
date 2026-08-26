#include "core/anims/AirGif.h"

#include "core/sprites/SpriteDocument.h"
#include "formats/codecs/Gif.h"
#include "formats/image/Quantize.h"

#include <QCoreApplication>
#include <QRectF>
#include <QTransform>

#include <cmath>

namespace OFS {

using namespace Formats;

QList<int> gifDelays(const QList<int>& times, double velocity)
{
    QList<int> delays;
    if (velocity <= 0)
        velocity = 1.0;
    qint64 total = 0;
    qint64 shown = 0;
    for (int time : times) {
        if (time < 0) {
            delays << 65535;
            break;
        }
        total += time;
        // Rounded down. The epsilon absorbs the binary error of velocities like 0.1.
        const qint64 end = qint64(std::floor(double(total) * 100.0 / (60.0 * velocity) + 1e-7));
        delays << int(end - shown);
        shown = end;
    }
    return delays;
}

namespace {

struct Placed
{
    Image image;
    QPoint axis;
    QTransform transform;
    QRectF bounds;
};

} // namespace

QByteArray animationGif(const AirAnimation& animation, const SpriteDocument& sprites, double scale, double velocity,
                        QString* error)
{
    auto fail = [error](const QString& message) {
        if (error)
            *error = message;
        return QByteArray();
    };
    if (scale <= 0)
        scale = 1.0;
    QList<const AirFrame*> frames;
    QList<int> times;
    for (const AirFrame& frame : animation.frames) {
        frames << &frame;
        times << frame.time;
        if (frame.time < 0)
            break;
    }
    if (frames.isEmpty())
        return fail(QCoreApplication::translate("OFS::AirGif", "The animation has no frames."));

    QList<Placed> placed;
    QRectF canvas;
    for (const AirFrame* frame : std::as_const(frames)) {
        Placed p;
        const int index = frame->group >= 0 ? sprites.findSprite(frame->group, frame->number) : -1;
        if (index >= 0) {
            p.image = sprites.image(index);
            const auto& header = sprites.sprites().at(index).header;
            p.axis = QPoint(header.axisX, header.axisY);
        }
        p.transform.translate(frame->x, frame->y);
        p.transform.rotate(-frame->angle);
        p.transform.scale(frame->xScale * (frame->flipH ? -1 : 1), frame->yScale * (frame->flipV ? -1 : 1));
        if (!p.image.pixelBuffer().empty()) {
            p.bounds =
                p.transform.mapRect(QRectF(-p.axis.x(), -p.axis.y(), p.image.width(), p.image.height())).normalized();
            canvas |= p.bounds;
        }
        placed << p;
    }
    if (canvas.isEmpty())
        return fail(QCoreApplication::translate("OFS::AirGif", "The animation shows no sprites."));
    const QRect units(QPoint(int(std::floor(canvas.left())), int(std::floor(canvas.top()))),
                      QPoint(int(std::ceil(canvas.right())) - 1, int(std::ceil(canvas.bottom())) - 1));

    GifDocument gif;
    gif.width = std::max(1, int(std::lround(units.width() * scale)));
    gif.height = std::max(1, int(std::lround(units.height() * scale)));
    bool shared = true;
    for (const Placed& p : std::as_const(placed)) {
        if (p.image.pixelBuffer().empty())
            continue;
        if (!p.image.isIndexed() || (!gif.globalPalette.empty() && !sameRgb(gif.globalPalette, p.image.palette()))) {
            shared = false;
            break;
        }
        if (gif.globalPalette.empty())
            gif.globalPalette = p.image.palette();
    }
    if (!shared)
        gif.globalPalette.clear();
    for (Rgba& c : gif.globalPalette)
        c.a = 255;

    const QList<int> delays = gifDelays(times, velocity);
    gif.loopCount = times.last() < 0 ? -1 : 0;
    for (int f = 0; f < placed.size(); ++f) {
        const Placed& p = placed.at(f);
        GifFrame out;
        out.delay = delays.value(f);
        out.disposal = GifDisposal::Background;
        out.transparentIndex = 0;
        if (p.image.pixelBuffer().empty()) {
            out.image = Image::indexed(1, 1, {0}, gif.globalPalette.empty() ? Palette(2) : gif.globalPalette);
            gif.frames.push_back(std::move(out));
            continue;
        }
        const int left = std::clamp(int(std::floor((p.bounds.left() - units.left()) * scale)), 0, gif.width - 1);
        const int top = std::clamp(int(std::floor((p.bounds.top() - units.top()) * scale)), 0, gif.height - 1);
        const int right = std::clamp(int(std::ceil((p.bounds.right() - units.left()) * scale)), left + 1, gif.width);
        const int bottom = std::clamp(int(std::ceil((p.bounds.bottom() - units.top()) * scale)), top + 1, gif.height);
        const int w = right - left;
        const int h = bottom - top;
        const QTransform inverse = p.transform.inverted();
        Image source = p.image;
        if (!source.isIndexed() || !shared) {
            if (!source.isIndexed())
                source = quantize(source);
            out.localPalette = true;
        }
        std::vector<std::uint8_t> pixels(std::size_t(w) * std::size_t(h), 0);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const QPointF at(units.left() + (left + x + 0.5) / scale, units.top() + (top + y + 0.5) / scale);
                const QPointF local = inverse.map(at);
                const int sx = int(std::floor(local.x())) + p.axis.x();
                const int sy = int(std::floor(local.y())) + p.axis.y();
                if (sx < 0 || sy < 0 || sx >= source.width() || sy >= source.height())
                    continue;
                pixels[std::size_t(y) * std::size_t(w) + std::size_t(x)] = source.indexAt(sx, sy);
            }
        }
        Palette palette = shared ? gif.globalPalette : source.palette();
        for (Rgba& c : palette)
            c.a = 255;
        out.image = Image::indexed(w, h, std::move(pixels), palette);
        out.left = left;
        out.top = top;
        gif.frames.push_back(std::move(out));
    }
    const auto bytes = writeGif(gif);
    if (!bytes)
        return fail(QString::fromStdString(bytes.error().message));
    return QByteArray(reinterpret_cast<const char*>(bytes.value().data()), qsizetype(bytes.value().size()));
}

} // namespace OFS
