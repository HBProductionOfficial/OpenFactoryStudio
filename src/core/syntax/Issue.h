#pragma once

#include <QList>
#include <QString>

namespace OFS {

// A problem found in a code file, shown in the Issues panel and underlined in the editor.
struct Issue
{
    enum class Severity
    {
        Error,
        Warning,
        Deprecation,
        Information,
    };

    Severity severity = Severity::Error;
    int line = 0; // 0-based
    int column = 0;
    int length = 0; // 0 = to the end of the line
    QString message;
    QString file; // absolute path, set when issues of several files are collected

    bool isError() const { return severity == Severity::Error; }
};

using IssueList = QList<Issue>;

} // namespace OFS
