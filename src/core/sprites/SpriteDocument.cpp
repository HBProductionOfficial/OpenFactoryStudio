#include "core/sprites/SpriteDocument.h"

#include <QHash>

namespace OFS {

using namespace Formats;

namespace {

SffSprite payloadOf(const SffSprite& sprite)
{
    SffSprite payload = sprite;
    payload.linkedIndex = -1;
    return payload;
}

// The name stored in SFF v1 comment bytes: up to the first zero byte.
QString nameFromComment(const std::array<std::uint8_t, 13>& bytes)
{
    QByteArray text;
    for (std::uint8_t b : bytes) {
        if (b == 0)
            break;
        text += char(b);
    }
    return QString::fromLatin1(text);
}

SffSprite headerOf(const SffSprite& sprite)
{
    SffSprite header = sprite;
    header.data.clear();
    header.image.reset();
    return header;
}

} // namespace

SpriteDocument SpriteDocument::fromSff(const SffDocument& sff)
{
    SpriteDocument doc;
    doc.m_base = sff;
    doc.m_base.sprites.clear();
    doc.m_base.palettes.clear();

    for (const SffPalette& p : sff.palettes) {
        auto palette = std::make_shared<SpritePalette>();
        palette->palette = p;
        // Files before SFF v2.01 store no alpha, so the first color is transparent and the others
        // opaque. Same as Fighter Factory.
        if (sff.version != SffVersion::V201) {
            Formats::Palette& colors = palette->palette.colors;
            palette->fileAlpha.reserve(colors.size());
            for (std::size_t c = 0; c < colors.size(); ++c) {
                palette->fileAlpha.push_back(colors[c].a);
                colors[c].a = c == 0 ? 0 : 255;
            }
        }
        doc.m_palettes << palette;
    }
    for (qsizetype i = 0; i < doc.m_palettes.size(); ++i) {
        const int link = sff.palettes[std::size_t(i)].linkedIndex;
        if (link >= 0 && link < doc.m_palettes.size() && link != i)
            std::const_pointer_cast<SpritePalette>(doc.m_palettes[i])->link = doc.m_palettes[link];
    }

    std::vector<SpritePixelsPtr> pixels(sff.sprites.size());
    for (std::size_t i = 0; i < sff.sprites.size(); ++i) {
        const SffSprite& s = sff.sprites[i];
        if (!s.isLinked() || s.image)
            pixels[i] = std::make_shared<SpritePixels>(SpritePixels{payloadOf(s)});
    }
    for (std::size_t i = 0; i < sff.sprites.size(); ++i) {
        const SffSprite& s = sff.sprites[i];
        SpriteEntry entry;
        entry.header = headerOf(s);
        if (sff.version == SffVersion::V1)
            entry.name = nameFromComment(s.raw.v1Reserved);
        entry.pixels = pixels[sff.resolveLink(i)];
        if (!entry.pixels) // a broken link: keep the sprite, without pixels
            entry.pixels = std::make_shared<SpritePixels>(SpritePixels{payloadOf(s)});
        if (s.paletteIndex >= 0 && std::size_t(s.paletteIndex) < sff.palettes.size())
            entry.palette = doc.m_palettes[s.paletteIndex];
        doc.m_sprites << entry;
    }
    return doc;
}

SffDocument SpriteDocument::toSff() const
{
    SffDocument sff = m_base;
    sff.sprites.clear();
    sff.palettes.clear();

    QList<SpritePalettePtr> palettes = m_palettes;
    for (const SpriteEntry& entry : m_sprites) {
        if (entry.palette && !palettes.contains(entry.palette))
            palettes << entry.palette;
    }
    QHash<const SpritePalette*, int> paletteIndex;
    for (qsizetype i = 0; i < palettes.size(); ++i) {
        SffPalette p = palettes[i]->palette;
        // An unchanged palette keeps the file's bytes in the versions without alpha.
        if (sff.version != SffVersion::V201 && palettes[i]->fileAlpha.size() == p.colors.size()) {
            for (std::size_t c = 0; c < p.colors.size(); ++c)
                p.colors[c].a = palettes[i]->fileAlpha[c];
        }
        p.linkedIndex = -1;
        // A link is kept while its target comes first (the writer drops it if the colors differ).
        if (const SpritePalettePtr target = palettes[i]->link.lock())
            p.linkedIndex = paletteIndex.value(target.get(), -1);
        paletteIndex.insert(palettes[i].get(), int(i));
        sff.palettes.push_back(std::move(p));
    }

    QHash<const SpritePixels*, int> owner;
    for (qsizetype i = 0; i < m_sprites.size(); ++i) {
        const SpriteEntry& entry = m_sprites[i];
        SffSprite s = entry.header;
        const SffSprite& payload = entry.pixels->payload;
        const auto first = owner.constFind(entry.pixels.get());
        if (first == owner.cend()) {
            owner.insert(entry.pixels.get(), int(i));
            s.width = payload.width;
            s.height = payload.height;
            s.compression = payload.compression;
            s.colorDepth = payload.colorDepth;
            s.flags = payload.flags;
            s.data = payload.data;
            s.image = payload.image;
            s.linkedIndex = -1;
            if (entry.header.isLinked()) // was a link in the file: its stored fields are not the data's
                s.raw = payload.raw;
        } else {
            if (!entry.header.isLinked()) {
                s.width = payload.width;
                s.height = payload.height;
                s.compression = payload.compression;
                s.colorDepth = payload.colorDepth;
            }
            s.data.clear();
            s.image.reset();
            s.linkedIndex = first.value();
        }
        s.paletteIndex = entry.palette ? paletteIndex.value(entry.palette.get(), 0) : 0;
        if (nameFromComment(s.raw.v1Reserved) != entry.name) {
            s.raw.v1Reserved.fill(0);
            const QByteArray text = entry.name.toLatin1().left(13);
            std::copy(text.cbegin(), text.cend(), s.raw.v1Reserved.begin());
        }
        sff.sprites.push_back(std::move(s));
    }
    return sff;
}

int SpriteDocument::findSprite(int group, int number) const
{
    for (qsizetype i = 0; i < m_sprites.size(); ++i) {
        if (m_sprites[i].group() == group && m_sprites[i].number() == number)
            return int(i);
    }
    return -1;
}

int SpriteDocument::findPalette(int group, int number) const
{
    for (qsizetype i = 0; i < m_palettes.size(); ++i) {
        if (m_palettes[i]->palette.group == group && m_palettes[i]->palette.number == number)
            return int(i);
    }
    return -1;
}

QList<int> SpriteDocument::linkedSprites(int index) const
{
    QList<int> linked;
    if (index < 0 || index >= m_sprites.size())
        return linked;
    const SpritePixels* pixels = m_sprites[index].pixels.get();
    for (qsizetype i = 0; i < m_sprites.size(); ++i) {
        if (m_sprites[i].pixels.get() == pixels)
            linked << int(i);
    }
    return linked;
}

Image SpriteDocument::image(int index, const Palette* palette) const
{
    if (index < 0 || index >= m_sprites.size())
        return {};
    const SpriteEntry& entry = m_sprites[index];
    if (!palette && entry.palette)
        palette = &entry.palette->palette.colors;
    auto decoded = SffDocument::decodePayload(entry.pixels->payload, palette);
    return decoded ? std::move(decoded.value()) : Image();
}

Image SpriteDocument::imageRgba(int index, const Palette* palette, bool showTransparent) const
{
    Image rgba = image(index, palette).toRgba(!showTransparent);
    if (showTransparent && rgba.isRgba()) {
        std::vector<std::uint8_t>& pixels = rgba.pixelBuffer();
        for (std::size_t i = 3; i < pixels.size(); i += 4)
            pixels[i] = 255;
    }
    return rgba;
}

SpritePixelsPtr SpriteDocument::makePixels(Image image)
{
    SffSprite payload;
    payload.width = std::uint16_t(std::clamp(image.width(), 0, 0xFFFF));
    payload.height = std::uint16_t(std::clamp(image.height(), 0, 0xFFFF));
    bool opaque = true;
    if (image.isRgba()) {
        const std::vector<std::uint8_t>& pixels = image.pixelBuffer();
        for (std::size_t i = 3; i < pixels.size() && opaque; i += 4)
            opaque = pixels[i] == 255;
    }
    payload.colorDepth = image.isIndexed() ? 8 : opaque ? 24 : 32;
    // Invalid lets the writer choose the indexed compression from the colors used.
    payload.compression = image.isIndexed() ? SffCompression::Invalid
                          : opaque          ? SffCompression::Png24
                                            : SffCompression::Png32;
    payload.image = std::move(image);
    return std::make_shared<SpritePixels>(SpritePixels{std::move(payload)});
}

SpritePalettePtr SpriteDocument::makePalette(int group, int number, Palette colors)
{
    auto palette = std::make_shared<SpritePalette>();
    palette->palette.group = std::uint16_t(group);
    palette->palette.number = std::uint16_t(number);
    palette->palette.colors = std::move(colors);
    return palette;
}

} // namespace OFS
