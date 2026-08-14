#include "engines/mugen/MugenRun.h"

#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/project/ProjectSearch.h"
#include "engines/mugen/DefFile.h"
#include "engines/mugen/ProjectGraphics.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace OFS::Mugen {

namespace {

QString tr(const char* text)
{
    return QCoreApplication::translate("OFS::Mugen::MugenRun", text);
}

QString freeName(const QString& dir)
{
    for (int n = 0;; ++n) {
        const QString name = QStringLiteral("ofs_%1").arg(n);
        const QString path = dir + QLatin1Char('/') + name;
        if (!QFileInfo::exists(path) && !QFileInfo::exists(path + QStringLiteral(".def")))
            return name;
    }
}

bool makeFolders(const QString& folder, RunCopy& copy)
{
    QStringList missing;
    for (QString dir = QDir::cleanPath(folder); !QFileInfo::exists(dir);) {
        missing.prepend(dir);
        const QString parent = QFileInfo(dir).absolutePath();
        if (parent == dir)
            break;
        dir = parent;
    }
    for (const QString& dir : std::as_const(missing)) {
        if (!QDir().mkdir(dir)) {
            copy.error = tr("Could not create the folder %1.").arg(QDir::toNativeSeparators(dir));
            return false;
        }
        copy.folders << dir;
    }
    return true;
}

bool put(const Project& project, const QString& source, const QString& target, RunCopy& copy)
{
    if (QFileInfo::exists(target)) {
        copy.error = tr("%1 already exists; running would overwrite it.").arg(QDir::toNativeSeparators(target));
        return false;
    }
    if (!makeFolders(QFileInfo(target).absolutePath(), copy))
        return false;
    QString error;
    bool ok = false;
    if (const ProjectFile* file = project.file(source)) {
        ok = file->writeCopy(target, &error);
    } else {
        QFile in(source);
        ok = in.copy(target);
        error = in.errorString();
    }
    if (!ok) {
        copy.error = tr("Could not write %1: %2").arg(QDir::toNativeSeparators(target), error);
        return false;
    }
    copy.files << target;
    return true;
}

QByteArray fileHash(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    return hash.result();
}

bool copyProjectFiles(const Project& project, const QString& dir, RunCopy& copy)
{
    const QDir projectDir(project.directory());
    for (const QString& path : ProjectSearch::projectFiles(project)) {
        const QString relative = projectDir.relativeFilePath(path);
        if (relative.startsWith(QLatin1String("..")) || QDir::isAbsolutePath(relative))
            continue; // outside the project folder: the engine finds it in its own folders
        if (const ProjectFile* file = project.file(path); file && file->isUntitled())
            continue; // a new document nothing refers to yet
        if (!put(project, path, dir + QLatin1Char('/') + relative, copy))
            return false;
    }
    return true;
}

} // namespace

RunCopy prepareRunCopy(const Project& project, const QString& engineDir)
{
    RunCopy copy;
    const QString root = QDir::cleanPath(QDir::fromNativeSeparators(engineDir));
    const QString type = project.typeId();
    auto fail = [&copy]() {
        const QString error = copy.error;
        copy.remove();
        copy = RunCopy();
        copy.error = error;
        return copy;
    };

    if (type == QLatin1String("char")) {
        const QString chars = root + QStringLiteral("/chars");
        copy.name = freeName(chars);
        const QString dir = chars + QLatin1Char('/') + copy.name;
        if (!makeFolders(dir, copy) || !copyProjectFiles(project, dir, copy) ||
            !put(project, project.filePath(), dir + QLatin1Char('/') + copy.name + QStringLiteral(".def"), copy))
            return fail();
        return copy;
    }

    if (type == QLatin1String("stage")) {
        const QString stages = root + QStringLiteral("/stages");
        copy.name = freeName(stages);
        DefFile def;
        def.parse(projectFileText(project, project.filePath()));
        // The sprite and music files go where the definition says (paths from the engine folder).
        const QList<QPair<QString, QString>> referenced = {
            {def.value(QStringLiteral("BGdef"), QStringLiteral("spr")),
             project.editorFiles(QStringLiteral("sprites")).value(0)},
            {def.value(QStringLiteral("Music"), QStringLiteral("bgmusic")), QString()}};
        for (const auto& [raw, known] : referenced) {
            if (raw.trimmed().isEmpty())
                continue;
            const QString relative = QDir::fromNativeSeparators(raw.trimmed());
            const QString source = !known.isEmpty() ? known : QDir(project.directory()).filePath(relative);
            const QString target = QDir::cleanPath(root + QLatin1Char('/') + relative);
            if (!QFileInfo::exists(source) ||
                QFileInfo(source).canonicalFilePath() == QFileInfo(target).canonicalFilePath())
                continue;
            if (QFileInfo::exists(target)) {
                if (fileHash(target) == fileHash(source))
                    continue;
                copy.error = tr("%1 already exists with other content; running would overwrite it.")
                                 .arg(QDir::toNativeSeparators(target));
                return fail();
            }
            if (!put(project, source, target, copy))
                return fail();
        }
        if (!put(project, project.filePath(), stages + QLatin1Char('/') + copy.name + QStringLiteral(".def"), copy))
            return fail();
        return copy;
    }

    if (type == QLatin1String("motif")) {
        const QString data = root + QStringLiteral("/data");
        copy.name = freeName(data);
        const QString dir = data + QLatin1Char('/') + copy.name;
        if (!makeFolders(dir, copy) || !copyProjectFiles(project, dir, copy))
            return fail();
        const QString system = dir + QStringLiteral("/system.def");
        if (!QFileInfo::exists(system) && !put(project, project.filePath(), system, copy))
            return fail();
        return copy;
    }

    copy.error = tr("Only characters, stages and screenpacks can be run.");
    return copy;
}

} // namespace OFS::Mugen
