#pragma once

#include <QString>

namespace OFS {

// Picks the preset's workspace toolbar ("code", "graphics", "sound") and the status bar layout.
enum class WorkspaceType
{
    Code,
    Graphics,
    Sound,
    Emulator,
};

struct EditorInfo
{
    QString id; // also the preset panel and toolbar name
    QString title;
    QString switchAction; // e.g. "editor_def"
    WorkspaceType workspace = WorkspaceType::Code;
    QString extraPanel; // default right panel, empty for none

    QString scope() const { return QStringLiteral("editor:") + id; }
};

} // namespace OFS
