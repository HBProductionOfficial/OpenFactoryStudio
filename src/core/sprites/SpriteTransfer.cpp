#include "core/sprites/SpriteTransfer.h"

#include "core/sprites/SpriteEdits.h"
#include "formats/image/ImageOps.h"

#include "core/sprites/SpriteEdits.h"
#include "core/sprites/SpriteExport.h"
#include "formats/palette/PaletteFormats.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSaveFile>

namespace OFS {

using namespace Formats;

namespace {

struct SpriteTransferTexts
{
    Q_DECLARE_TR_FUNCTIONS(OFS::SpriteTransferTexts)
};

QString idFile(int group, int number, const char* suffix)
{
    return QStringLiteral("%1-%2.%3").arg(group).arg(number).arg(QLatin1String(suffix));
}

bool writeFile(const QString& path, const QByteArray& data, QString* error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

} // namespace

bool exportSprites(const SpriteDocument& doc, const QList<int>& sprites, const QString& listPath,
                   SpriteListFormat format, QString* error)
{
    const QDir dir = QFileInfo(listPath).dir();
    QHash<const SpritePixels*, QString> imageFiles;
    QList<SpritePalettePtr> palettes;
    for (int index : sprites) {
        if (index < 0 || index >= doc.spriteCount())
            continue;
        const SpriteEntry& entry = doc.sprites()[index];
        if (!imageFiles.contains(entry.pixels.get())) {
            const QString name = idFile(entry.group(), entry.number(), "png");
            if (!writeImageFile(dir.filePath(name), doc.image(index), error))
                return false;
            imageFiles.insert(entry.pixels.get(), name);
        }
        if (entry.palette && !palettes.contains(entry.palette))
            palettes << entry.palette;
    }
    std::stable_sort(palettes.begin(), palettes.end(), [&doc](const SpritePalettePtr& a, const SpritePalettePtr& b) {
        return doc.palettes().indexOf(a) < doc.palettes().indexOf(b);
    });
    for (const SpritePalettePtr& palette : std::as_const(palettes)) {
        Palette colors = palette->palette.colors;
        colors.resize(256, Rgba{0, 0, 0, 255});
        const Bytes act = writeAct(colors, ActOrder::Reversed);
        if (!writeFile(dir.filePath(idFile(palette->palette.group, palette->palette.number, "act")),
                       QByteArray(reinterpret_cast<const char*>(act.data()), qsizetype(act.size())), error))
            return false;
    }

    QStringList lines;
    if (format == SpriteListFormat::Ffe) {
        lines << QStringLiteral(
                     ";0 always import the palette, 1 import only if all colors doesn't match to any existing palette")
              << QStringLiteral("noDuplicatePal = 0");
        for (const SpritePalettePtr& palette : std::as_const(palettes)) {
            const SffPalette& p = palette->palette;
            lines << QString() << QStringLiteral("[PaletteDef]") << QStringLiteral("group = %1").arg(p.group)
                  << QStringLiteral("number = %1").arg(p.number)
                  << QStringLiteral("file = %1").arg(idFile(p.group, p.number, "act"));
        }
        for (int index : sprites) {
            if (index < 0 || index >= doc.spriteCount())
                continue;
            const SpriteEntry& entry = doc.sprites()[index];
            lines << QString() << QStringLiteral("[SpriteDef]") << QStringLiteral("group = %1").arg(entry.group())
                  << QStringLiteral("image = %1").arg(entry.number())
                  << QStringLiteral("xaxis = %1").arg(entry.header.axisX)
                  << QStringLiteral("yaxis = %1").arg(entry.header.axisY)
                  << QStringLiteral("flags = %1").arg(SpriteEdits::decompressesOnLoad(doc, index) ? 1 : 0);
            if (entry.palette)
                lines << QStringLiteral("palette = %1,%2")
                             .arg(entry.palette->palette.group)
                             .arg(entry.palette->palette.number);
            lines << QStringLiteral("file = %1").arg(imageFiles.value(entry.pixels.get()));
        }
        return writeFile(listPath, (lines.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8(), error);
    }

    lines << QStringLiteral("[Output]")
          << QStringLiteral(";filename = %1.sff").arg(QFileInfo(listPath).completeBaseName()) << QString()
          << QStringLiteral("[Option]") << QStringLiteral("sprite.compress.5 = lz5")
          << QStringLiteral("sprite.compress.8 = png8") << QStringLiteral("sprite.compress.24 = png24")
          << QStringLiteral("sprite.compress.32 = png32") << QStringLiteral("sprite.decompressonload = 0")
          << QStringLiteral("pal.reversepng = 0") << QString() << QStringLiteral("[Pal]");
    for (const SpritePalettePtr& palette : std::as_const(palettes)) {
        const SffPalette& p = palette->palette;
        lines << QStringLiteral("%1,%2, %3, 0,%4")
                     .arg(p.group)
                     .arg(p.number)
                     .arg(idFile(p.group, p.number, "act"))
                     .arg(p.colors.size() <= 32 ? 31 : 255);
    }
    lines << QString() << QStringLiteral("[Sprite]");
    for (int index : sprites) {
        if (index < 0 || index >= doc.spriteCount())
            continue;
        const SpriteEntry& entry = doc.sprites()[index];
        QString line = QStringLiteral("%1,%2, %3, %4,%5")
                           .arg(entry.group())
                           .arg(entry.number())
                           .arg(imageFiles.value(entry.pixels.get()))
                           .arg(entry.header.axisX)
                           .arg(entry.header.axisY);
        if (entry.palette)
            line += QStringLiteral(" ? usepal = %1,%2")
                        .arg(entry.palette->palette.group)
                        .arg(entry.palette->palette.number);
        lines << line;
    }
    return writeFile(listPath, (lines.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n")).toUtf8(), error);
}

QList<int> importFfe(SpriteDocument& doc, int after, const QString& path, const AddSpritesOptions& options,
                     QString* error)
{
    auto fail = [error](const QString& message) {
        if (error)
            *error = message;
        return QList<int>();
    };
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(file.errorString());
    const QDir dir = QFileInfo(path).dir();
    const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));

    // Read the list first: nothing is added when it is malformed.
    struct Section
    {
        QString kind;
        QHash<QString, QString> values;
        int line = 0;
    };
    QList<Section> sections;
    bool noDuplicatePalettes = false;
    for (int n = 0; n < lines.size(); ++n) {
        QString line = lines[n];
        if (const qsizetype comment = line.indexOf(QLatin1Char(';')); comment >= 0)
            line.truncate(comment);
        line = line.trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith(QLatin1Char('['))) {
            if (!line.endsWith(QLatin1Char(']')))
                return fail(SpriteTransferTexts::tr("%1, line %2: the section name is not closed.")
                                .arg(QFileInfo(path).fileName())
                                .arg(n + 1));
            sections << Section{line.mid(1, line.size() - 2).trimmed().toLower(), {}, n + 1};
            continue;
        }
        const qsizetype equals = line.indexOf(QLatin1Char('='));
        if (equals <= 0)
            return fail(SpriteTransferTexts::tr("%1, line %2: expected 'name = value'.")
                            .arg(QFileInfo(path).fileName())
                            .arg(n + 1));
        const QString key = line.left(equals).trimmed().toLower();
        const QString value = line.mid(equals + 1).trimmed();
        if (sections.isEmpty()) {
            if (key == QLatin1String("noduplicatepal"))
                noDuplicatePalettes = value == QLatin1String("1");
            continue;
        }
        sections.last().values.insert(key, value);
    }

    SpriteDocument edited = doc;
    QHash<QString, SpritePalettePtr> palettes;
    for (const Section& section : std::as_const(sections)) {
        if (section.kind != QLatin1String("palettedef"))
            continue;
        QFile act(dir.filePath(section.values.value(QStringLiteral("file"))));
        if (!act.open(QIODevice::ReadOnly))
            return fail(SpriteTransferTexts::tr("%1, line %2: %3")
                            .arg(QFileInfo(path).fileName())
                            .arg(section.line)
                            .arg(act.errorString()));
        const QByteArray data = act.readAll();
        auto colors = readAct(ByteSpan(reinterpret_cast<const std::byte*>(data.constData()), std::size_t(data.size())),
                              ActOrder::Reversed);
        if (!colors)
            return fail(SpriteTransferTexts::tr("%1, line %2: %3")
                            .arg(QFileInfo(path).fileName())
                            .arg(section.line)
                            .arg(QString::fromStdString(colors.error().message)));
        const int group = section.values.value(QStringLiteral("group")).toInt();
        const int number = section.values.value(QStringLiteral("number")).toInt();
        SpritePalettePtr palette;
        if (noDuplicatePalettes) {
            for (const SpritePalettePtr& p : edited.palettes()) {
                if (sameRgb(p->palette.colors, colors.value()))
                    palette = p;
            }
        }
        if (!palette) {
            palette = SpriteDocument::makePalette(group, number, colors.value());
            edited.palettes() << palette;
        }
        palettes.insert(QStringLiteral("%1,%2").arg(group).arg(number), palette);
    }

    QHash<QString, SpritePixelsPtr> images;
    QList<int> added;
    int position = std::clamp(after + 1, 0, edited.spriteCount());
    std::optional<AddSpritesOptions::Conflict> forAll;
    for (const Section& section : std::as_const(sections)) {
        if (section.kind != QLatin1String("spritedef"))
            continue;
        const QString imageFile = section.values.value(QStringLiteral("file"));
        SpritePixelsPtr pixels = images.value(imageFile);
        if (!pixels) {
            QString loadError;
            Image image = loadImageFile(dir.filePath(imageFile), &loadError);
            if (image.pixelBuffer().empty())
                return fail(SpriteTransferTexts::tr("%1, line %2: %3")
                                .arg(QFileInfo(path).fileName())
                                .arg(section.line)
                                .arg(loadError));
            pixels = SpriteDocument::makePixels(std::move(image));
            images.insert(imageFile, pixels);
        }
        SpriteEntry entry;
        int group = section.values.value(QStringLiteral("group")).toInt();
        int index = section.values.value(QStringLiteral("image")).toInt();
        if (edited.findSprite(group, index) >= 0) {
            AddSpritesOptions::Conflict answer = forAll                ? *forAll
                                                 : options.askConflict ? options.askConflict(group, index)
                                                                       : AddSpritesOptions::Conflict::NextFree;
            if (answer == AddSpritesOptions::Conflict::NextFreeForAll ||
                answer == AddSpritesOptions::Conflict::AsIsForAll)
                forAll = answer;
            if (answer == AddSpritesOptions::Conflict::Skip)
                continue;
            if (answer == AddSpritesOptions::Conflict::NextFree ||
                answer == AddSpritesOptions::Conflict::NextFreeForAll)
                index = SpriteEdits::freeNumber(edited, group, index);
        }
        entry.header.group = std::uint16_t(std::clamp(group, 0, 0xFFFF));
        entry.header.number = std::uint16_t(std::clamp(index, 0, 0xFFFF));
        entry.header.axisX = std::int16_t(section.values.value(QStringLiteral("xaxis")).toInt());
        entry.header.axisY = std::int16_t(section.values.value(QStringLiteral("yaxis")).toInt());
        entry.pixels = pixels;
        const QString paletteId = section.values.value(QStringLiteral("palette")).remove(QLatin1Char(' '));
        entry.palette = palettes.value(paletteId);
        if (!entry.palette && !paletteId.isEmpty()) {
            const int existing = edited.findPalette(paletteId.section(QLatin1Char(','), 0, 0).toInt(),
                                                    paletteId.section(QLatin1Char(','), 1, 1).toInt());
            if (existing >= 0)
                entry.palette = edited.palettes()[existing];
        }
        edited.sprites().insert(position, entry);
        if (section.values.value(QStringLiteral("flags")) == QLatin1String("1"))
            SpriteEdits::setDecompressOnLoad(edited, position, true);
        added << position++;
    }
    doc = edited;
    return added;
}
ProjectImportResult importFromProject(SpriteDocument& doc, int after, const SpriteDocument& source,
                                      const QList<ProjectImportItem>& items, const ProjectImportOptions& options)
{
    using Options = ProjectImportOptions;
    ProjectImportResult result;
    const SpritePalettePtr chosen = options.paletteIndex >= 0 && options.paletteIndex < int(doc.palettes().size())
                                        ? doc.palettes()[options.paletteIndex]
                                        : nullptr;
    int position = std::clamp(after + 1, 0, doc.spriteCount());
    for (const ProjectImportItem& item : items) {
        if (item.source < 0 || item.source >= source.spriteCount())
            continue;
        const SpriteEntry& from = source.sprites()[item.source];
        Image image = source.image(item.source);
        if (image.pixelBuffer().empty())
            continue;
        if (image.isIndexed() && chosen && options.palette != Options::Palette::ImageOwn) {
            if (options.palette == Options::Palette::Force)
                image = adaptToPalette(image, chosen->palette.colors);
            image.setPalette(chosen->palette.colors);
        }
        const int group = std::clamp(item.group, 0, 0xFFFF);
        const int number = std::clamp(item.number, 0, 0xFFFF);
        const int existing = doc.findSprite(group, number);
        if (existing >= 0 && options.conflict != Options::Conflict::AsIs) {
            const bool replace =
                options.conflict == Options::Conflict::Replace ||
                (options.conflict == Options::Conflict::Ask && options.askReplace && options.askReplace(group, number));
            if (!replace)
                continue;
            SpriteEdits::setImage(doc, existing, std::move(image));
            SpriteEdits::setAxis(doc, existing, from.header.axisX, from.header.axisY);
            ++result.replaced;
            continue;
        }
        SpriteEntry entry;
        entry.header.group = std::uint16_t(group);
        entry.header.number = std::uint16_t(number);
        entry.header.axisX = from.header.axisX;
        entry.header.axisY = from.header.axisY;
        entry.name = from.name;
        doc.sprites().insert(position, entry);
        SpriteEdits::setImage(doc, position, std::move(image));
        // Positions of the sprites added before stay right: they come first.
        result.added << position++;
    }
    return result;
}

} // namespace OFS
