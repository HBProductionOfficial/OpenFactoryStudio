#pragma once

#include "core/CoreExport.h"

#include <QString>
#include <QStringList>

class QTranslator;

namespace OFS::Script {

// Literal strings passed to FF.translate (or OFS.translate), in order and once each.
OFS_CORE_EXPORT QStringList translatableStrings(const QString& code);

// The context is the script's base name, as FF.translate looks it up.
OFS_CORE_EXPORT bool writeTranslationFile(const QString& path, const QString& context, const QStringList& strings,
                                          QString* error = nullptr);

// Loads <name>_<language>.qm beside the script.
OFS_CORE_EXPORT bool loadScriptTranslation(QTranslator& translator, const QString& scriptPath);

} // namespace OFS::Script
