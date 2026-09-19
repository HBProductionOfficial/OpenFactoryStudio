#pragma once

#include "core/CoreExport.h"

#include <QBrush>
#include <QColor>

namespace OFS {

// Set the painter's brush origin to the board's top-left and scale with QBrush::setTransform.
OFS_CORE_EXPORT QBrush checkerboard(int cell, const QColor& first, const QColor& second);

} // namespace OFS
