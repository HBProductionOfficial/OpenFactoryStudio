#pragma once

#include "core/engine/Engine.h"

namespace OFS::Mugen {

class MugenPlugin : public QObject, public EnginePlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OFS_ENGINE_PLUGIN_IID FILE "mugen.json")
    Q_INTERFACES(OFS::EnginePlugin)

public:
    Engine* createEngine(QObject* parent) override;
};

} // namespace OFS::Mugen
