#pragma once

#include <QString>

namespace OFS {

// Ids of the editors with their own pages. An engine may add others.
namespace EditorId {
inline constexpr QLatin1StringView Definitions{"definitions"};
inline constexpr QLatin1StringView Sprites{"sprites"};
inline constexpr QLatin1StringView Animations{"animations"};
inline constexpr QLatin1StringView Backgrounds{"backgrounds"};
inline constexpr QLatin1StringView Commands{"commands"};
inline constexpr QLatin1StringView States{"states"};
inline constexpr QLatin1StringView Sounds{"sounds"};
inline constexpr QLatin1StringView Emulator{"emulator"};
} // namespace EditorId

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
    // File actions are <prefix>_new, _open, _save, _saveas and _saveas_utf8 ("st_new").
    QString actionPrefix;
    QString fileFilter;
    QString defaultSuffix;

    QString scope() const { return QStringLiteral("editor:") + id; }
};

} // namespace OFS
