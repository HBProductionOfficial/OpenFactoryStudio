#include "engines/mugen/MugenPlugin.h"

#include "engines/mugen/MugenEngine.h"

namespace OFS::Mugen {

Engine* MugenPlugin::createEngine(QObject* parent)
{
    return new MugenEngine(parent);
}

} // namespace OFS::Mugen
