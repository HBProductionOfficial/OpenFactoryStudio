#include "engines/mugen/MugenRun.h"

#include "core/editors/EditorInfo.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/project/ProjectSearch.h"
#include "engines/mugen/DefFile.h"
#include "engines/mugen/MugenTypes.h"
#include "engines/mugen/ProjectGraphics.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace OFS::Mugen {

namespace {

struct MugenRunTexts
{
    Q_DECLARE_TR_FUNCTIONS(OFS::Mugen::MugenRunTexts)
};

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
            copy.error = MugenRunTexts::tr("Could not create the folder %1.").arg(QDir::toNativeSeparators(dir));
            return false;
        }
        copy.folders << dir;
    }
    return true;
}

bool put(const Project& project, const QString& source, const QString& target, RunCopy& copy)
{
    if (QFileInfo::exists(target)) {
        copy.error =
            MugenRunTexts::tr("%1 already exists; running would overwrite it.").arg(QDir::toNativeSeparators(target));
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
        copy.error = MugenRunTexts::tr("Could not write %1: %2").arg(QDir::toNativeSeparators(target), error);
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

// A character also takes the other files its definition names (WinMUGEN palette files belong to no
// editor).
bool copyProjectFiles(const Project& project, const QString& dir, RunCopy& copy)
{
    const QDir projectDir(project.directory());
    QStringList paths = ProjectSearch::projectFiles(project);
    if (project.typeId() == MugenType::Character) {
        DefFile def;
        def.parse(projectFileText(project, project.filePath()));
        for (const QString& value : def.values(QStringLiteral("Files"), QStringLiteral(".*"))) {
            const QString path = QDir::cleanPath(projectDir.filePath(QDir::fromNativeSeparators(value)));
            if (QFileInfo(path).isFile() && !paths.contains(path, Qt::CaseInsensitive))
                paths << path;
        }
    }
    for (const QString& path : std::as_const(paths)) {
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

    // A project already inside the engine folder runs from there (saved first) without a copy: a
    // character by its folder (or folder/file.def), a stage by its name, a motif by its folder.
    const QString projectFile = QDir::cleanPath(QDir::fromNativeSeparators(project.filePath()));
    auto inside = [&](const QString& folder) -> QString {
        const QString base = root + QLatin1Char('/') + folder + QLatin1Char('/');
        return projectFile.startsWith(base, Qt::CaseInsensitive) && !project.isMemoryOnly()
                   ? projectFile.mid(base.size())
                   : QString();
    };
    if (type == MugenType::Character) {
        if (const QString relative = inside(QStringLiteral("chars")); relative.count(QLatin1Char('/')) == 1) {
            const QString folder = relative.section(QLatin1Char('/'), 0, 0);
            const bool same = QFileInfo(relative).completeBaseName().compare(folder, Qt::CaseInsensitive) == 0;
            copy.name = same ? folder : relative;
            copy.inPlace = true;
            return copy;
        }
    } else if (type == MugenType::Stage) {
        if (const QString relative = inside(QStringLiteral("stages")); !relative.isEmpty()) {
            copy.name = relative.endsWith(QLatin1String(".def"), Qt::CaseInsensitive) ? relative.chopped(4) : relative;
            copy.inPlace = true;
            return copy;
        }
    } else if (type == MugenType::Motif) {
        const QString relative = inside(QStringLiteral("data"));
        if (relative.count(QLatin1Char('/')) == 1 &&
            QFileInfo(relative).fileName().compare(QLatin1String("system.def"), Qt::CaseInsensitive) == 0) {
            copy.name = relative.section(QLatin1Char('/'), 0, 0);
            copy.inPlace = true;
            return copy;
        }
    }

    if (type == MugenType::Character) {
        const QString chars = root + QStringLiteral("/chars");
        copy.name = freeName(chars);
        const QString dir = chars + QLatin1Char('/') + copy.name;
        if (!makeFolders(dir, copy) || !copyProjectFiles(project, dir, copy) ||
            !put(project, project.filePath(), dir + QLatin1Char('/') + copy.name + QStringLiteral(".def"), copy))
            return fail();
        return copy;
    }

    if (type == MugenType::Stage) {
        const QString stages = root + QStringLiteral("/stages");
        copy.name = freeName(stages);
        DefFile def;
        def.parse(projectFileText(project, project.filePath()));
        // The sprite and music files go where the definition says (paths from the engine folder).
        const QList<QPair<QString, QString>> referenced = {
            {def.value(QStringLiteral("BGdef"), QStringLiteral("spr")),
             project.editorFiles(EditorId::Sprites).value(0)},
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
                copy.error = MugenRunTexts::tr("%1 already exists with other content; running would overwrite it.")
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

    if (type == MugenType::Motif) {
        const QString data = root + QStringLiteral("/data");
        copy.name = freeName(data);
        const QString dir = data + QLatin1Char('/') + copy.name;
        if (!makeFolders(dir, copy) || !copyProjectFiles(project, dir, copy))
            return fail();
        // The screenpack's own fight screen goes along, but not its sprites, sounds and fonts. Same as
        // Fighter Factory.
        DefFile def;
        def.parse(projectFileText(project, project.filePath()));
        const QString fight =
            QDir::fromNativeSeparators(def.value(QStringLiteral("Files"), QStringLiteral("fight")).trimmed());
        if (!fight.isEmpty()) {
            const QDir projectDir(project.directory());
            const QString relative = projectDir.relativeFilePath(projectDir.filePath(fight));
            const QString target = dir + QLatin1Char('/') + relative;
            if (QFileInfo(projectDir.filePath(fight)).isFile() && !relative.startsWith(QLatin1String("..")) &&
                !QDir::isAbsolutePath(relative) && !QFileInfo::exists(target) &&
                !put(project, projectDir.filePath(fight), target, copy))
                return fail();
        }
        const QString system = dir + QStringLiteral("/system.def");
        if (!QFileInfo::exists(system) && !put(project, project.filePath(), system, copy))
            return fail();
        return copy;
    }

    copy.error = MugenRunTexts::tr("Only characters, stages and screenpacks can be run.");
    return copy;
}

} // namespace OFS::Mugen
