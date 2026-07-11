#include "core/engine/Engine.h"

#include "core/app/Paths.h"

#include <QFileInfo>

namespace OFS {

Engine::Engine(QObject* parent)
    : QObject(parent)
{}

Engine::~Engine() = default;

void Engine::initialize(AppContext&) {}

EditorInfo Engine::editor(const QString& id) const
{
    for (const EditorInfo& e : editors()) {
        if (e.id == id)
            return e;
    }
    return {};
}

QString Engine::projectFileFilter() const
{
    QStringList patterns;
    for (const QString& ext : projectExtensions())
        patterns << QStringLiteral("*.") + ext;
    return QStringLiteral("%1 (%2)").arg(displayName(), patterns.join(QLatin1Char(' ')));
}

bool Engine::canOpen(const QString& filePath) const
{
    return projectExtensions().contains(QFileInfo(filePath).suffix(), Qt::CaseInsensitive);
}

QString Engine::syntaxFileType(const Project&, const QString&) const
{
    return {};
}

IssueList Engine::validate(const Project&, const QString&, const QString&) const
{
    return {};
}

QString Engine::dataDir() const
{
    return Paths::engineDataDir(name());
}

QString Engine::userDataDir() const
{
    return Paths::userEngineDir(name());
}

} // namespace OFS
