#include "core/sprites/ImageDocument.h"

#include "core/sprites/QtImages.h"
#include "formats/image/ImageOps.h"
#include "formats/image/Quantize.h"
#include "formats/image/Resize.h"

#include <QCoreApplication>
#include <QImage>
#include <QPainter>
#include <QStringList>

#include <cmath>
#include <deque>
#include <numbers>

namespace OFS {

using namespace Formats;

QStringList blendModeNames()
{
    return {QStringLiteral("Normal"),     QStringLiteral("Add"),         QStringLiteral("Multiply"),
            QStringLiteral("Screen"),     QStringLiteral("Overlay"),     QStringLiteral("Darken"),
            QStringLiteral("Lighten"),    QStringLiteral("Color Dodge"), QStringLiteral("Color Burn"),
            QStringLiteral("Hard Light"), QStringLiteral("Soft Light"),  QStringLiteral("Difference"),
            QStringLiteral("Exclusion")};
}

namespace {

QPainter::CompositionMode compositionMode(int blend)
{
    static const QPainter::CompositionMode modes[] = {
        QPainter::CompositionMode_SourceOver, QPainter::CompositionMode_Plus,
        QPainter::CompositionMode_Multiply,   QPainter::CompositionMode_Screen,
        QPainter::CompositionMode_Overlay,    QPainter::CompositionMode_Darken,
        QPainter::CompositionMode_Lighten,    QPainter::CompositionMode_ColorDodge,
        QPainter::CompositionMode_ColorBurn,  QPainter::CompositionMode_HardLight,
        QPainter::CompositionMode_SoftLight,  QPainter::CompositionMode_Difference,
        QPainter::CompositionMode_Exclusion};
    return modes[std::clamp(blend, 0, 12)];
}

Image blankLayer(const ImageDocument& doc)
{
    const std::size_t count = std::size_t(doc.width) * std::size_t(doc.height);
    return doc.indexed ? Image::indexed(doc.width, doc.height, std::vector<std::uint8_t>(count, 0), doc.palette)
                       : Image::rgba(doc.width, doc.height, std::vector<std::uint8_t>(count * 4, 0));
}

QString nextLayerName(const ImageDocument& doc)
{
    return QCoreApplication::translate("OFS::ImageEdits", "Layer %1").arg(doc.layers.size() + 1);
}

QList<QPoint> line(QPoint a, QPoint b)
{
    QList<QPoint> points;
    int x = a.x();
    int y = a.y();
    const int dx = std::abs(b.x() - x);
    const int dy = -std::abs(b.y() - y);
    const int sx = x < b.x() ? 1 : -1;
    const int sy = y < b.y() ? 1 : -1;
    int error = dx + dy;
    while (true) {
        points << QPoint(x, y);
        if (x == b.x() && y == b.y())
            break;
        const int e2 = 2 * error;
        if (e2 >= dy) {
            error += dy;
            x += sx;
        }
        if (e2 <= dx) {
            error += dx;
            y += sy;
        }
    }
    return points;
}

template <typename Apply>
void stamp(const ImageDocument& doc, QPoint from, QPoint to, int size, Apply apply)
{
    size = std::max(1, size);
    std::vector<std::uint8_t> done(std::size_t(doc.width) * std::size_t(doc.height), 0);
    for (const QPoint& p : line(from, to)) {
        for (int y = p.y(); y < p.y() + size; ++y) {
            for (int x = p.x(); x < p.x() + size; ++x) {
                if (!doc.contains(x, y))
                    continue;
                std::uint8_t& seen = done[std::size_t(y) * std::size_t(doc.width) + std::size_t(x)];
                if (seen)
                    continue;
                seen = 1;
                apply(x, y);
            }
        }
    }
}

bool same(const ImageDocument& doc, const PaintColor& a, const PaintColor& b)
{
    return doc.indexed ? a.index == b.index : a.rgba == b.rgba;
}

std::vector<std::uint8_t> region(const ImageDocument& doc, QPoint at, bool contiguous)
{
    std::vector<std::uint8_t> mask(std::size_t(doc.width) * std::size_t(doc.height), 0);
    if (!doc.contains(at.x(), at.y()))
        return mask;
    const PaintColor target = doc.pixel(at.x(), at.y());
    if (!contiguous) {
        for (int y = 0; y < doc.height; ++y)
            for (int x = 0; x < doc.width; ++x)
                mask[std::size_t(y) * std::size_t(doc.width) + std::size_t(x)] = same(doc, doc.pixel(x, y), target);
        return mask;
    }
    std::deque<QPoint> queue{at};
    mask[std::size_t(at.y()) * std::size_t(doc.width) + std::size_t(at.x())] = 1;
    while (!queue.empty()) {
        const QPoint p = queue.front();
        queue.pop_front();
        for (const QPoint d : {QPoint(1, 0), QPoint(-1, 0), QPoint(0, 1), QPoint(0, -1)}) {
            const QPoint n = p + d;
            if (!doc.contains(n.x(), n.y()))
                continue;
            std::uint8_t& m = mask[std::size_t(n.y()) * std::size_t(doc.width) + std::size_t(n.x())];
            if (m || !same(doc, doc.pixel(n.x(), n.y()), target))
                continue;
            m = 1;
            queue.push_back(n);
        }
    }
    return mask;
}

Image fromQImage(const QImage& image)
{
    const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    std::vector<std::uint8_t> pixels(std::size_t(rgba.width()) * std::size_t(rgba.height()) * 4);
    for (int y = 0; y < rgba.height(); ++y)
        std::memcpy(pixels.data() + std::size_t(y) * std::size_t(rgba.width()) * 4, rgba.constScanLine(y),
                    std::size_t(rgba.width()) * 4);
    return Image::rgba(rgba.width(), rgba.height(), std::move(pixels));
}

void placeOnCanvas(ImageDocument& doc, const QRect& rect, const PaintColor& background, int except = -1)
{
    const QPoint shift = -rect.topLeft();
    ImageDocument resized = doc;
    resized.width = rect.width();
    resized.height = rect.height();
    for (int i = 0; i < doc.layers.size(); ++i) {
        if (i == except)
            continue;
        Image canvas = blankLayer(resized);
        if (i == 0) {
            for (int y = 0; y < resized.height; ++y)
                for (int x = 0; x < resized.width; ++x) {
                    if (resized.indexed)
                        canvas.setIndex(x, y, std::uint8_t(background.index));
                    else
                        canvas.setColor(x, y, background.rgba);
                }
        }
        const Image& old = doc.layers[i].image;
        for (int y = 0; y < doc.height; ++y) {
            for (int x = 0; x < doc.width; ++x) {
                const int nx = x + shift.x();
                const int ny = y + shift.y();
                if (!canvas.contains(nx, ny))
                    continue;
                if (doc.indexed)
                    canvas.setIndex(nx, ny, old.indexAt(x, y));
                else
                    canvas.setColor(nx, ny, old.colorAt(x, y, false));
            }
        }
        resized.layers[i].image = std::move(canvas);
    }
    if (doc.hasSelection()) {
        resized.selection.assign(std::size_t(resized.width) * std::size_t(resized.height), 0);
        for (int y = 0; y < doc.height; ++y)
            for (int x = 0; x < doc.width; ++x) {
                const int nx = x + shift.x();
                const int ny = y + shift.y();
                if (resized.contains(nx, ny))
                    resized.selection[std::size_t(ny) * std::size_t(resized.width) + std::size_t(nx)] =
                        doc.selection[std::size_t(y) * std::size_t(doc.width) + std::size_t(x)];
            }
    }
    resized.origin += shift;
    doc = std::move(resized);
}

Image convertTo(const ImageDocument& doc, const Image& image)
{
    if (doc.indexed) {
        if (image.isIndexed()) {
            Image copy = image;
            copy.setPalette(doc.palette);
            return copy;
        }
        AdaptOptions options;
        return adaptToPalette(image, doc.palette, options);
    }
    return image.isIndexed() ? image.toRgba(true) : image;
}

} // namespace

ImageDocument ImageDocument::fromImage(const Image& image, const QString& name)
{
    ImageDocument doc;
    doc.indexed = image.isIndexed();
    doc.palette = image.palette();
    doc.width = image.width();
    doc.height = image.height();
    doc.layers << ImageLayer{name, image, true, 0, 100};
    return doc;
}

bool ImageDocument::editable(int x, int y) const
{
    if (!contains(x, y))
        return false;
    return selection.empty() || selection[std::size_t(y) * std::size_t(width) + std::size_t(x)] != 0;
}

PaintColor ImageDocument::pixel(int x, int y) const
{
    PaintColor color;
    if (!contains(x, y))
        return color;
    if (indexed)
        color.index = layer().image.indexAt(x, y);
    else
        color.rgba = layer().image.colorAt(x, y, false);
    return color;
}

void ImageDocument::setPixel(int x, int y, const PaintColor& color)
{
    if (!contains(x, y))
        return;
    if (indexed)
        layer().image.setIndex(x, y, std::uint8_t(std::clamp(color.index, 0, 255)));
    else
        layer().image.setColor(x, y, color.rgba);
}

namespace ImageEdits {

bool paint(ImageDocument& doc, QPoint from, QPoint to, int size, const PaintColor& color)
{
    bool changed = false;
    stamp(doc, from, to, size, [&](int x, int y) {
        if (!doc.editable(x, y) || same(doc, doc.pixel(x, y), color))
            return;
        doc.setPixel(x, y, color);
        changed = true;
    });
    return changed;
}

bool remap(ImageDocument& doc, QPoint from, QPoint to, int size, const QList<QPair<PaintColor, PaintColor>>& slots)
{
    bool changed = false;
    stamp(doc, from, to, size, [&](int x, int y) {
        if (!doc.editable(x, y))
            return;
        const PaintColor current = doc.pixel(x, y);
        const PaintColor* replacement = nullptr;
        for (const auto& slot : slots) {
            if (same(doc, slot.first, current))
                replacement = &slot.second;
        }
        if (replacement && !same(doc, *replacement, current)) {
            doc.setPixel(x, y, *replacement);
            changed = true;
        }
    });
    return changed;
}

bool fill(ImageDocument& doc, QPoint at, const PaintColor& color, bool contiguous)
{
    if (!doc.contains(at.x(), at.y()) || same(doc, doc.pixel(at.x(), at.y()), color))
        return false;
    const std::vector<std::uint8_t> area = region(doc, at, contiguous);
    bool changed = false;
    for (int y = 0; y < doc.height; ++y) {
        for (int x = 0; x < doc.width; ++x) {
            if (area[std::size_t(y) * std::size_t(doc.width) + std::size_t(x)] && doc.editable(x, y)) {
                doc.setPixel(x, y, color);
                changed = true;
            }
        }
    }
    return changed;
}

PaintColor pick(const ImageDocument& doc, QPoint at)
{
    PaintColor color = doc.pixel(at.x(), at.y());
    if (!doc.indexed && color.rgba.a != 0)
        color.rgba.a = 255; // the alpha is not kept
    return color;
}

void selectColor(ImageDocument& doc, QPoint at, bool contiguous, SelectMode mode)
{
    const std::vector<std::uint8_t> area = region(doc, at, contiguous);
    const std::size_t count = area.size();
    std::vector<std::uint8_t> mask;
    switch (mode) {
    case SelectMode::Replace:
        mask = area;
        break;
    case SelectMode::Add:
        mask = doc.hasSelection() ? doc.selection : std::vector<std::uint8_t>(count, 0);
        for (std::size_t i = 0; i < count; ++i)
            mask[i] = mask[i] || area[i];
        break;
    case SelectMode::Subtract:
        mask = doc.hasSelection() ? doc.selection : std::vector<std::uint8_t>(count, 1);
        for (std::size_t i = 0; i < count; ++i)
            mask[i] = mask[i] && !area[i];
        break;
    case SelectMode::AllExcept:
        mask.resize(count);
        for (std::size_t i = 0; i < count; ++i)
            mask[i] = !area[i];
        break;
    }
    doc.selection = std::move(mask);
}

void selectRect(ImageDocument& doc, const QRect& rect)
{
    const QRect clipped = rect.normalized().intersected(QRect(0, 0, doc.width, doc.height));
    if (rect.width() == 0 || rect.height() == 0 || clipped.isEmpty()) {
        doc.selection.clear();
        return;
    }
    doc.selection.assign(std::size_t(doc.width) * std::size_t(doc.height), 0);
    for (int y = clipped.top(); y <= clipped.bottom(); ++y)
        for (int x = clipped.left(); x <= clipped.right(); ++x)
            doc.selection[std::size_t(y) * std::size_t(doc.width) + std::size_t(x)] = 1;
}

void paintSelection(ImageDocument& doc, QPoint from, QPoint to, int size, bool add)
{
    if (!doc.hasSelection())
        doc.selection.assign(std::size_t(doc.width) * std::size_t(doc.height), add ? 0 : 1);
    stamp(doc, from, to, size,
          [&](int x, int y) { doc.selection[std::size_t(y) * std::size_t(doc.width) + std::size_t(x)] = add ? 1 : 0; });
}

void selectAll(ImageDocument& doc)
{
    doc.selection.clear();
}

void selectNone(ImageDocument& doc)
{
    doc.selection.assign(std::size_t(doc.width) * std::size_t(doc.height), 0);
}

void invertSelection(ImageDocument& doc)
{
    if (!doc.hasSelection()) {
        selectNone(doc);
        return;
    }
    for (std::uint8_t& m : doc.selection)
        m = m ? 0 : 1;
}

bool moveSelection(ImageDocument& doc, int dx, int dy, const PaintColor& background)
{
    if (dx == 0 && dy == 0)
        return false;
    struct Moved
    {
        int x;
        int y;
        PaintColor color;
    };
    std::vector<Moved> moved;
    for (int y = 0; y < doc.height; ++y)
        for (int x = 0; x < doc.width; ++x)
            if (doc.editable(x, y))
                moved.push_back({x, y, doc.pixel(x, y)});
    if (moved.empty())
        return false;
    for (const Moved& m : moved)
        doc.setPixel(m.x, m.y, background);
    for (const Moved& m : moved)
        doc.setPixel(m.x + dx, m.y + dy, m.color);
    if (doc.hasSelection()) {
        std::vector<std::uint8_t> mask(doc.selection.size(), 0);
        for (const Moved& m : moved)
            if (doc.contains(m.x + dx, m.y + dy))
                mask[std::size_t(m.y + dy) * std::size_t(doc.width) + std::size_t(m.x + dx)] = 1;
        doc.selection = std::move(mask);
    }
    return true;
}

void addLayer(ImageDocument& doc)
{
    ImageLayer layer{nextLayerName(doc), blankLayer(doc), true, 0, 100};
    doc.layers << layer;
    doc.current = int(doc.layers.size()) - 1;
}

void deleteLayer(ImageDocument& doc)
{
    if (doc.layers.size() < 2)
        return;
    doc.layers.removeAt(doc.current);
    doc.current = int(doc.layers.size()) - 1;
}

void duplicateLayer(ImageDocument& doc)
{
    ImageLayer copy = doc.layer();
    copy.name = QCoreApplication::translate("OFS::ImageEdits", "%1 copy").arg(copy.name);
    doc.layers.insert(doc.current + 1, copy);
    ++doc.current;
}

void mergeVisible(ImageDocument& doc)
{
    int lowest = -1;
    for (int i = 0; i < doc.layers.size(); ++i) {
        if (doc.layers[i].visible) {
            lowest = i;
            break;
        }
    }
    if (lowest < 0)
        return;
    ImageLayer merged{doc.layers[lowest].name, flatten(doc, true), true, 0, 100};
    QList<ImageLayer> layers;
    int mergedIndex = 0;
    for (int i = 0; i < doc.layers.size(); ++i) {
        if (i == lowest) {
            mergedIndex = int(layers.size());
            layers << merged;
        } else if (!doc.layers[i].visible) {
            layers << doc.layers[i];
        }
    }
    doc.layers = layers;
    doc.current = mergedIndex;
}

void mergeAll(ImageDocument& doc)
{
    if (doc.layers.isEmpty())
        return;
    ImageLayer merged{doc.layers.first().name, flatten(doc, true), true, 0, 100};
    doc.layers = {merged};
    doc.current = 0;
}

void resizeCanvas(ImageDocument& doc, int width, int height, const PaintColor& background)
{
    width = std::max(1, width);
    height = std::max(1, height);
    if (width == doc.width && height == doc.height)
        return;
    const int left = int(std::floor((width - doc.width) / 2.0));
    const int top = int(std::floor((height - doc.height) / 2.0));
    placeOnCanvas(doc, QRect(-left, -top, width, height), background);
}

void setIndexed(ImageDocument& doc, bool indexed)
{
    if (indexed == doc.indexed)
        return;
    if (indexed) {
        if (doc.palette.empty())
            doc.palette = quantize(flatten(doc, false)).palette();
        for (ImageLayer& layer : doc.layers) {
            Image result = Image::indexed(
                doc.width, doc.height, std::vector<std::uint8_t>(std::size_t(doc.width) * std::size_t(doc.height), 0),
                doc.palette);
            for (int y = 0; y < doc.height; ++y)
                for (int x = 0; x < doc.width; ++x) {
                    const Rgba c = layer.image.colorAt(x, y, false);
                    if (c.a >= 128)
                        result.setIndex(x, y, std::uint8_t(nearestColorIndex(doc.palette, c, 0)));
                }
            layer.image = std::move(result);
            layer.blend = 0;
            layer.opacity = 100;
        }
    } else {
        for (ImageLayer& layer : doc.layers) {
            layer.image.setPalette(doc.palette);
            layer.image = layer.image.toRgba(true);
        }
    }
    doc.indexed = indexed;
}

void rotate(ImageDocument& doc, double degrees, RotateFilter filter, const PaintColor& background)
{
    const Image source = doc.layer().image;
    const int w = doc.width;
    const int h = doc.height;
    const double turns = degrees / 90.0;
    Image rotated;
    int nw = w;
    int nh = h;
    auto blank = [&doc](int width, int height) {
        const std::size_t count = std::size_t(width) * std::size_t(height);
        return doc.indexed ? Image::indexed(width, height, std::vector<std::uint8_t>(count, 0), doc.palette)
                           : Image::rgba(width, height, std::vector<std::uint8_t>(count * 4, 0));
    };
    auto copyPixel = [&doc](const Image& from, int x, int y, Image& to, int tx, int ty) {
        if (doc.indexed)
            to.setIndex(tx, ty, from.indexAt(x, y));
        else
            to.setColor(tx, ty, from.colorAt(x, y, false));
    };
    if (std::abs(turns - std::round(turns)) < 1e-9) {
        const int k = ((int(std::lround(turns)) % 4) + 4) % 4;
        if (k == 1 || k == 3)
            std::swap(nw, nh);
        rotated = blank(nw, nh);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const QPoint t = k == 0   ? QPoint(x, y)
                                 : k == 1 ? QPoint(h - 1 - y, x)
                                 : k == 2 ? QPoint(w - 1 - x, h - 1 - y)
                                          : QPoint(y, w - 1 - x);
                copyPixel(source, x, y, rotated, t.x(), t.y());
            }
    } else {
        const double a = degrees * std::numbers::pi / 180.0;
        const double c = std::cos(a);
        const double s = std::sin(a);
        nw = int(std::ceil(std::abs(w * c) + std::abs(h * s) - 1e-9));
        nh = int(std::ceil(std::abs(w * s) + std::abs(h * c) - 1e-9));
        rotated = blank(nw, nh);
        Image magnified;
        int factor = 1;
        if (filter == RotateFilter::Epx || (filter == RotateFilter::Hq && doc.indexed)) {
            magnified = scaleEpx(source, 4);
            factor = 4;
        } else if (filter == RotateFilter::Hq) {
            magnified = scaleHq(source, 4);
            factor = 4;
        }
        const Image& from = factor > 1 ? magnified : source;
        const int fw = w * factor;
        const int fh = h * factor;
        const bool bilinear = !doc.indexed && (filter == RotateFilter::Bilinear || filter == RotateFilter::Hq);
        for (int v = 0; v < nh; ++v) {
            for (int u = 0; u < nw; ++u) {
                const double dx = u + 0.5 - nw / 2.0;
                const double dy = v + 0.5 - nh / 2.0;
                const double sx = dx * c + dy * s + w / 2.0;
                const double sy = -dx * s + dy * c + h / 2.0;
                if (sx < 0 || sy < 0 || sx >= w || sy >= h) {
                    if (doc.indexed)
                        rotated.setIndex(u, v, std::uint8_t(background.index));
                    else
                        rotated.setColor(u, v, background.rgba);
                    continue;
                }
                if (bilinear) {
                    const double fx = std::clamp(sx * factor - 0.5, 0.0, fw - 1.0);
                    const double fy = std::clamp(sy * factor - 0.5, 0.0, fh - 1.0);
                    const int x0 = int(fx);
                    const int y0 = int(fy);
                    const int x1 = std::min(x0 + 1, fw - 1);
                    const int y1 = std::min(y0 + 1, fh - 1);
                    const double tx = fx - x0;
                    const double ty = fy - y0;
                    auto mix = [&](auto channel) {
                        const double top =
                            channel(from.colorAt(x0, y0, false)) * (1 - tx) + channel(from.colorAt(x1, y0, false)) * tx;
                        const double bottom =
                            channel(from.colorAt(x0, y1, false)) * (1 - tx) + channel(from.colorAt(x1, y1, false)) * tx;
                        return std::uint8_t(std::lround(top * (1 - ty) + bottom * ty));
                    };
                    rotated.setColor(u, v,
                                     {mix([](Rgba p) { return p.r; }), mix([](Rgba p) { return p.g; }),
                                      mix([](Rgba p) { return p.b; }), mix([](Rgba p) { return p.a; })});
                } else {
                    copyPixel(from, std::min(int(sx * factor), fw - 1), std::min(int(sy * factor), fh - 1), rotated, u,
                              v);
                }
            }
        }
    }
    const int current = doc.current;
    const int left = int(std::floor((nw - w) / 2.0));
    const int top = int(std::floor((nh - h) / 2.0));
    placeOnCanvas(doc, QRect(-left, -top, nw, nh), background, current);
    doc.layers[current].image = std::move(rotated);
    doc.selection.clear();
}

void flip(ImageDocument& doc, bool horizontal)
{
    for (ImageLayer& layer : doc.layers)
        layer.image = horizontal ? flipHorizontal(layer.image) : flipVertical(layer.image);
    if (doc.hasSelection()) {
        std::vector<std::uint8_t> mask(doc.selection.size());
        for (int y = 0; y < doc.height; ++y)
            for (int x = 0; x < doc.width; ++x) {
                const int sx = horizontal ? doc.width - 1 - x : x;
                const int sy = horizontal ? y : doc.height - 1 - y;
                mask[std::size_t(y) * std::size_t(doc.width) + std::size_t(x)] =
                    doc.selection[std::size_t(sy) * std::size_t(doc.width) + std::size_t(sx)];
            }
        doc.selection = std::move(mask);
    }
}

Image layerRgba(const ImageDocument& doc, int index)
{
    Image image = doc.layers[index].image;
    if (image.isIndexed()) {
        image.setPalette(doc.palette);
        return image.toRgba(true);
    }
    return image;
}

Image flatten(const ImageDocument& doc, bool visibleOnly)
{
    if (doc.indexed) {
        Image result = blankLayer(doc);
        for (const ImageLayer& layer : doc.layers) {
            if (visibleOnly && !layer.visible)
                continue;
            for (int y = 0; y < doc.height; ++y)
                for (int x = 0; x < doc.width; ++x) {
                    const std::uint8_t index = layer.image.indexAt(x, y);
                    if (index != 0)
                        result.setIndex(x, y, index);
                }
        }
        return result;
    }
    QImage canvas(doc.width, doc.height, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    for (int i = 0; i < doc.layers.size(); ++i) {
        const ImageLayer& layer = doc.layers[i];
        if (visibleOnly && !layer.visible)
            continue;
        painter.setCompositionMode(compositionMode(layer.blend));
        painter.setOpacity(std::clamp(layer.opacity, 0, 100) / 100.0);
        painter.drawImage(0, 0, toQImage(layerRgba(doc, i)));
    }
    painter.end();
    return fromQImage(canvas);
}

void addLayers(ImageDocument& doc, const QList<LayerSource>& sources, bool aligned, QPoint axis,
               const PaintColor& background)
{
    if (sources.isEmpty())
        return;
    // Sprites the 8-bit canvas cannot hold as they are make it RGBA.
    for (const LayerSource& source : sources) {
        if (doc.indexed && (!source.image.isIndexed() || !sameRgb(source.image.palette(), doc.palette) ||
                            source.image.palette().size() != doc.palette.size()))
            setIndexed(doc, false);
    }
    QRect bounds(0, 0, doc.width, doc.height);
    QList<QPoint> positions;
    for (const LayerSource& source : sources) {
        const QPoint at = aligned ? axis - source.axis : QPoint(0, 0);
        positions << at;
        bounds |= QRect(at, QSize(source.image.width(), source.image.height()));
    }
    const QPoint shift = -bounds.topLeft();
    placeOnCanvas(doc, bounds, background);
    for (int n = 0; n < sources.size(); ++n) {
        const Image image = convertTo(doc, sources[n].image);
        Image layerImage = blankLayer(doc);
        const QPoint at = positions[n] + shift;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x) {
                if (doc.indexed)
                    layerImage.setIndex(at.x() + x, at.y() + y, image.indexAt(x, y));
                else
                    layerImage.setColor(at.x() + x, at.y() + y, image.colorAt(x, y, false));
            }
        doc.layers.insert(n, ImageLayer{sources[n].name, std::move(layerImage), true, 0, 100});
    }
    doc.current += int(sources.size());
}

Image copy(const ImageDocument& doc)
{
    Image result = blankLayer(doc);
    for (int y = 0; y < doc.height; ++y)
        for (int x = 0; x < doc.width; ++x) {
            if (!doc.editable(x, y))
                continue;
            if (doc.indexed)
                result.setIndex(x, y, doc.layer().image.indexAt(x, y));
            else
                result.setColor(x, y, doc.layer().image.colorAt(x, y, false));
        }
    return result;
}

void paste(ImageDocument& doc, const Image& image)
{
    const Image converted = convertTo(doc, image);
    Image layerImage = blankLayer(doc);
    for (int y = 0; y < converted.height() && y < doc.height; ++y)
        for (int x = 0; x < converted.width() && x < doc.width; ++x) {
            if (doc.indexed)
                layerImage.setIndex(x, y, converted.indexAt(x, y));
            else
                layerImage.setColor(x, y, converted.colorAt(x, y, false));
        }
    doc.layers << ImageLayer{nextLayerName(doc), std::move(layerImage), true, 0, 100};
    doc.current = int(doc.layers.size()) - 1;
}

} // namespace ImageEdits

} // namespace OFS
