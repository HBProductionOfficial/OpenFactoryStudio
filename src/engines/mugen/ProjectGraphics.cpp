#include "engines/mugen/ProjectGraphics.h"

#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "engines/mugen/DefFile.h"
#include "formats/sff/SffDocument.h"

#include <QFile>
#include <QTextDocument>

namespace OFS::Mugen {

QString projectFileText(const Project& project, const QString& path)
{
    if (const auto* file = qobject_cast<const CodeFile*>(project.file(path)))
        return file->document()->toPlainText();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QString encoding;
    QByteArray bom;
    return CodeFile::decode(file.readAll(), &encoding, &bom);
}

ProjectGraphics::ProjectGraphics(const Project& project)
    : m_project(project)
{
    reloadAnimations();

    DefFile def;
    def.parse(projectFileText(project, project.filePath()));
    const int width = def.value(QStringLiteral("Info"), QStringLiteral("localcoord"))
                          .section(QLatin1Char(','), 0, 0)
                          .trimmed()
                          .toInt();
    if (width > 0)
        m_localWidth = width;

    QFile sff(project.editorFiles(QStringLiteral("sprites")).value(0));
    if (!sff.fileName().isEmpty() && sff.open(QIODevice::ReadOnly)) {
        const QByteArray data = sff.readAll();
        auto document = Formats::SffDocument::read(Formats::ByteSpan(
            reinterpret_cast<const std::byte*>(data.constData()), static_cast<std::size_t>(data.size())));
        if (document)
            m_sprites = std::make_unique<Formats::SffDocument>(std::move(document.value()));
    }
}

ProjectGraphics::~ProjectGraphics() = default;

QString ProjectGraphics::animationFile() const
{
    return m_project.editorFiles(QStringLiteral("animations")).value(0);
}

void ProjectGraphics::reloadAnimations()
{
    const QString air = animationFile();
    m_animations = air.isEmpty() ? QList<AirAction>() : readAnimations(projectFileText(m_project, air));
}

QImage ProjectGraphics::sprite(int group, int index, QPoint* axis) const
{
    if (!m_sprites || group < 0 || index < 0 || group > 0xFFFF || index > 0xFFFF)
        return {};
    const quint32 key = (quint32(group) << 16) | quint32(index);
    auto it = m_cache.find(key);
    if (it == m_cache.end()) {
        Cached cached;
        if (const auto found = m_sprites->findSprite(std::uint16_t(group), std::uint16_t(index))) {
            const Formats::SffSprite& info = m_sprites->sprites.at(*found);
            if (const auto image = m_sprites->decodeSpriteRgba(*found)) {
                const Formats::Image& rgba = image.value();
                cached.image =
                    QImage(rgba.pixels().data(), rgba.width(), rgba.height(), rgba.width() * 4, QImage::Format_RGBA8888)
                        .copy();
                cached.axis = QPoint(info.axisX, info.axisY);
            }
        }
        it = m_cache.insert(key, cached);
    }
    if (axis)
        *axis = it->axis;
    return it->image;
}

} // namespace OFS::Mugen
