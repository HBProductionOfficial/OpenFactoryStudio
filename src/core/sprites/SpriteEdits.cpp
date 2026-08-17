#include "core/sprites/SpriteEdits.h"

#include "formats/image/ImageOps.h"

#include <QHash>
#include <QPoint>

#include <algorithm>
#include <cmath>

namespace OFS::SpriteEdits {

using namespace Formats;

namespace {

bool valid(const SpriteDocument& doc, int index)
{
    return index >= 0 && index < doc.spriteCount();
}

QList<int> validIndexes(const SpriteDocument& doc, const QList<int>& indexes)
{
    QList<int> result;
    for (int i : indexes) {
        if (valid(doc, i) && !result.contains(i))
            result << i;
    }
    return result;
}

int freePaletteNumber(const SpriteDocument& doc, int group, int start)
{
    for (int n = start; n <= 0xFFFF; ++n) {
        if (doc.findPalette(group, n) < 0)
            return n;
    }
    return start;
}

SpritePalettePtr paletteFor(SpriteDocument& doc, const Palette& colors, int group, int number)
{
    for (const SpritePalettePtr& p : doc.palettes()) {
        if (p->palette.colors.size() == colors.size() && sameRgb(p->palette.colors, colors))
            return p;
    }
    SpritePalettePtr palette = SpriteDocument::makePalette(group, freePaletteNumber(doc, group, number), colors);
    doc.palettes() << palette;
    return palette;
}

// Listed sprites sharing pixels keep sharing the result. Sprites outside the list keep the old
// pixels. transform returns false to leave a sprite alone.
template <typename Transform>
void transformPixels(SpriteDocument& doc, const QList<int>& indexes, Transform transform)
{
    struct Result
    {
        bool changed = false;
        SpritePixelsPtr pixels;
        QPoint shift;
        double scaleX = 1.0;
        double scaleY = 1.0;
    };
    QHash<const SpritePixels*, Result> results;
    for (int index : validIndexes(doc, indexes)) {
        SpriteEntry& entry = doc.sprites()[index];
        auto found = results.find(entry.pixels.get());
        if (found == results.end()) {
            Result result;
            Image image = doc.image(index);
            if (!image.pixelBuffer().empty())
                result.changed = transform(image, result.shift, result.scaleX, result.scaleY);
            if (result.changed)
                result.pixels = SpriteDocument::makePixels(std::move(image));
            found = results.insert(entry.pixels.get(), result);
        }
        if (!found->changed)
            continue;
        entry.pixels = found->pixels;
        entry.header.linkedIndex = -1; // new pixels: nothing of the stored link applies
        entry.header.axisX = std::int16_t(std::lround(entry.header.axisX * found->scaleX) + found->shift.x());
        entry.header.axisY = std::int16_t(std::lround(entry.header.axisY * found->scaleY) + found->shift.y());
    }
}

Image channel(const Image& rgba, int which)
{
    std::vector<std::uint8_t> pixels(std::size_t(rgba.width()) * std::size_t(rgba.height()));
    const std::span<const std::uint8_t> in = rgba.pixels();
    for (std::size_t i = 0; i < pixels.size(); ++i)
        pixels[i] = in[i * 4 + 3] == 0 ? 0 : in[i * 4 + std::size_t(which)];
    return Image::indexed(rgba.width(), rgba.height(), std::move(pixels));
}

Palette ramp(int which)
{
    Palette colors(256);
    for (int i = 0; i < 256; ++i) {
        const auto v = std::uint8_t(i);
        colors[std::size_t(i)] = {which == 0 ? v : std::uint8_t(0), which == 1 ? v : std::uint8_t(0),
                                  which == 2 ? v : std::uint8_t(0), 255};
    }
    return colors;
}

} // namespace

void setIdentifiers(SpriteDocument& doc, int index, int group, int number)
{
    if (!valid(doc, index))
        return;
    doc.sprites()[index].header.group = std::uint16_t(std::clamp(group, 0, 0xFFFF));
    doc.sprites()[index].header.number = std::uint16_t(std::clamp(number, 0, 0xFFFF));
}

void setAxis(SpriteDocument& doc, int index, int x, int y)
{
    if (!valid(doc, index))
        return;
    doc.sprites()[index].header.axisX = std::int16_t(std::clamp(x, -32768, 32767));
    doc.sprites()[index].header.axisY = std::int16_t(std::clamp(y, -32768, 32767));
}

void moveAxes(SpriteDocument& doc, const QList<int>& indexes, int dx, int dy)
{
    for (int index : validIndexes(doc, indexes)) {
        const Formats::SffSprite& h = doc.sprites()[index].header;
        setAxis(doc, index, h.axisX + dx, h.axisY + dy);
    }
}

void remove(SpriteDocument& doc, QList<int> indexes)
{
    indexes = validIndexes(doc, indexes);
    std::sort(indexes.begin(), indexes.end(), std::greater<>());
    for (int index : std::as_const(indexes))
        doc.sprites().removeAt(index);
}

int removeGroup(SpriteDocument& doc, int group)
{
    const qsizetype before = doc.sprites().size();
    doc.sprites().removeIf([group](const SpriteEntry& entry) { return entry.group() == group; });
    return int(before - doc.sprites().size());
}

int renameGroup(SpriteDocument& doc, int from, int to)
{
    int moved = 0;
    for (SpriteEntry& entry : doc.sprites()) {
        if (entry.group() == from) {
            entry.header.group = std::uint16_t(std::clamp(to, 0, 0xFFFF));
            ++moved;
        }
    }
    return moved;
}

int freeNumber(const SpriteDocument& doc, int group, int start)
{
    for (int n = std::max(start, 0); n <= 0xFFFF; ++n) {
        if (doc.findSprite(group, n) < 0)
            return n;
    }
    for (int n = 0; n < start && n <= 0xFFFF; ++n) {
        if (doc.findSprite(group, n) < 0)
            return n;
    }
    return start;
}

int duplicate(SpriteDocument& doc, int index)
{
    if (!valid(doc, index))
        return -1;
    SpriteEntry copy = doc.sprites()[index];
    copy.header.linkedIndex = -1;
    copy.header.raw = {};
    doc.sprites().insert(index + 1, copy);
    return index + 1;
}

void setImage(SpriteDocument& doc, int index, Image image)
{
    if (!valid(doc, index))
        return;
    SpriteEntry& entry = doc.sprites()[index];
    if (image.isIndexed() && !image.palette().empty() &&
        !(entry.palette && entry.palette->palette.colors.size() == image.palette().size() &&
          sameRgb(entry.palette->palette.colors, image.palette())))
        entry.palette = paletteFor(doc, image.palette(), entry.group(), entry.number());
    entry.pixels = SpriteDocument::makePixels(std::move(image));
    entry.header.linkedIndex = -1;
}

void crop(SpriteDocument& doc, const QList<int>& indexes)
{
    transformPixels(doc, indexes, [](Image& image, QPoint& shift, double&, double&) {
        Rect bounds;
        Image cropped = cropToContent(image, &bounds);
        if (bounds.x == 0 && bounds.y == 0 && cropped.width() == image.width() && cropped.height() == image.height())
            return false;
        image = std::move(cropped);
        shift = QPoint(-bounds.x, -bounds.y);
        return true;
    });
}

void resize(SpriteDocument& doc, const QList<int>& indexes, double widthScale, double heightScale, ResizeFilter filter)
{
    if (widthScale <= 0 || heightScale <= 0 || (widthScale == 1.0 && heightScale == 1.0))
        return;
    transformPixels(doc, indexes, [&](Image& image, QPoint&, double& scaleX, double& scaleY) {
        ResizeOptions options;
        options.filter = filter;
        image = resizeImage(image, scaledLength(image.width(), widthScale), scaledLength(image.height(), heightScale),
                            options);
        scaleX = widthScale;
        scaleY = heightScale;
        return true;
    });
}

void setDecompressOnLoad(SpriteDocument& doc, int index, bool decompress)
{
    if (!valid(doc, index) || decompressesOnLoad(doc, index) == decompress)
        return;
    const SpritePixelsPtr old = doc.sprites()[index].pixels;
    auto changed = std::make_shared<SpritePixels>(*old);
    changed->payload.flags = std::uint16_t(decompress ? (changed->payload.flags | 1u) : (changed->payload.flags & ~1u));
    for (SpriteEntry& entry : doc.sprites()) {
        if (entry.pixels == old)
            entry.pixels = changed;
    }
}

bool decompressesOnLoad(const SpriteDocument& doc, int index)
{
    return valid(doc, index) && (doc.sprites()[index].pixels->payload.flags & 1u) != 0;
}

int splitChannels(SpriteDocument& doc, int index)
{
    if (!valid(doc, index))
        return -1;
    const Image rgba = doc.imageRgba(index);
    if (rgba.pixelBuffer().empty())
        return -1;
    const SpriteEntry original = doc.sprites()[index];
    static const char* const names[] = {" R", " G", " B"};
    for (int which = 0; which < 3; ++which) {
        SpriteEntry entry = original;
        const int number = std::min(original.number() + which, 0xFFFF);
        entry.header.number = std::uint16_t(number);
        entry.header.linkedIndex = -1;
        entry.pixels = SpriteDocument::makePixels(channel(rgba, which));
        entry.name = QString::fromLatin1(names[which]);
        const Palette colors = ramp(which);
        entry.palette = nullptr;
        for (const SpritePalettePtr& p : doc.palettes()) {
            if (p->palette.colors.size() == colors.size() && sameRgb(p->palette.colors, colors)) {
                entry.palette = p;
                break;
            }
        }
        if (!entry.palette) {
            entry.palette = SpriteDocument::makePalette(original.group(), number, colors);
            doc.palettes() << entry.palette;
        }
        if (which == 0) {
            doc.sprites()[index] = entry;
        } else {
            entry.header.raw = {};
            doc.sprites().insert(index + which, entry);
        }
    }
    return index + 2;
}

} // namespace OFS::SpriteEdits
