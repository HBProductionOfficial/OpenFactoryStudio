#include "core/sprites/SpriteEdits.h"

#include "formats/image/ImageOps.h"
#include "formats/image/Quantize.h"

#include <QCoreApplication>
#include <QHash>
#include <QPoint>

#include <algorithm>
#include <cmath>
#include <tuple>

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

void alignToGuides(SpriteDocument& doc, int index, int horizontal, int vertical, const QPoint& guides)
{
    if (!valid(doc, index))
        return;
    const SpriteEntry& entry = doc.sprites()[index];
    const int x = horizontal == 0 ? 0 : horizontal == 1 ? entry.width() / 2 : entry.width();
    const int y = vertical == 0 ? 0 : vertical == 1 ? entry.height() / 2 : entry.height();
    // The image point (x, y) is at (x - axisX, y - axisY) from the axis.
    setAxis(doc, index, x - guides.x(), y - guides.y());
}

void remove(SpriteDocument& doc, QList<int> indexes)
{
    indexes = validIndexes(doc, indexes);
    std::sort(indexes.begin(), indexes.end(), std::greater<>());
    for (int index : std::as_const(indexes))
        doc.sprites().removeAt(index);
}

namespace {

bool inGroup(const SpriteEntry& entry, int group, int first, int last)
{
    if (entry.group() != group)
        return false;
    return first < 0 || (entry.number() >= first && entry.number() <= last);
}

} // namespace

int removeGroup(SpriteDocument& doc, int group, int first, int last)
{
    const qsizetype before = doc.sprites().size();
    doc.sprites().removeIf([&](const SpriteEntry& entry) { return inGroup(entry, group, first, last); });
    return int(before - doc.sprites().size());
}

int renameGroup(SpriteDocument& doc, int from, int to, int first, int last)
{
    int moved = 0;
    for (SpriteEntry& entry : doc.sprites()) {
        if (inGroup(entry, from, first, last)) {
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

bool hasTrueColor(const SpriteDocument& doc)
{
    for (int i = 0; i < doc.spriteCount(); ++i) {
        const Formats::SffSprite& payload = doc.sprites()[i].pixels->payload;
        if (payload.image ? !payload.image->isIndexed()
                          : !sffIsIndexed(payload.compression) && payload.compression != SffCompression::Invalid)
            return true;
    }
    return false;
}

int convertToIndexed(SpriteDocument& doc)
{
    QHash<const SpritePixels*, SpriteEntry> converted;
    int changed = 0;
    for (int i = 0; i < doc.spriteCount(); ++i) {
        SpriteEntry& entry = doc.sprites()[i];
        const auto done = converted.constFind(entry.pixels.get());
        if (done != converted.cend()) {
            entry.pixels = done->pixels;
            entry.palette = done->palette;
            entry.header.linkedIndex = -1;
            ++changed;
            continue;
        }
        const Image image = doc.image(i);
        if (image.pixelBuffer().empty() || image.isIndexed())
            continue;
        const SpritePixels* old = entry.pixels.get();
        Image indexed = quantize(image);
        entry.palette = paletteFor(doc, indexed.palette(), entry.group(), entry.number());
        entry.pixels = SpriteDocument::makePixels(std::move(indexed));
        entry.header.linkedIndex = -1;
        converted.insert(old, entry);
        ++changed;
    }
    return changed;
}

namespace {

template <typename T>
QList<int> moveInList(QList<T>& list, QList<int> rows, int delta)
{
    const int count = int(list.size());
    rows.removeIf([count](int r) { return r < 0 || r >= count; });
    std::sort(rows.begin(), rows.end());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    QList<int> moved;
    if (delta < 0) {
        int limit = 0;
        for (int r : std::as_const(rows)) {
            const int target = std::max(r + delta, limit);
            list.move(r, target);
            moved << target;
            limit = target + 1;
        }
    } else if (delta > 0) {
        int limit = count - 1;
        for (auto it = rows.crbegin(); it != rows.crend(); ++it) {
            const int target = std::min(*it + delta, limit);
            list.move(*it, target);
            moved.prepend(target);
            limit = target - 1;
        }
    } else {
        moved = rows;
    }
    return moved;
}

} // namespace

QList<int> moveRows(SpriteDocument& doc, QList<int> rows, int delta, bool palettes)
{
    return palettes ? moveInList(doc.palettes(), rows, delta) : moveInList(doc.sprites(), rows, delta);
}

void arrange(SpriteDocument& doc, bool palettes)
{
    if (palettes) {
        std::stable_sort(
            doc.palettes().begin(), doc.palettes().end(), [](const SpritePalettePtr& a, const SpritePalettePtr& b) {
                return std::pair(a->palette.group, a->palette.number) < std::pair(b->palette.group, b->palette.number);
            });
        return;
    }
    QHash<const SpritePalette*, int> blocks;
    for (const SpriteEntry& entry : doc.sprites()) {
        if (!blocks.contains(entry.palette.get()))
            blocks.insert(entry.palette.get(), int(blocks.size()));
    }
    std::stable_sort(doc.sprites().begin(), doc.sprites().end(), [&blocks](const SpriteEntry& a, const SpriteEntry& b) {
        return std::tuple(blocks.value(a.palette.get()), a.group(), a.number()) <
               std::tuple(blocks.value(b.palette.get()), b.group(), b.number());
    });
}

IssueList identifierIssues(const SpriteDocument& doc)
{
    IssueList issues;
    QHash<quint32, int> first;
    for (int i = 0; i < doc.spriteCount(); ++i) {
        const quint32 id = (quint32(doc.sprites()[i].group()) << 16) | quint32(doc.sprites()[i].number());
        const auto found = first.constFind(id);
        if (found == first.cend()) {
            first.insert(id, i);
            continue;
        }
        Issue issue;
        issue.severity = Issue::Severity::Error;
        issue.line = i;
        issue.message =
            QCoreApplication::translate("OFS::SpriteEdits", "The group %1, index %2 is already in use by sprite #%3!")
                .arg(doc.sprites()[i].group())
                .arg(doc.sprites()[i].number())
                .arg(found.value() + 1);
        issues << issue;
    }
    return issues;
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
