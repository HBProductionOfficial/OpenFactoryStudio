#include "core/engine/Engine.h"

#include "core/app/Paths.h"

namespace OFS {

Engine::Engine(QObject* parent)
    : QObject(parent)
{}

Engine::~Engine() = default;

QString Engine::dataDir() const
{
    return Paths::engineDataDir(name());
}

QString Engine::userDataDir() const
{
    return Paths::userEngineDir(name());
}

} // namespace OFS
