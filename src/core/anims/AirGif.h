#pragma once

#include "core/CoreExport.h"
#include "core/anims/AirDocument.h"

#include <QByteArray>
#include <QList>

namespace OFS {

class SpriteDocument;

// Animations > Save as animated GIF. Loops for ever, or plays once and holds an endless frame.
OFS_CORE_EXPORT QByteArray animationGif(const AirAnimation& animation, const SpriteDocument& sprites, double scale,
                                        double velocity, QString* error = nullptr);

// Rounded down so no time is lost. An endless frame lasts 65535 and ends the list.
OFS_CORE_EXPORT QList<int> gifDelays(const QList<int>& times, double velocity);

} // namespace OFS
