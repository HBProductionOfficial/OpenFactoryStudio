#pragma once

#include "core/CoreExport.h"

#include <QString>

#include <functional>

class QPainter;
class QWidget;

namespace OFS {

class Config;

// Renderer of the graphic workspaces (Options > Graphics > Renderer, read once at startup).
namespace Rendering {

enum class Backend
{
    Native,
    OpenGL,
};

// Picks the backend from the settings. If the last OpenGL start crashed before its first frame,
// this start uses Native.
OFS_CORE_EXPORT void choose(const Config& config, const QString& userDir);
// Call on a normal exit, so the next start knows OpenGL did not crash.
OFS_CORE_EXPORT void finish();
OFS_CORE_EXPORT Backend backend();
OFS_CORE_EXPORT QString backendName();

// With OpenGL, covers `owner` with a surface that draws through `paint`. Returns nullptr with Native.
OFS_CORE_EXPORT QWidget* attachSurface(QWidget* owner, std::function<void(QPainter&)> paint);
OFS_CORE_EXPORT void update(QWidget* surface);

} // namespace Rendering

} // namespace OFS
