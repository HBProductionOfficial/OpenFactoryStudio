#include "core/palettes/PaletteOps.h"

#include "formats/image/Quantize.h"

#include <QColor>
#include <QCoreApplication>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <map>

namespace OFS::PaletteOps {

using namespace Formats;

namespace {

std::uint8_t clampByte(int value)
{
    return std::uint8_t(std::clamp(value, 0, 255));
}

QList<int> validSlots(const Palette& palette, QList<int> slots)
{
    std::sort(slots.begin(), slots.end());
    slots.erase(std::unique(slots.begin(), slots.end()), slots.end());
    slots.removeIf([&palette](int slot) { return slot < 0 || slot >= int(palette.size()); });
    return slots;
}

quint32 packed(Rgba c)
{
    return (quint32(c.r) << 24) | (quint32(c.g) << 16) | (quint32(c.b) << 8) | c.a;
}

bool isEmpty(const Palette& palette, int slot)
{
    return slot > 0 && palette[std::size_t(slot)].a == 0;
}

// Pixels as they show: visible colors opaque, transparent ones with alpha 0.
Rgba normalized(Rgba c)
{
    c.a = c.a < 128 ? 0 : 255;
    return c;
}

} // namespace

QStringList effectNames()
{
    static const char* const names[] = {
        QT_TRANSLATE_NOOP("OFS::PaletteOps", "Blue to Red"),  QT_TRANSLATE_NOOP("OFS::PaletteOps", "Blue to Green"),
        QT_TRANSLATE_NOOP("OFS::PaletteOps", "Green to Red"), QT_TRANSLATE_NOOP("OFS::PaletteOps", "Green to Blue"),
        QT_TRANSLATE_NOOP("OFS::PaletteOps", "Red to Green"), QT_TRANSLATE_NOOP("OFS::PaletteOps", "Red to Blue"),
        QT_TRANSLATE_NOOP("OFS::PaletteOps", "Lighter"),      QT_TRANSLATE_NOOP("OFS::PaletteOps", "Darker"),
        QT_TRANSLATE_NOOP("OFS::PaletteOps", "Monochrome"),   QT_TRANSLATE_NOOP("OFS::PaletteOps", "Fire"),
        QT_TRANSLATE_NOOP("OFS::PaletteOps", "Sapphire"),     QT_TRANSLATE_NOOP("OFS::PaletteOps", "Stone")};
    QStringList list;
    for (const char* name : names)
        list << QCoreApplication::translate("OFS::PaletteOps", name);
    return list;
}

Rgba effect(Effect which, Rgba c)
{
    const int gray = (int(c.r) + int(c.g) + int(c.b)) / 3;
    switch (which) {
    case Effect::BlueToRed:
    case Effect::RedToBlue:
        std::swap(c.r, c.b);
        break;
    case Effect::BlueToGreen:
    case Effect::GreenToBlue:
        std::swap(c.g, c.b);
        break;
    case Effect::GreenToRed:
    case Effect::RedToGreen:
        std::swap(c.r, c.g);
        break;
    case Effect::Lighter:
        c = Rgba{clampByte(c.r + 32), clampByte(c.g + 32), clampByte(c.b + 32), c.a};
        break;
    case Effect::Darker:
        c = Rgba{clampByte(c.r - 32), clampByte(c.g - 32), clampByte(c.b - 32), c.a};
        break;
    case Effect::Monochrome:
        c = Rgba{std::uint8_t(gray), std::uint8_t(gray), std::uint8_t(gray), c.a};
        break;
    case Effect::Fire:
        c = Rgba{clampByte(gray + 100), std::uint8_t(gray / 2), 0, c.a};
        break;
    case Effect::Sapphire:
        c = Rgba{std::uint8_t(60 + gray / 4), 0, std::uint8_t(60 + gray / 2), c.a};
        break;
    case Effect::Stone: {
        const std::uint8_t stone = std::uint8_t(std::min(gray + 50, 205));
        c = Rgba{stone, stone, stone, c.a};
        break;
    }
    }
    return c;
}

void applyEffect(Palette& palette, const QList<int>& slots, Effect which)
{
    for (int slot : validSlots(palette, slots))
        palette[std::size_t(slot)] = effect(which, palette[std::size_t(slot)]);
}

void adjustHueSaturation(Palette& palette, const QList<int>& slots, int hue, int saturation)
{
    for (int slot : validSlots(palette, slots)) {
        Rgba& c = palette[std::size_t(slot)];
        float h = 0;
        float s = 0;
        float l = 0;
        QColor(c.r, c.g, c.b).getHslF(&h, &s, &l);
        if (h < 0)
            h = 0;
        double degrees = std::fmod(double(h) * 360.0 + hue, 360.0);
        if (degrees < 0)
            degrees += 360.0;
        const double sat = std::clamp(double(s) + saturation / 255.0, 0.0, 1.0);
        const QColor result = QColor::fromHslF(float(degrees / 360.0), float(sat), l);
        c = Rgba{std::uint8_t(result.red()), std::uint8_t(result.green()), std::uint8_t(result.blue()), c.a};
    }
}

void adjustRgb(Palette& palette, const QList<int>& slots, int red, int green, int blue)
{
    for (int slot : validSlots(palette, slots)) {
        Rgba& c = palette[std::size_t(slot)];
        c = Rgba{clampByte(c.r + red), clampByte(c.g + green), clampByte(c.b + blue), c.a};
    }
}

void gradient(Palette& palette, QList<int> slots, Rgba start, Rgba end)
{
    slots = validSlots(palette, slots);
    const int n = int(slots.size());
    for (int k = 0; k < n; ++k) {
        auto channel = [k, n](std::uint8_t from, std::uint8_t to) {
            if (n == 1)
                return from;
            return std::uint8_t(int(from) + (int(to) - int(from)) * k / (n - 1));
        };
        palette[std::size_t(slots[k])] =
            Rgba{channel(start.r, end.r), channel(start.g, end.g), channel(start.b, end.b), channel(start.a, end.a)};
    }
}

void reverseSlots(Palette& palette, QList<int> slots)
{
    slots = validSlots(palette, slots);
    for (qsizetype i = 0, j = slots.size() - 1; i < j; ++i, --j)
        std::swap(palette[std::size_t(slots[i])], palette[std::size_t(slots[j])]);
}

void invert(Palette& palette)
{
    for (Rgba& c : palette)
        c = Rgba{std::uint8_t(255 - c.r), std::uint8_t(255 - c.g), std::uint8_t(255 - c.b), c.a};
}

void remapImage(Image& image, const SlotMap& map)
{
    if (!image.isIndexed())
        return;
    for (std::uint8_t& p : image.pixels()) {
        if (p < map.size())
            p = std::uint8_t(map[p]);
    }
}

SlotMap sortByTone(Palette& palette)
{
    const int count = int(palette.size());
    SlotMap map(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
        map[std::size_t(i)] = i;
    if (count < 3)
        return map;
    struct Entry
    {
        int slot;
        int group; // 0 grays, 1 colors, 2 empty slots
        float hue;
        float lightness;
    };
    std::vector<Entry> entries;
    for (int i = 1; i < count; ++i) {
        const Rgba c = palette[std::size_t(i)];
        float h = 0;
        float s = 0;
        float l = 0;
        QColor(c.r, c.g, c.b).getHslF(&h, &s, &l);
        const int group = c.a == 0 ? 2 : (h < 0 || s < 0.05f ? 0 : 1);
        entries.push_back(Entry{i, group, h < 0 ? 0.0f : h, l});
    }
    std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        if (a.group != b.group)
            return a.group < b.group;
        if (a.group == 1 && a.hue != b.hue)
            return a.hue < b.hue;
        return a.group != 2 && a.lightness < b.lightness;
    });
    const Palette old = palette;
    for (int i = 0; i < int(entries.size()); ++i) {
        palette[std::size_t(i + 1)] = old[std::size_t(entries[std::size_t(i)].slot)];
        map[std::size_t(entries[std::size_t(i)].slot)] = i + 1;
    }
    return map;
}

