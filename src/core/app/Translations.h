#pragma once

#include "core/CoreExport.h"

#include <QString>
#include <QStringList>

namespace OFS::Translations {

// Installs every .qm file in languages/<code>/ of each search folder (earlier folders win), plus
// Qt's own qtbase_<lang>.qm. English (USA) installs none. Returns the number of catalogs.
OFS_CORE_EXPORT int install(const QString& code, const QStringList& searchDirs);

// The Qt locale name of a language code: "pt-br" -> "pt_BR", "de" -> "de".
OFS_CORE_EXPORT QString localeName(const QString& code);

} // namespace OFS::Translations
