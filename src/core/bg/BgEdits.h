#pragma once

#include "core/CoreExport.h"
#include "core/bg/BgDocument.h"

#include <QList>
#include <QStringList>

#include <utility>

namespace OFS::BgEdits {

// Backgrounds editor edits on the lines of a definitions file. Sections are found by header line.

// Rewrites the last "key = ..." line, keeping its key spelling and comment, or adds one.
OFS_CORE_EXPORT void setValue(QStringList& lines, int headerLine, const QString& key, const QString& value);
OFS_CORE_EXPORT void setNumbers(QStringList& lines, int headerLine, const QString& key, const QList<double>& values,
                                const QList<double>& defaults);
OFS_CORE_EXPORT void removeKey(QStringList& lines, int headerLine, const QString& key);
OFS_CORE_EXPORT void setHeader(QStringList& lines, int headerLine, const QString& header);

// Lines the section owns, [first, end), including the comment lines right above its header.
OFS_CORE_EXPORT std::pair<int, int> block(const QStringList& lines, int headerLine);
OFS_CORE_EXPORT int insertSection(QStringList& lines, int at, const QStringList& text);
OFS_CORE_EXPORT int duplicate(QStringList& lines, int headerLine);
OFS_CORE_EXPORT void removeSection(QStringList& lines, int headerLine);
OFS_CORE_EXPORT void removeLines(QStringList& lines, int first, int end);
OFS_CORE_EXPORT int swapSections(QStringList& lines, int headerLine, int otherHeaderLine);

OFS_CORE_EXPORT QString number(double value);
OFS_CORE_EXPORT QString numbers(const QList<double>& values);

} // namespace OFS::BgEdits
