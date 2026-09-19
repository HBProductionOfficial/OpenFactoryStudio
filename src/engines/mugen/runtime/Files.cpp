#include "engines/mugen/runtime/Files.h"

#include "core/project/ProjectFile.h"
#include "core/sprites/SpriteDocument.h"
#include "formats/sff/SffDocument.h"

#include <QFile>

namespace OFS::Mugen::Runtime {

QString readText(const QString& path, bool* ok)
{
    QFile file(path);
    const bool opened = file.open(QIODevice::ReadOnly);
    if (ok)
        *ok = opened;
    if (!opened)
        return {};
    QString encoding;
    QByteArray bom;
    return CodeFile::decode(file.readAll(), &encoding, &bom).replace(QLatin1String("\r\n"), QLatin1String("\n"));
}

QByteArray readBytes(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

Formats::ByteSpan span(const QByteArray& data)
{
    return Formats::ByteSpan(reinterpret_cast<const std::byte*>(data.constData()), std::size_t(data.size()));
}

std::shared_ptr<SpriteDocument> readSprites(const QString& path)
{
    const QByteArray data = readBytes(path);
    if (data.isEmpty())
        return nullptr;
    auto sff = Formats::SffDocument::read(span(data));
    if (!sff)
        return nullptr;
    return std::make_shared<SpriteDocument>(SpriteDocument::fromSff(sff.value()));
}

} // namespace OFS::Mugen::Runtime
