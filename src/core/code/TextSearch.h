#pragma once

#include "core/CoreExport.h"

#include <QList>
#include <QRegularExpression>
#include <QString>

class QPlainTextEdit;

namespace OFS {

struct SearchOptions
{
    bool caseSensitive = false;
    bool wholeWords = false;
    bool regExp = false; // replacements may use captures as %1, %2...
};

// A text pattern shared by the quick search bar, the multi-line Find/Replace dialog and Advanced
// Search. Plain patterns may span lines ("\n" between them).
class OFS_CORE_EXPORT TextSearch
{
public:
    struct Match
    {
        qsizetype start = -1;
        qsizetype length = 0;
        QStringList captures;
        bool isValid() const { return start >= 0; }
    };

    TextSearch(const QString& pattern, const SearchOptions& options);

    bool isEmpty() const { return m_pattern.isEmpty(); }
    bool isValid() const;
    QString errorString() const;

    // With backward, the last match ending at or before from. Empty matches are skipped.
    Match find(const QString& text, qsizetype from, bool backward = false) const;
    QList<Match> findAll(const QString& text) const;
    Match matchAt(const QString& text, qsizetype start, qsizetype length) const;

    QString replacement(const Match& match, const QString& replace) const;

private:
    QString m_pattern;
    SearchOptions m_options;
    QRegularExpression m_regex;
};

// Selecting a match in a CodeEditor unfolds its code section.
namespace TextSearchOps {

OFS_CORE_EXPORT bool find(QPlainTextEdit* edit, const TextSearch& search, bool backward, bool wrap);
OFS_CORE_EXPORT bool replace(QPlainTextEdit* edit, const TextSearch& search, const QString& replace, bool backward,
                             bool wrap);
OFS_CORE_EXPORT int replaceAll(QPlainTextEdit* edit, const TextSearch& search, const QString& replace);

} // namespace TextSearchOps

} // namespace OFS
