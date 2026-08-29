#pragma once

#include "core/CoreExport.h"
#include "formats/sff/SffDocument.h"

#include <QList>
#include <QString>

#include <memory>

namespace OFS {

// Shared by linked sprites. Never changed once made, so edits make a new one.
struct SpritePixels
{
    Formats::SffSprite payload;
};

struct SpritePalette
{
    Formats::SffPalette palette;
    // The palette whose file data this one shares (SFF v2 linked palettes), if any.
    std::weak_ptr<const SpritePalette> link;
    // Colors past 32 kept after Switch color count, so switching back restores them.
    Formats::Palette hidden;
};

using SpritePixelsPtr = std::shared_ptr<const SpritePixels>;
using SpritePalettePtr = std::shared_ptr<const SpritePalette>;

struct SpriteEntry
{
    Formats::SffSprite header;
    SpritePixelsPtr pixels;
    SpritePalettePtr palette;
    // The 13 comment bytes of SFF v1 sprites, kept in memory for other versions.
    QString name;

    int group() const { return header.group; }
    int number() const { return header.number; }
    int width() const { return pixels ? pixels->payload.width : 0; }
    int height() const { return pixels ? pixels->payload.height : 0; }
};

// Sprites point straight at their pixels and palette, so the file's links and palette numbers are
// worked out again on write. Copies share pixels and palettes, which keeps undo snapshots small.
class OFS_CORE_EXPORT SpriteDocument
{
public:
    SpriteDocument() = default;

    static SpriteDocument fromSff(const Formats::SffDocument& sff);
    // Sprites sharing pixels become links to the first of them. Unchanged sprites keep their
    // stored bytes.
    Formats::SffDocument toSff() const;

    Formats::SffVersion version() const { return m_base.version; }
    const QList<SpriteEntry>& sprites() const { return m_sprites; }
    QList<SpriteEntry>& sprites() { return m_sprites; }
    const QList<SpritePalettePtr>& palettes() const { return m_palettes; }
    QList<SpritePalettePtr>& palettes() { return m_palettes; }

    int spriteCount() const { return int(m_sprites.size()); }
    int findSprite(int group, int number) const;
    int findPalette(int group, int number) const;
    QList<int> linkedSprites(int index) const;

    Formats::Image image(int index, const Formats::Palette* palette = nullptr) const;
    Formats::Image imageRgba(int index, const Formats::Palette* palette = nullptr, bool showTransparent = false) const;

    // Encoded when the file is written. Indexed images use LZ5 under 32 colors, else RLE8.
    // True-color images use PNG24 when opaque, else PNG32.
    static SpritePixelsPtr makePixels(Formats::Image image);
    static SpritePalettePtr makePalette(int group, int number, Formats::Palette colors);

private:
    Formats::SffDocument m_base;
    QList<SpriteEntry> m_sprites;
    QList<SpritePalettePtr> m_palettes;
};

} // namespace OFS
