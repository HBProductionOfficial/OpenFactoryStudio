#pragma once

#include "core/CoreExport.h"
#include "core/bg/BgDocument.h"

#include <QStringList>

namespace OFS::BgEdits {

// Backgrounds editor edits on the lines of a definitions file. Sections are found by header line.

OFS_CORE_EXPORT void setValue(QStringList& lines, int headerLine, const QString& key, const QString& value);
OFS_CORE_EXPORT void removeKey(QStringList& lines, int headerLine, const QString& key);
OFS_CORE_EXPORT void setHeader(QStringList& lines, int headerLine, const QString& header);

OFS_CORE_EXPORT QStringList sectionText(const QStringList& lines, int headerLine);
OFS_CORE_EXPORT int insertSection(QStringList& lines, int afterHeaderLine, const QStringList& text);
OFS_CORE_EXPORT void removeSection(QStringList& lines, int headerLine);

OFS_CORE_EXPORT QString number(double value);

} // namespace OFS::BgEdits
