#include "core/project/ContentPaths.h"

#include <QDir>
#include <QFileInfo>
#include <QStringList>

#include <filesystem>
#include <system_error>

namespace OFS {

namespace {

// Windows drops dots and spaces at the end of a name, so "x.lnk." opens x.lnk too.
bool isShortcut(QString part)
{
    while (part.endsWith(QLatin1Char('.')) || part.endsWith(QLatin1Char(' ')))
        part.chop(1);
    return part.endsWith(QLatin1String(".lnk"), Qt::CaseInsensitive);
}

// The names in a folder. Only the names are read: asking Qt about a shortcut follows it.
QStringList namesIn(const QString& folder)
{
    QStringList names;
    std::error_code error;
    std::filesystem::directory_iterator it(std::filesystem::path(folder.toStdU16String()), error);
    for (const std::filesystem::directory_iterator end; !error && it != end; it.increment(error))
        names << QString::fromStdU16String(it->path().filename().u16string());
    return names;
}

} // namespace

QString cleanContentPath(const QString& name)
{
    QString cleaned = name.trimmed();
    cleaned.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return cleaned;
}

QString findContentFile(const QString& path)
{
    const QString cleaned = QDir::cleanPath(cleanContentPath(path));
    if (cleaned.isEmpty() || QFileInfo::exists(cleaned))
        return cleaned;
    // Walk down from the deepest folder that exists, matching the rest by name.
    QString existing = cleaned;
    QStringList rest;
    while (!QFileInfo::exists(existing)) {
        const int slash = int(existing.lastIndexOf(QLatin1Char('/')));
        if (slash <= 0)
            return cleaned;
        rest.prepend(existing.mid(slash + 1));
        existing.truncate(slash);
    }
    QDir dir(existing);
    for (qsizetype i = 0; i < rest.size(); ++i) {
        const QString& part = rest[i];
        QString match;
        for (const QString& entry : namesIn(dir.path())) {
            if (entry.compare(part, Qt::CaseInsensitive) == 0 && !isShortcut(entry)) {
                match = entry;
                break;
            }
        }
        if (match.isEmpty())
            return cleaned;
        if (i + 1 == rest.size())
            return dir.filePath(match);
        if (!dir.cd(match))
            return cleaned;
    }
    return cleaned;
}

bool isUnsafeName(const QString& name)
{
    const QString cleaned = cleanContentPath(name);
    if (cleaned.startsWith(QLatin1String("//")))
        return true;
    for (const QString& part : cleaned.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        if (isShortcut(part))
            return true;
    }
    return false;
}

bool namesDriveOrStream(const QString& name)
{
#if defined(Q_OS_WIN)
    return name.contains(QLatin1Char(':'));
#else
    Q_UNUSED(name);
    return false;
#endif
}

bool isInsideFolder(const QString& path, const QString& folder)
{
    const QString relative = QDir(QDir::cleanPath(folder)).relativeFilePath(QDir::cleanPath(path));
    return relative != QLatin1String("..") && !relative.startsWith(QLatin1String("../")) &&
           !QDir::isAbsolutePath(relative);
}

QString findInFolders(const QString& name, const QStringList& folders)
{
    const QString cleaned = cleanContentPath(name);
    if (cleaned.isEmpty() || isUnsafeName(cleaned))
        return {};
    for (const QString& folder : folders) {
        if (folder.isEmpty())
            continue;
        const QString found = findContentFile(QDir(folder).filePath(cleaned));
        if (QFileInfo(found).isFile())
            return QDir::cleanPath(found);
    }
    return {};
}

} // namespace OFS
