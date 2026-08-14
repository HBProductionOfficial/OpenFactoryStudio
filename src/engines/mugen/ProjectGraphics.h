#pragma once

#include "engines/mugen/AirFile.h"

#include <QHash>
#include <QImage>
#include <QPoint>

#include <memory>

namespace OFS {
class Project;
}

namespace OFS::Formats {
class SffDocument;
}

namespace OFS::Mugen {

QString projectFileText(const Project& project, const QString& path);

// What the tools need to draw a project's animations: its AIR, SFF and coordinate space.
class ProjectGraphics
{
public:
    explicit ProjectGraphics(const Project& project);
    ~ProjectGraphics();

    const Project& project() const { return m_project; }
    const QList<AirAction>& animations() const { return m_animations; }
    // [Info] localcoord width, 320 when not given.
    int localWidth() const { return m_localWidth; }

    QImage sprite(int group, int index, QPoint* axis = nullptr) const;

private:
    const Project& m_project;
    QList<AirAction> m_animations;
    int m_localWidth = 320;
    std::unique_ptr<Formats::SffDocument> m_sprites;
    struct Cached
    {
        QImage image;
        QPoint axis;
    };
    mutable QHash<quint32, Cached> m_cache;
};

} // namespace OFS::Mugen
