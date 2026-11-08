#pragma once

#include "core/CoreExport.h"

#include <QString>
#include <QStringList>

namespace OFS {

// File names written in game files come from Windows: backslashes and any letter case.

// Turns backslashes into slashes and trims spaces.
OFS_CORE_EXPORT QString cleanContentPath(const QString& name);

// The path as it is on disk. Where it doesn't exist as written, each part is looked up ignoring
// letter case, which matters on Linux and macOS. Returns the path unchanged when nothing matches.
OFS_CORE_EXPORT QString findContentFile(const QString& path);

// True for a name that can lead to another computer: a network path (\\server\share\...) or a
// Windows shortcut (.lnk), which can point to one. Game files have no reason to name either, and
// opening one makes Windows sign in to that computer and send it the user's password hash.
OFS_CORE_EXPORT bool isUnsafeName(const QString& name);

// True for a name with a colon on Windows. There it names a drive or a hidden stream of a file,
// so the name isn't a plain path inside a folder.
OFS_CORE_EXPORT bool namesDriveOrStream(const QString& name);

// True for the folder itself and anything in it, with the letter case rules of the system.
OFS_CORE_EXPORT bool isInsideFolder(const QString& path, const QString& folder);

// The file a game file names, from the first folder that has it, or an empty string. Unsafe names
// are refused.
OFS_CORE_EXPORT QString findInFolders(const QString& name, const QStringList& folders);

} // namespace OFS
