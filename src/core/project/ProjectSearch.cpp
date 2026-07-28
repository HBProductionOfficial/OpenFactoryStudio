#include "core/project/ProjectSearch.h"

#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/syntax/SyntaxDatabase.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextCursor>
#include <QTextDocument>

#include <algorithm>

namespace OFS::ProjectSearch {

namespace {

CodeFile* openFile(const QList<Project*>& projects, const QString& path)
{
    for (Project* project : projects) {
        if (auto* file = qobject_cast<CodeFile*>(project->file(path)))
            return file;
    }
    return nullptr;
}

} // namespace

QStringList splitPatterns(const QString& text)
{
    static const QRegularExpression separators(QStringLiteral("[,;\\s]+"));
    return text.split(separators, Qt::SkipEmptyParts);
}

bool matchesPatterns(const QString& path, const QStringList& include, const QStringList& exclude)
{
    const QString name = QFileInfo(path).fileName();
    for (const QString& pattern : exclude) {
        if (SyntaxDatabase::wildcardMatch(pattern, name))
            return false;
    }
    if (include.isEmpty())
        return true;
    for (const QString& pattern : include) {
        if (SyntaxDatabase::wildcardMatch(pattern, name))
            return true;
    }
    return false;
}

QStringList projectFiles(const Project& project)
{
    QStringList files;
    auto add = [&files](const QString& path) {
        const QString cleaned = QDir::cleanPath(QDir::fromNativeSeparators(path));
        if (!cleaned.isEmpty() && QFileInfo(cleaned).isFile() && !files.contains(cleaned, Qt::CaseInsensitive))
            files << cleaned;
    };
    add(project.filePath());
    for (const QString& editorId : project.editorIds()) {
        for (const QString& path : project.editorFiles(editorId))
            add(path);
    }
    return files;
}

QStringList directoryFiles(const QString& dir, const QStringList& include, const QStringList& exclude)
{
    QStringList files;
    if (dir.isEmpty() || !QFileInfo(dir).isDir())
        return files;
    QDirIterator it(dir, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (matchesPatterns(path, include, exclude))
            files << QDir::fromNativeSeparators(path);
    }
    files.sort(Qt::CaseInsensitive);
    return files;
}

QString fileText(const QList<Project*>& projects, const QString& path, bool* ok)
{
    if (CodeFile* file = openFile(projects, path)) {
        if (ok)
            *ok = true;
        return file->document()->toPlainText();
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (ok)
            *ok = false;
        return {};
    }
    QString encoding;
    QByteArray bom;
    QString text = CodeFile::decode(f.readAll(), &encoding, &bom);
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    if (ok)
        *ok = true;
    return text;
}

QList<SearchHit> find(const QString& path, const QString& text, const TextSearch& search)
{
    QList<SearchHit> hits;
    const QList<TextSearch::Match> matches = search.findAll(text);
    if (matches.isEmpty())
        return hits;
    QList<qsizetype> starts{0};
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text.at(i) == QLatin1Char('\n'))
            starts << i + 1;
    }
    for (const TextSearch::Match& m : matches) {
        const auto it = std::upper_bound(starts.cbegin(), starts.cend(), m.start);
        const int line = int(it - starts.cbegin()) - 1;
        const qsizetype lineStart = starts.at(line);
        qsizetype lineEnd = text.indexOf(QLatin1Char('\n'), lineStart);
        if (lineEnd < 0)
            lineEnd = text.size();
        SearchHit hit;
        hit.filePath = path;
        hit.line = line;
        hit.column = int(m.start - lineStart);
        hit.length = int(qMin(m.length, lineEnd - m.start));
        hit.lineText = text.mid(lineStart, lineEnd - lineStart);
        hits << hit;
    }
    return hits;
}

int replaceAll(const QList<Project*>& projects, const QString& path, const TextSearch& search,
               const QString& replacement, QString* error)
{
    auto replaceIn = [&search, &replacement](QTextDocument* document) {
        const QList<TextSearch::Match> matches = search.findAll(document->toPlainText());
        if (matches.isEmpty())
            return 0;
        QTextCursor cursor(document);
        cursor.beginEditBlock();
        for (auto it = matches.crbegin(); it != matches.crend(); ++it) {
            cursor.setPosition(int(it->start));
            cursor.setPosition(int(it->start + it->length), QTextCursor::KeepAnchor);
            cursor.insertText(search.replacement(*it, replacement));
        }
        cursor.endEditBlock();
        return int(matches.size());
    };

    if (CodeFile* open = openFile(projects, path))
        return replaceIn(open->document());

    // Closed file: go through a temporary CodeFile so its encoding, BOM and line endings stay.
    CodeFile file(path, QString());
    if (!file.load(error))
        return -1;
    const int count = replaceIn(file.document());
    if (count > 0 && !file.save(error))
        return -1;
    return count;
}

} // namespace OFS::ProjectSearch
