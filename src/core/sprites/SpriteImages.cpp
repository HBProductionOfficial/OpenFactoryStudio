#include "core/sprites/SpriteImages.h"

#include "formats/image/ImageFile.h"
#include "formats/image/ImageOps.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QRegularExpression>

namespace OFS {

using namespace Formats;

Image loadImageFile(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return {};
    }
    const QByteArray data = file.readAll();
    const ByteSpan bytes(reinterpret_cast<const std::byte*>(data.constData()), std::size_t(data.size()));
    if (imageFileType(bytes) != ImageFileType::Unknown) {
        auto image = decodeImageFile(bytes);
        if (image)
            return std::move(image.value());
        if (error)
            *error = QString::fromStdString(image.error().message);
        return {};
    }
    QImage qt;
    if (!qt.loadFromData(data)) {
        if (error)
            *error = QCoreApplication::translate("OFS::SpriteImages", "%1 is not an image that can be read.")
                         .arg(QFileInfo(path).fileName());
        return {};
    }
    qt = qt.convertToFormat(QImage::Format_RGBA8888);
    std::vector<std::uint8_t> pixels(std::size_t(qt.width()) * std::size_t(qt.height()) * 4);
    for (int y = 0; y < qt.height(); ++y)
        std::memcpy(pixels.data() + std::size_t(y) * std::size_t(qt.width()) * 4, qt.constScanLine(y),
                    std::size_t(qt.width()) * 4);
    return Image::rgba(qt.width(), qt.height(), std::move(pixels));
}

bool identifiersFromFileName(const QString& fileName, int* group, int* index)
{
    static const QRegularExpression pattern(QStringLiteral("(\\d+)\\s*[_,\\-]\\s*(\\d+)$"));
    const QRegularExpressionMatch m = pattern.match(QFileInfo(fileName).completeBaseName());
    if (!m.hasMatch())
        return false;
    bool okGroup = false;
    bool okIndex = false;
    const int g = m.captured(1).toInt(&okGroup);
    const int i = m.captured(2).toInt(&okIndex);
    if (!okGroup || !okIndex || g > 0xFFFF || i > 0xFFFF)
        return false;
    *group = g;
    *index = i;
    return true;
}

namespace {

QPoint anchorPoint(int anchor, int width, int height)
{
    const int column = anchor % 3;
    const int row = anchor / 3;
    return {column == 0 ? 0 : column == 1 ? width / 2 : width, row == 0 ? 0 : row == 1 ? height / 2 : height};
}

} // namespace

QList<int> addSprites(SpriteDocument& doc, int after, const QList<ImageToAdd>& images, const AddSpritesOptions& options)
{
    QList<int> added;
    const int target = doc.findPalette(options.paletteGroup, options.paletteIndex);
    const SpritePalettePtr chosen = target >= 0 ? doc.palettes()[target] : nullptr;
    const bool trueColor = doc.version() == SffVersion::V201;
    int nextIndex = options.index;
    int position = std::clamp(after + 1, 0, doc.spriteCount());

    for (const ImageToAdd& item : images) {
        Image image = item.image;
        if (image.pixelBuffer().empty())
            continue;
        SpriteEntry entry;
        int group = options.group;
        int index = nextIndex;
        if (!(options.detectFromFileName && identifiersFromFileName(item.fileName, &group, &index)))
            nextIndex += options.indexStep;

        Rect bounds{0, 0, image.width(), image.height()};
        if (options.crop == AddSpritesOptions::Crop::BeforeAxis)
            image = cropToContent(image, &bounds);
        QPoint axis = options.axisAnchor >= 0 && options.axisAnchor < 9
                          ? anchorPoint(options.axisAnchor, image.width(), image.height())
                          : QPoint(options.axisX, options.axisY);
        if (options.crop == AddSpritesOptions::Crop::AfterAxis) {
            image = cropToContent(image, &bounds);
            axis -= QPoint(bounds.x, bounds.y);
        }

        if (options.palette == AddSpritesOptions::Palette::Force && chosen) {
            image = adaptToPalette(image, chosen->palette.colors);
            entry.palette = chosen;
        } else if (options.palette == AddSpritesOptions::Palette::Exchange && chosen && image.isIndexed()) {
            entry.palette = chosen;
        } else if (image.isIndexed() && !image.palette().empty()) {
            entry.palette = nullptr;
            for (const SpritePalettePtr& p : doc.palettes()) {
                if (p->palette.colors.size() == image.palette().size() && sameRgb(p->palette.colors, image.palette())) {
                    entry.palette = p;
                    break;
                }
            }
            if (!entry.palette) {
                int number = index;
                while (doc.findPalette(group, number) >= 0 && number < 0xFFFF)
                    ++number;
                entry.palette = SpriteDocument::makePalette(group, number, image.palette());
                doc.palettes() << entry.palette;
            }
        } else if (!image.isIndexed() && !trueColor) {
            // Versions without true color: the nearest colors of the first palette.
            const Palette colors = doc.palettes().isEmpty() ? grayscalePalette(256) : doc.palettes()[0]->palette.colors;
            image = adaptToPalette(image, colors);
            entry.palette = doc.palettes().isEmpty() ? nullptr : doc.palettes()[0];
        }

        entry.header.group = std::uint16_t(std::clamp(group, 0, 0xFFFF));
        entry.header.number = std::uint16_t(std::clamp(index, 0, 0xFFFF));
        entry.header.axisX = std::int16_t(std::clamp(axis.x(), -32768, 32767));
        entry.header.axisY = std::int16_t(std::clamp(axis.y(), -32768, 32767));
        entry.pixels = SpriteDocument::makePixels(std::move(image));
        doc.sprites().insert(position, entry);
        added << position;
        ++position;
    }
    return added;
}

} // namespace OFS
