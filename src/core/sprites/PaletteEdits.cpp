#include "core/sprites/PaletteEdits.h"

#include "formats/palette/PaletteFormats.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSaveFile>
#include <QSet>

#include <algorithm>

namespace OFS::PaletteEdits {

using namespace Formats;

namespace {

bool valid(const SpriteDocument& doc, int index)
{
    return index >= 0 && index < doc.palettes().size();
}

void repoint(SpriteDocument& doc, const QHash<const SpritePalette*, SpritePalettePtr>& map)
{
    for (SpriteEntry& entry : doc.sprites()) {
        const auto found = map.constFind(entry.palette.get());
        if (entry.palette && found != map.cend())
            entry.palette = found.value();
    }
}

template <typename Change>
void modify(SpriteDocument& doc, int index, Change change)
{
    if (!valid(doc, index))
        return;
    const SpritePalettePtr old = doc.palettes()[index];
    auto copy = std::make_shared<SpritePalette>(*old);
    change(*copy);
    doc.palettes()[index] = copy;
    repoint(doc, {{old.get(), copy}});
}

struct PaletteEditsTexts
{
    Q_DECLARE_TR_FUNCTIONS(OFS::PaletteEdits::PaletteEditsTexts)
};

} // namespace

bool isIndexed(const SpriteEntry& entry)
{
    if (!entry.pixels)
        return false;
    const SffSprite& payload = entry.pixels->payload;
    if (payload.image)
        return payload.image->isIndexed();
    return payload.compression != SffCompression::Png24 && payload.compression != SffCompression::Png32;
}

QList<int> users(const SpriteDocument& doc, int index)
{
    QList<int> sprites;
    if (!valid(doc, index))
        return sprites;
    const SpritePalette* palette = doc.palettes()[index].get();
    for (int i = 0; i < doc.spriteCount(); ++i) {
        if (doc.sprites()[i].palette.get() == palette)
            sprites << i;
    }
    return sprites;
}

int paletteOf(const SpriteDocument& doc, int sprite)
{
    if (sprite < 0 || sprite >= doc.spriteCount() || !doc.sprites()[sprite].palette)
        return -1;
    return int(doc.palettes().indexOf(doc.sprites()[sprite].palette));
}

void setColors(SpriteDocument& doc, int index, Palette colors)
{
    modify(doc, index, [&colors](SpritePalette& palette) {
        palette.palette.colors = std::move(colors);
        palette.hidden.clear();
    });
}

void setIdentifiers(SpriteDocument& doc, int index, int group, int number)
{
    modify(doc, index, [group, number](SpritePalette& palette) {
        palette.palette.group = std::uint16_t(std::clamp(group, 0, 0xFFFF));
        palette.palette.number = std::uint16_t(std::clamp(number, 0, 0xFFFF));
    });
}

int insert(SpriteDocument& doc, int after, int group, int number, Palette colors)
{
    const int position = std::clamp(after + 1, 0, int(doc.palettes().size()));
    doc.palettes().insert(position, SpriteDocument::makePalette(group, number, std::move(colors)));
    return position;
}

Palette emptyColors()
{
    return Palette(32, Rgba{0, 0, 0, 0});
}

void remove(SpriteDocument& doc, int index, int replacement)
{
    if (!valid(doc, index))
        return;
    const SpritePalettePtr old = doc.palettes()[index];
    const SpritePalettePtr target =
        replacement != index && valid(doc, replacement) ? doc.palettes()[replacement] : SpritePalettePtr();
    doc.palettes().removeAt(index);
    repoint(doc, {{old.get(), target}});
}

int removeDuplicates(SpriteDocument& doc)
{
    QHash<const SpritePalette*, SpritePalettePtr> map;
    QList<SpritePalettePtr> kept;
    for (const SpritePalettePtr& palette : std::as_const(doc.palettes())) {
        const auto same = std::find_if(kept.cbegin(), kept.cend(), [&palette](const SpritePalettePtr& earlier) {
            return earlier->palette.colors == palette->palette.colors;
        });
        if (same != kept.cend())
            map.insert(palette.get(), *same);
        else
            kept << palette;
    }
    const int removed = int(doc.palettes().size() - kept.size());
    doc.palettes() = kept;
    repoint(doc, map);
    return removed;
}

int removeUnused(SpriteDocument& doc)
{
    QSet<const SpritePalette*> used;
    for (const SpriteEntry& entry : doc.sprites())
        used.insert(entry.palette.get());
    const qsizetype before = doc.palettes().size();
    doc.palettes().removeIf([&used](const SpritePalettePtr& palette) {
        return palette->palette.group != 1 && !used.contains(palette.get());
    });
    return int(before - doc.palettes().size());
}

void clear(SpriteDocument& doc)
{
    QHash<const SpritePalette*, SpritePalettePtr> map;
    for (const SpritePalettePtr& palette : std::as_const(doc.palettes()))
        map.insert(palette.get(), SpritePalettePtr());
    doc.palettes().clear();
    repoint(doc, map);
}

void switchCount(SpriteDocument& doc, int index)
{
    modify(doc, index, [](SpritePalette& palette) {
        Palette& colors = palette.palette.colors;
        if (colors.size() > 32) {
            palette.hidden.assign(colors.begin() + 32, colors.end());
            colors.resize(32);
        } else {
            colors.resize(32, Rgba{0, 0, 0, 255});
            colors.insert(colors.end(), palette.hidden.begin(), palette.hidden.end());
            colors.resize(256, Rgba{0, 0, 0, 255});
            palette.hidden.clear();
        }
    });
}

void moveColor(SpriteDocument& doc, int index, int from, int to)
{
    modify(doc, index, [from, to](SpritePalette& palette) {
        Palette& colors = palette.palette.colors;
        const int count = int(colors.size());
        if (from < 0 || to < 0 || from >= count || to >= count || from == to)
            return;
        const Rgba color = colors[std::size_t(from)];
        colors.erase(colors.begin() + from);
        colors.insert(colors.begin() + to, color);
    });
}

void exchange(SpriteDocument& doc, int index, QList<int> slots)
{
    std::sort(slots.begin(), slots.end());
    slots.erase(std::unique(slots.begin(), slots.end()), slots.end());
    modify(doc, index, [&slots](SpritePalette& palette) {
        Palette& colors = palette.palette.colors;
        slots.removeIf([&colors](int slot) { return slot < 0 || slot >= int(colors.size()); });
        for (qsizetype i = 0, j = slots.size() - 1; i < j; ++i, --j)
            std::swap(colors[std::size_t(slots[i])], colors[std::size_t(slots[j])]);
    });
}

void fill(SpriteDocument& doc, int index, const QList<int>& slots, Rgba color)
{
    modify(doc, index, [&slots, color](SpritePalette& palette) {
        Palette& colors = palette.palette.colors;
        for (int slot : slots) {
            if (slot >= 0 && slot < int(colors.size()))
                colors[std::size_t(slot)] = color;
        }
    });
}

void applyToSprites(SpriteDocument& doc, int index, const QList<int>& sprites)
{
    if (!valid(doc, index))
        return;
    for (int sprite : sprites) {
        if (sprite >= 0 && sprite < doc.spriteCount())
            doc.sprites()[sprite].palette = doc.palettes()[index];
    }
}

QList<int> spritesWithoutPalette(const SpriteDocument& doc)
{
    QList<int> sprites;
    for (int i = 0; i < doc.spriteCount(); ++i) {
        const SpriteEntry& entry = doc.sprites()[i];
        if (!entry.palette && isIndexed(entry))
            sprites << i;
    }
    return sprites;
}

int addMissingPalette(SpriteDocument& doc)
{
    const QList<int> sprites = spritesWithoutPalette(doc);
    if (sprites.isEmpty())
        return -1;
    Palette gray(256);
    for (int i = 0; i < 256; ++i)
        gray[std::size_t(i)] = Rgba{std::uint8_t(i), std::uint8_t(i), std::uint8_t(i), 255};
    int number = 1;
    for (bool taken = true; taken; ++number) {
        taken = false;
        for (const SpritePalettePtr& palette : doc.palettes())
            taken = taken || (palette->palette.group == 1 && palette->palette.number == number);
        if (!taken)
            break;
    }
    const int index = insert(doc, int(doc.palettes().size()) - 1, 1, number, std::move(gray));
    applyToSprites(doc, index, sprites);
    return index;
}

IssueList issues(const SpriteDocument& doc)
{
    IssueList issues;
    QHash<quint32, int> first;
    for (int i = 0; i < doc.palettes().size(); ++i) {
        const SffPalette& palette = doc.palettes()[i]->palette;
        const quint32 id = (quint32(palette.group) << 16) | quint32(palette.number);
        const auto found = first.constFind(id);
        if (found == first.cend()) {
            first.insert(id, i);
            continue;
        }
        Issue issue;
        issue.severity = Issue::Severity::Error;
        issue.line = i;
        issue.message = PaletteEditsTexts::tr("The group %1, index %2 is already in use by palette #%3!")
                            .arg(palette.group)
                            .arg(palette.number)
                            .arg(found.value() + 1);
        issues << issue;
    }
    for (int i = 0; i < doc.spriteCount(); ++i) {
        const SpriteEntry& entry = doc.sprites()[i];
        if (entry.palette || !isIndexed(entry))
            continue;
        Issue issue;
        issue.severity = Issue::Severity::Error;
        issue.line = i;
        issue.message = PaletteEditsTexts::tr("Indexed sprite without palette!");
        issues << issue;
    }
    return issues;
}

Result<Palette> readFile(const QString& path, bool reversed)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return Error{file.errorString().toStdString()};
    const QByteArray bytes = file.readAll();
    const ByteSpan data(reinterpret_cast<const std::byte*>(bytes.constData()), std::size_t(bytes.size()));
    auto read = readPaletteFile(data, reversed ? ActOrder::Natural : ActOrder::Reversed);
    if (!read)
        return read.error();
    Palette colors = std::move(read.value());
    if (colors.empty())
        return Error{"the file has no colors"};
    colors.resize(colors.size() <= 32 ? 32 : 256, Rgba{0, 0, 0, 255});
    colors[0].a = 0;
    return colors;
}

QStringList saveAct(const SpriteDocument& doc, const QList<int>& palettes, const QString& path, QString* error)
{
    QStringList written;
    const QFileInfo info(path);
    for (int index : palettes) {
        if (!valid(doc, index))
            continue;
        const SffPalette& palette = doc.palettes()[index]->palette;
        QString target = path;
        if (palettes.size() > 1) {
            target = info.dir().filePath(
                QStringLiteral("%1_%2-%3.act").arg(info.completeBaseName()).arg(palette.group).arg(palette.number));
        } else if (info.suffix().isEmpty()) {
            target += QStringLiteral(".act");
        }
        const Bytes bytes = writeAct(palette.colors, ActOrder::Reversed);
        QSaveFile out(target);
        if (!out.open(QIODevice::WriteOnly) ||
            out.write(reinterpret_cast<const char*>(bytes.data()), qint64(bytes.size())) != qint64(bytes.size()) ||
            !out.commit()) {
            if (error)
                *error = out.errorString();
            return written;
        }
        written << target;
    }
    return written;
}

} // namespace OFS::PaletteEdits
