#include "core/sprites/SpriteExport.h"

#include "formats/codecs/Bmp.h"
#include "formats/codecs/Gif.h"
#include "formats/codecs/Pcx.h"
#include "formats/codecs/Png.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>

#include <climits>

namespace OFS {

using namespace Formats;

namespace {

struct SpriteExportTexts
{
    Q_DECLARE_TR_FUNCTIONS(OFS::SpriteExportTexts)
};

Result<Bytes> encode(const QString& suffix, const Image& image)
{
    const QString s = suffix.toLower();
    if (s == QLatin1String("png")) {
        PngEncodeOptions options;
        options.transparentIndex0 = image.isIndexed();
        return encodePng(image, options);
    }
    if (s == QLatin1String("bmp"))
        return encodeBmp(image);
    if (!image.isIndexed())
        return Error{"only indexed images can be saved in this format"};
    if (s == QLatin1String("pcx"))
        return encodePcx(image);
    if (s == QLatin1String("gif")) {
        GifDocument gif;
        gif.width = image.width();
        gif.height = image.height();
        GifFrame frame;
        frame.image = image;
        frame.transparentIndex = 0;
        gif.globalPalette = image.palette();
        gif.frames.push_back(std::move(frame));
        return writeGif(gif);
    }
    return Error{"unknown image format"};
}

QString numberedPath(const QString& path, const QString& suffix, const SpriteEntry& entry)
{
    const QFileInfo info(path);
    return info.dir().filePath(
        QStringLiteral("%1_%2-%3.%4").arg(info.completeBaseName()).arg(entry.group()).arg(entry.number()).arg(suffix));
}

QString withSuffix(const QString& path, const QString& suffix)
{
    return QFileInfo(path).suffix().isEmpty() ? path + QLatin1Char('.') + suffix : path;
}

} // namespace

bool writeImageFile(const QString& path, const Image& image, QString* error)
{
    auto bytes = encode(QFileInfo(path).suffix(), image);
    if (!bytes) {
        if (error)
            *error = SpriteExportTexts::tr("%1: %2").arg(QFileInfo(path).fileName(),
                                                         QString::fromStdString(bytes.error().message));
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(reinterpret_cast<const char*>(bytes.value().data()), qint64(bytes.value().size())) !=
            qint64(bytes.value().size()) ||
        !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

QStringList saveSpriteImages(const SpriteDocument& doc, const QList<int>& sprites, const QString& path,
                             const QString& suffix, QString* error)
{
    QStringList written;
    for (int index : sprites) {
        if (index < 0 || index >= doc.spriteCount())
            continue;
        const QString target =
            sprites.size() == 1 ? withSuffix(path, suffix) : numberedPath(path, suffix, doc.sprites()[index]);
        if (!writeImageFile(target, doc.image(index), error))
            return written;
        written << target;
    }
    return written;
}

AlignedImages alignSprites(const SpriteDocument& doc, const QList<int>& sprites)
{
    AlignedImages result;
    int left = INT_MAX;
    int top = INT_MAX;
    int right = INT_MIN;
    int bottom = INT_MIN;
    for (int index : sprites) {
        if (index < 0 || index >= doc.spriteCount())
            continue;
        const SpriteEntry& entry = doc.sprites()[index];
        left = std::min(left, -int(entry.header.axisX));
        top = std::min(top, -int(entry.header.axisY));
        right = std::max(right, entry.width() - entry.header.axisX);
        bottom = std::max(bottom, entry.height() - entry.header.axisY);
    }
    if (left > right)
        return result;
    result.axis = QPoint(-left, -top);
    const int width = right - left;
    const int height = bottom - top;
    for (int index : sprites) {
        if (index < 0 || index >= doc.spriteCount())
            continue;
        const SpriteEntry& entry = doc.sprites()[index];
        const Image image = doc.image(index);
        const int x0 = result.axis.x() - entry.header.axisX;
        const int y0 = result.axis.y() - entry.header.axisY;
        Image canvas = image.isIndexed()
                           ? Image::indexed(width, height, std::vector<std::uint8_t>(std::size_t(width) * height, 0),
                                            image.palette())
                           : Image::rgba(width, height, std::vector<std::uint8_t>(std::size_t(width) * height * 4, 0));
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (image.isIndexed())
                    canvas.setIndex(x0 + x, y0 + y, image.indexAt(x, y));
                else
                    canvas.setColor(x0 + x, y0 + y, image.colorAt(x, y));
            }
        }
        result.images << canvas;
    }
    return result;
}

QStringList saveAlignedImages(const SpriteDocument& doc, const QList<int>& sprites, const AlignedImages& aligned,
                              const QString& path, const QString& suffix, QString* error)
{
    QStringList written;
    int n = 0;
    for (int index : sprites) {
        if (index < 0 || index >= doc.spriteCount() || n >= aligned.images.size())
            continue;
        const QString target =
            sprites.size() == 1 ? withSuffix(path, suffix) : numberedPath(path, suffix, doc.sprites()[index]);
        if (!writeImageFile(target, aligned.images[n++], error))
            return written;
        written << target;
    }
    return written;
}

QByteArray axisFileText(const QPoint& axis)
{
    return QStringLiteral("X axis: %1\r\nY axis: %2").arg(axis.x()).arg(axis.y()).toLatin1();
}

} // namespace OFS
