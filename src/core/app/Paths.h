#pragma once

#include "core/CoreExport.h"

#include <QString>
#include <QStringList>

namespace OFS {

// Data directory: $OFS_DATA_DIR, else <appdir>/data (Windows and the build tree),
// <appdir>/../share/openfactorystudio (Linux) or <appdir>/../Resources/data (macOS).
// User directory: $OFS_CONFIG_DIR, else <GenericDataLocation>/Open Factory Studio.
class OFS_CORE_EXPORT Paths
{
public:
    static QString dataDir();
    static QString userDir();
    static QString engineDataDir(const QString& engine);
    static QString userEngineDir(const QString& engine);

    static QStringList engineSearchDirs();
    static QStringList extensionSearchDirs();

    // Looks in the user directory first, then the data directory.
    static QString locate(const QString& relative);

    static void reset();
};

} // namespace OFS
