#pragma once

#include "core/CoreExport.h"
#include "core/code/TextSearch.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace OFS {

class Project;
class SyntaxDatabase;

struct OFS_CORE_EXPORT SearchHit
{
    QString filePath;
    int line = 0; // 0-based
    int column = 0;
    int length = 0;
    QString lineText;
    int offset = 0; // in the file text, with "\n" line ends
    QString matched;
    QStringList captures;
    QString replacement; // Lazy syntax fix: the text proposed for matched
};

// Files open in a project are read and changed in their document, in one undo step. Closed files
// are rewritten with their own encoding and line endings.
namespace ProjectSearch {

// "*.def, *.cmd" or "*.def *.sff; *.air" -> wildcards. An empty text gives no pattern.
OFS_CORE_EXPORT QStringList splitPatterns(const QString& text);
OFS_CORE_EXPORT bool matchesPatterns(const QString& path, const QStringList& include, const QStringList& exclude);

OFS_CORE_EXPORT QStringList projectFiles(const Project& project);
OFS_CORE_EXPORT QStringList directoryFiles(const QString& dir, const QStringList& include, const QStringList& exclude);

OFS_CORE_EXPORT QString fileText(const QList<Project*>& projects, const QString& path, bool* ok = nullptr);
OFS_CORE_EXPORT QList<SearchHit> find(const QString& path, const QString& text, const TextSearch& search);
OFS_CORE_EXPORT int replaceAll(const QList<Project*>& projects, const QString& path, const TextSearch& search,
                               const QString& replacement, QString* error = nullptr);
OFS_CORE_EXPORT int replaceHits(const QList<Project*>& projects, const QString& path, const QList<SearchHit>& hits,
                                QString* error = nullptr);

// Lazy syntax fix: the engine accepts some values in short form, like a keyword cut to its first
// letters ("animtype = Med"). Each hit proposes the full keyword ("Medium").
OFS_CORE_EXPORT QList<SearchHit> lazySyntaxFixes(const QString& path, const QString& text, const SyntaxDatabase* syntax,
                                                 const QString& fileType);

} // namespace ProjectSearch

} // namespace OFS
