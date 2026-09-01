#include "core/project/ProjectBackup.h"

#include "core/project/Project.h"
#include "core/project/ZipArchive.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QRegularExpression>
#include <QTemporaryDir>

#include <algorithm>

namespace OFS::Backup {

namespace {

QString tr(const char* text)
{
    return QCoreApplication::translate("OFS::Backup", text);
}

QString fail(QString* error, const QString& message)
{
    if (error)
        *error = message;
    return {};
}

// "10.32.40", "1.35.3": the 12-hour clock without padding.
QString clock12(const QTime& time, QChar separator)
{
    const int hour = time.hour() % 12 == 0 ? 12 : time.hour() % 12;
    return QString::number(hour) + separator + QString::number(time.minute()) + separator +
           QString::number(time.second());
}

QString projectFolder(const Project& project)
{
    return project.directory();
}

} // namespace

QString folder(const Project& project)
{
    const QString base =
        project.archivePath().isEmpty() ? project.directory() : QFileInfo(project.archivePath()).absolutePath();
    return base + QStringLiteral("/backup");
}

QString fileName(const QDateTime& time, bool automatic)
{
    // Day and month padded, the 12-hour time not: "04.10.2026_1.35.3_am_".
    return QLocale::c().toString(time.date(), QStringLiteral("dd.MM.yyyy_")) + clock12(time.time(), QLatin1Char('.')) +
           QLatin1Char('_') + (time.time().hour() < 12 ? QStringLiteral("am") : QStringLiteral("pm")) +
           QLatin1Char('_') + (automatic ? QStringLiteral("Auto") : QString()) + QStringLiteral(".zip");
}

QDateTime timeOf(const QString& name, bool* automatic)
{
    static const QRegularExpression pattern(
        QStringLiteral(
            "^(\\d{2})\\.(\\d{2})\\.(\\d{4})_(\\d{1,2})\\.(\\d{1,2})\\.(\\d{1,2})_(am|pm)_(Auto)?\\d*(\\.zip)?$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = pattern.match(name);
    if (!m.hasMatch())
        return {};
    int hour = m.captured(4).toInt() % 12;
    if (m.captured(7).compare(QLatin1String("pm"), Qt::CaseInsensitive) == 0)
        hour += 12;
    const QDate date(m.captured(3).toInt(), m.captured(2).toInt(), m.captured(1).toInt());
    const QTime clock(hour, m.captured(5).toInt(), m.captured(6).toInt());
    if (!date.isValid() || !clock.isValid())
        return {};
    if (automatic)
        *automatic = !m.captured(8).isEmpty();
    return QDateTime(date, clock);
}

QString create(const Project& project, const QDateTime& time, bool automatic, QString* error)
{
    if (project.isMemoryOnly())
        return fail(error, tr("The project has not been saved yet."));
    QTemporaryDir staging;
    if (!staging.isValid())
        return fail(error, staging.errorString());
    const QString main = QDir(staging.path()).filePath(QFileInfo(project.filePath()).fileName());
    if (!project.writeCopyTo(main, error))
        return {};
    QStringList files;
    QDirIterator it(staging.path(), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
        files << QDir(staging.path()).relativeFilePath(it.next());
    files.sort(Qt::CaseInsensitive);

    const QString dir = folder(project);
    if (!QDir().mkpath(dir))
        return fail(error, tr("Cannot create the folder %1.").arg(QDir::toNativeSeparators(dir)));
    QString path = QDir(dir).filePath(fileName(time, automatic));
    for (int n = 2; QFileInfo::exists(path); ++n)
        path =
            QDir(dir).filePath(fileName(time, automatic).replace(QStringLiteral(".zip"), QString::number(n) + ".zip"));
    if (!Zip::create(path, staging.path(), files, error))
        return {};
    return path;
}

QList<Item> list(const Project& project)
{
    QList<Item> items;
    const QDir dir(folder(project));
    for (const QFileInfo& info : dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        if (info.isFile() && info.suffix().compare(QLatin1String("zip"), Qt::CaseInsensitive) != 0)
            continue;
        Item item;
        item.path = info.absoluteFilePath();
        item.time = timeOf(info.fileName(), &item.automatic);
        if (item.time.isValid()) {
            item.label = QLocale::c().toString(item.time.date(), QStringLiteral("dd/MM/yyyy ")) +
                         clock12(item.time.time(), QLatin1Char(':')) +
                         (item.time.time().hour() < 12 ? QStringLiteral(" am") : QStringLiteral(" pm"));
            if (item.automatic)
                item.label += tr(" (automatic)");
        } else {
            item.label = tr("Unknown");
        }
        items << item;
    }
    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        if (a.time.isValid() != b.time.isValid())
            return a.time.isValid();
        return a.time.isValid() && a.time < b.time;
    });
    return items;
}

bool restore(const Project& project, const QString& backupPath, QString* error)
{
    const QString target = projectFolder(project);
    const QFileInfo info(backupPath);
    if (info.isFile())
        return Zip::extract(backupPath, target, error);
    if (!info.isDir()) {
        fail(error, tr("The backup %1 does not exist.").arg(QDir::toNativeSeparators(backupPath)));
        return false;
    }
    const QDir source(backupPath);
    QDirIterator it(backupPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString file = it.next();
        const QString destination = QDir(target).filePath(source.relativeFilePath(file));
        QDir().mkpath(QFileInfo(destination).absolutePath());
        QFile::remove(destination);
        if (!QFile::copy(file, destination)) {
            fail(error, tr("Cannot write %1.").arg(QDir::toNativeSeparators(destination)));
            return false;
        }
    }
    return true;
}

} // namespace OFS::Backup