SlotMap removeDuplicates(Palette& palette, Rgba base)
{
    const int count = int(palette.size());
    SlotMap map(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
        map[std::size_t(i)] = i;
    QSet<quint32> taken;
    for (const Rgba& c : palette)
        taken.insert(packed(c));
    std::map<quint32, int> last;
    for (int i = 1; i < count; ++i) {
        if (!isEmpty(palette, i))
            last[packed(palette[std::size_t(i)])] = i;
    }
    auto next = [&taken](Rgba c) {
        quint32 rgb = (quint32(c.r) << 16) | (quint32(c.g) << 8) | c.b;
        for (;;) {
            const Rgba candidate{std::uint8_t(rgb >> 16), std::uint8_t(rgb >> 8), std::uint8_t(rgb), 255};
            if (!taken.contains(packed(candidate)))
                return candidate;
            rgb = (rgb + 1) & 0xFFFFFF;
        }
    };
    Rgba from{base.r, base.g, base.b, 255};
    for (int i = 1; i < count; ++i) {
        if (isEmpty(palette, i))
            continue;
        const int kept = last[packed(palette[std::size_t(i)])];
        if (kept == i)
            continue;
        map[std::size_t(i)] = kept;
        const Rgba unique = next(from);
        taken.insert(packed(unique));
        palette[std::size_t(i)] = unique;
        from = unique;
    }
    return map;
}

int addDifferent(Palette& palette, const Palette& other)
{
    QSet<quint32> have;
    for (int i = 0; i < int(palette.size()); ++i) {
        if (!isEmpty(palette, i))
            have.insert(packed(palette[std::size_t(i)]) | 0xFF);
    }
    int added = 0;
    int slot = 1;
    for (int i = 1; i < int(other.size()); ++i) {
        const Rgba c = other[std::size_t(i)];
        if (c.a == 0 || have.contains(packed(c) | 0xFF))
            continue;
        while (slot < int(palette.size()) && !isEmpty(palette, slot))
            ++slot;
        if (slot >= int(palette.size()))
            break;
        palette[std::size_t(slot)] = Rgba{c.r, c.g, c.b, 255};
        have.insert(packed(c) | 0xFF);
        ++added;
    }
    return added;
}

std::vector<Rgba> usedColors(const Image& image)
{
    std::vector<Rgba> colors;
    QSet<quint32> seen;
    auto add = [&](Rgba c) {
        if (!seen.contains(packed(c))) {
            seen.insert(packed(c));
            colors.push_back(c);
        }
    };
    if (image.isIndexed()) {
        const Palette& palette = image.palette();
        std::vector<bool> used(256, false);
        for (std::uint8_t p : image.pixels())
            used[p] = true;
        for (int i = 0; i < 256; ++i) {
            if (!used[std::size_t(i)])
                continue;
            Rgba c = i < int(palette.size()) ? palette[std::size_t(i)] : Rgba{0, 0, 0, 255};
            if (i == 0)
                c.a = 0;
            add(normalized(c));
        }
    } else {
        const auto pixels = image.pixels();
        for (std::size_t i = 0; i + 3 < pixels.size(); i += 4)
            add(normalized(Rgba{pixels[i], pixels[i + 1], pixels[i + 2], pixels[i + 3]}));
    }
    return colors;
}

Palette paletteFromImages(const QList<Image>& images)
{
    std::vector<Rgba> colors;
    QSet<quint32> seen;
    for (const Image& image : images) {
        for (const Rgba& c : usedColors(image)) {
            if (!seen.contains(packed(c))) {
                seen.insert(packed(c));
                colors.push_back(c);
            }
        }
    }
    if (colors.size() > 256) {
        std::vector<std::uint8_t> data;
        for (const Rgba& c : colors)
            data.insert(data.end(), {c.r, c.g, c.b, c.a});
        const Image reduced = quantize(Image::rgba(int(colors.size()), 1, std::move(data)));
        colors = reduced.palette();
        colors.resize(std::min<std::size_t>(colors.size(), 256));
    }
    std::sort(colors.begin(), colors.end(), [](Rgba a, Rgba b) {
        return (packed(a) >> 8) != (packed(b) >> 8) ? (packed(a) >> 8) < (packed(b) >> 8) : a.a < b.a;
    });
    colors.resize(256, Rgba{0, 0, 0, 0});
    return colors;
}

void optimize(Image& image, Palette& palette)
{
    if (!image.isIndexed()) {
        image = quantize(image);
        palette = image.palette();
        palette.resize(256, Rgba{0, 0, 0, 0});
        image.setPalette(palette);
        return;
    }
    std::vector<bool> used(256, false);
    for (std::uint8_t p : image.pixels())
        used[p] = true;
    Palette result;
    SlotMap map(256);
    for (int i = 0; i < 256; ++i)
        map[std::size_t(i)] = i;
    if (used[0] || (!palette.empty() && palette[0].a == 0)) {
        result.push_back(palette.empty() ? Rgba{0, 0, 0, 0} : palette[0]);
        map[0] = 0;
    }
    for (int i = 1; i < 256; ++i) {
        if (!used[std::size_t(i)])
            continue;
        const Rgba c = i < int(palette.size()) ? palette[std::size_t(i)] : Rgba{0, 0, 0, 255};
        const auto same = std::find(result.begin() + (result.empty() ? 0 : 1), result.end(), c);
        if (same != result.end()) {
            map[std::size_t(i)] = int(same - result.begin());
        } else {
            map[std::size_t(i)] = int(result.size());
            result.push_back(c);
        }
    }
    remapImage(image, map);
    result.resize(256, Rgba{0, 0, 0, 0});
    palette = result;
    image.setPalette(palette);
}

Image adapt(const Image& reference, const Palette& palette)
{
    const Image rgba = reference.isIndexed() ? reference.toRgba(true) : reference;
    int transparent = 0;
    for (int i = 0; i < int(palette.size()); ++i) {
        if (palette[std::size_t(i)].a == 0) {
            transparent = i;
            break;
        }
    }
    std::vector<int> visible;
    for (int i = 0; i < int(palette.size()); ++i) {
        if (palette[std::size_t(i)].a != 0 && i != transparent)
            visible.push_back(i);
    }
    std::vector<std::uint8_t> indexes(std::size_t(rgba.width()) * std::size_t(rgba.height()),
                                      std::uint8_t(transparent));
    const auto pixels = rgba.pixels();
    std::map<quint32, int> cache;
    for (std::size_t i = 0; i < indexes.size(); ++i) {
        const Rgba c{pixels[i * 4], pixels[i * 4 + 1], pixels[i * 4 + 2], pixels[i * 4 + 3]};
        if (c.a < 128 || visible.empty())
            continue;
        const auto found = cache.find(packed(c));
        if (found != cache.end()) {
            indexes[i] = std::uint8_t(found->second);
            continue;
        }
        int best = visible.front();
        long bestDistance = -1;
        for (int slot : visible) {
            const Rgba p = palette[std::size_t(slot)];
            const long dr = long(c.r) - p.r;
            const long dg = long(c.g) - p.g;
            const long db = long(c.b) - p.b;
            const long distance = dr * dr + dg * dg + db * db;
            if (bestDistance < 0 || distance < bestDistance) {
                best = slot;
                bestDistance = distance;
            }
        }
        cache[packed(c)] = best;
        indexes[i] = std::uint8_t(best);
    }
    return Image::indexed(rgba.width(), rgba.height(), std::move(indexes), palette);
}

void grayscale(Image& image, Palette& palette)
{
    const Image reference = image.isIndexed() ? image.toRgba(true) : image;
    std::vector<int> grays;
    for (const Rgba& c : usedColors(reference)) {
        if (c.a != 0)
            grays.push_back((299 * c.r + 587 * c.g + 114 * c.b) / 1000);
    }
    std::sort(grays.begin(), grays.end());
    grays.erase(std::unique(grays.begin(), grays.end()), grays.end());
    grays.resize(std::min<std::size_t>(grays.size(), 255));
    Palette result;
    Rgba transparent = palette.empty() ? Rgba{0, 0, 0, 0} : palette[0];
    transparent.a = 0;
    result.push_back(transparent);
    for (int g : grays)
        result.push_back(Rgba{std::uint8_t(g), std::uint8_t(g), std::uint8_t(g), 255});
    result.resize(256, Rgba{0, 0, 0, 0});
    palette = result;
    image = adapt(reference, palette);
}

} // namespace OFS::PaletteOps
