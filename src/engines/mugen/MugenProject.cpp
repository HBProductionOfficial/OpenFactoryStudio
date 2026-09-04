#include "engines/mugen/MugenProject.h"

#include "core/anims/AirEdits.h"
#include "core/engine/Engine.h"
#include "core/project/ProjectFile.h"
#include "core/sounds/SoundFile.h"
#include "core/sprites/SpriteFile.h"
#include "engines/mugen/DefFile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QTextDocument>

namespace OFS::Mugen {

MugenProject::MugenProject(Engine* engine, const QString& filePath, QObject* parent)
    : Project(engine, filePath, parent)
{}

bool MugenProject::load(QString* error)
{
    DefFile def;
    if (!def.load(filePath())) {
        if (error)
            *error = tr("Could not read '%1'.").arg(QDir::toNativeSeparators(filePath()));
        return false;
    }
    setTypeId(detectType(def));
    setEditorIds(editorsForType(typeId()));
    readInfo(def);
    collectFiles(def);
    return true;
}

bool MugenProject::createNew(const QString& typeId, const QString& blankDir, QString* error)
{
    auto readBlank = [&blankDir](const QString& fileName) {
        QFile file(QDir(blankDir).filePath(fileName));
        if (!file.open(QIODevice::ReadOnly))
            return QString();
        // The documents use LF line ends whatever the blank files have.
        return QString::fromUtf8(file.readAll()).replace(QLatin1String("\r\n"), QLatin1String("\n"));
    };
    const QString definitions = readBlank(QFileInfo(filePath()).fileName());
    if (definitions.isEmpty()) {
        if (error)
            *error = tr("There is no blank project of type '%1'.").arg(typeId);
        return false;
    }
    DefFile def;
    def.parse(definitions);
    setMemoryOnly(true);
    setTypeId(typeId);
    QStringList editors = editorsForType(typeId);
    if (typeId == QLatin1String("font"))
        editors.insert(1, QStringLiteral("sprites"));
    setEditorIds(editors);
    readInfo(def);
    collectFiles(def);
    if (typeId == QLatin1String("font"))
        setEditorFiles(QStringLiteral("sprites"), {directory() + QLatin1Char('/') +
                                                   QFileInfo(filePath()).completeBaseName() + QStringLiteral(".sff")});

    const Formats::SffVersion sffVersion = versionId() == QLatin1String("1.1")   ? Formats::SffVersion::V201
                                           : versionId() == QLatin1String("1.0") ? Formats::SffVersion::V2
                                                                                 : Formats::SffVersion::V1;
    for (const QString& editorId : editors) {
        const EditorInfo info = engine()->editor(editorId);
        for (const QString& path : editorFiles(editorId)) {
            if (path.isEmpty() || file(path))
                continue;
            ProjectFile* document = nullptr;
            if (editorId == QLatin1String("sprites")) {
                auto* sprites = new SpriteFile(path, editorId, this);
                sprites->createEmpty(sffVersion);
                document = sprites;
            } else if (editorId == QLatin1String("sounds")) {
                auto* sounds = new SoundFile(path, editorId, this);
                sounds->createEmpty();
                document = sounds;
            } else if (info.workspace == WorkspaceType::Code || editorId == QLatin1String("animations") ||
                       editorId == QLatin1String("backgrounds")) {
                auto* code = new CodeFile(path, editorId, this);
                code->createEmpty(path == filePath() ? definitions : readBlank(QFileInfo(path).fileName()));
                document = code;
            } else {
                continue;
            }
            document->setNameless(true);
            addFile(document);
            if (path != filePath()) {
                connect(document, &ProjectFile::pathChanged, this, [this, document](const QString& oldPath) {
                    if (!document->isUntitled())
                        renameReference(oldPath, document->path());
                });
            }
        }
    }
    return true;
}

void MugenProject::renameReference(const QString& oldPath, const QString& newPath)
{
    auto* definitions = qobject_cast<CodeFile*>(file(filePath()));
    if (!definitions || oldPath.compare(newPath, Qt::CaseInsensitive) == 0)
        return;
    const QDir folder(directory());
    const QString before = folder.relativeFilePath(oldPath);
    QString after = folder.relativeFilePath(newPath);
    if (after.startsWith(QLatin1String("../")))
        after = QDir::toNativeSeparators(newPath);
    // "key = value ; comment" lines whose value is the old name.
    static const QRegularExpression entry(QStringLiteral("^(\\s*[^=;\\[]+?=\\s*)(\"?)([^\";]*?)(\"?)(\\s*(;.*)?)$"));
    QStringList lines = AirEdits::lines(definitions->document());
    bool changed = false;
    for (QString& line : lines) {
        const QRegularExpressionMatch m = entry.match(line);
        if (!m.hasMatch() ||
            QDir::fromNativeSeparators(m.captured(3).trimmed()).compare(before, Qt::CaseInsensitive) != 0)
            continue;
        line = m.captured(1) + m.captured(2) + after + m.captured(4) + m.captured(5);
        changed = true;
    }
    if (changed)
        AirEdits::apply(definitions->document(), lines);
}

QString MugenProject::singleFileEditor(const QString& filePath)
{
    static const QHash<QString, QString> editors = {
        {QStringLiteral("def"), QStringLiteral("definitions")}, {QStringLiteral("sff"), QStringLiteral("sprites")},
        {QStringLiteral("snd"), QStringLiteral("sounds")},      {QStringLiteral("air"), QStringLiteral("animations")},
        {QStringLiteral("cns"), QStringLiteral("states")},      {QStringLiteral("st"), QStringLiteral("states")},
        {QStringLiteral("cmd"), QStringLiteral("commands")},
    };
    return editors.value(QFileInfo(filePath).suffix().toLower());
}

bool MugenProject::loadSingle(QString* error)
{
    const QString editorId = singleFileEditor(filePath());
    if (editorId.isEmpty() || !QFileInfo(filePath()).isFile()) {
        if (error)
            *error = tr("'%1' is not a file the editors can open.").arg(QDir::toNativeSeparators(filePath()));
        return false;
    }
    // Shown as a M.U.G.E.N 1.1 character, whatever the file is.
    setSingleFile(true);
    setTypeId(QStringLiteral("char"));
    setVersionId(QStringLiteral("1.1"));
    setVersionDeclared(true);
    setEditorIds({editorId, QStringLiteral("emulator")});
    setEditorFiles(editorId, {filePath()});
    setEditorFiles(QStringLiteral("emulator"), {});
    setName(QString());
    return true;
}

void MugenProject::readInfo(const DefFile& def)
{
    setVersionId(detectVersion(def));
    setVersionDeclared(!def.value(QStringLiteral("Info"), QStringLiteral("mugenversion")).trimmed().isEmpty());
    QString name = def.value(QStringLiteral("Info"), QStringLiteral("name"));
    if (name.isEmpty() && typeId() == QLatin1String("stage"))
        name = def.value(QStringLiteral("Info"), QStringLiteral("displayname"));
    setName(name);
}

QString MugenProject::detectType(const DefFile& def)
{
    if (def.hasSection(QStringLiteral("SceneDef")))
        return QStringLiteral("storyboard");
    if (def.hasSection(QStringLiteral("StageInfo")) || def.hasSection(QStringLiteral("Camera")) ||
        def.hasSection(QStringLiteral("PlayerInfo")))
        return QStringLiteral("stage");
    if (def.hasSection(QStringLiteral("Title Info")) || def.hasSection(QStringLiteral("Select Info")))
        return QStringLiteral("motif");
    if (def.hasSection(QStringLiteral("FightFX")) || def.hasSection(QStringLiteral("Lifebar")))
        return QStringLiteral("fight");
    if (def.hasSection(QStringLiteral("Characters")) || def.hasSection(QStringLiteral("ExtraStages")))
        return QStringLiteral("select");
    if (def.hasSection(QStringLiteral("FNT v2")) ||
        (def.hasSection(QStringLiteral("Def")) && !def.value(QStringLiteral("Def"), QStringLiteral("Type")).isEmpty()))
        return QStringLiteral("font");
    return QStringLiteral("char");
}

QString MugenProject::detectVersion(const DefFile& def)
{
    const QString v = def.value(QStringLiteral("Info"), QStringLiteral("mugenversion")).trimmed();
    if (v.isEmpty())
        return QStringLiteral("dosbeta");
    if (v.startsWith(QLatin1String("1.1")))
        return QStringLiteral("1.1");
    if (v.startsWith(QLatin1String("1.")))
        return QStringLiteral("1.0");
    // Date-style versions ("04.14.2001", "2002.04.14") are the Windows/Linux betas.
    return QStringLiteral("winbeta");
}

QStringList MugenProject::editorsForType(const QString& typeId)
{
    const QString def = QStringLiteral("definitions");
    const QString spr = QStringLiteral("sprites");
    const QString anim = QStringLiteral("animations");
    const QString bg = QStringLiteral("backgrounds");
    const QString cmd = QStringLiteral("commands");
    const QString st = QStringLiteral("states");
    const QString snd = QStringLiteral("sounds");
    const QString emul = QStringLiteral("emulator");

    if (typeId == QLatin1String("char"))
        return {def, spr, anim, cmd, st, snd, emul};
    if (typeId == QLatin1String("stage") || typeId == QLatin1String("storyboard"))
        return {def, spr, anim, bg, emul};
    if (typeId == QLatin1String("motif"))
        return {def, spr, anim, bg, snd, emul};
    if (typeId == QLatin1String("fight"))
        return {def, spr, anim, snd, emul};
    return {def, emul}; // font, select
}

QString MugenProject::resolve(const QString& relative) const
{
    if (relative.isEmpty())
        return {};
    const QString cleaned = QDir::fromNativeSeparators(relative.trimmed());
    if (QDir::isAbsolutePath(cleaned))
        return QDir::cleanPath(cleaned);
    return QDir::cleanPath(directory() + QLatin1Char('/') + cleaned);
}

QString MugenProject::resolveData(const QString& relative) const
{
    const QString own = resolve(relative);
    if (relative.isEmpty() || QFileInfo::exists(own) || QDir::isAbsolutePath(relative))
        return own;
    // The nearest "data" folder above the motif, then the folder holding it.
    QDir dir(directory());
    do {
        if (dir.dirName().compare(QLatin1String("data"), Qt::CaseInsensitive) == 0) {
            const QString inData = QDir::cleanPath(dir.filePath(relative));
            if (QFileInfo::exists(inData))
                return inData;
            QDir root = dir;
            if (root.cdUp() && QFileInfo::exists(root.filePath(relative)))
                return QDir::cleanPath(root.filePath(relative));
            break;
        }
    } while (dir.cdUp());
    return own;
}

void MugenProject::collectFiles(const DefFile& def)
{
    const QString files = QStringLiteral("Files");
    auto addUnique = [](QStringList& list, const QString& path) {
        if (!path.isEmpty() && !list.contains(path, Qt::CaseInsensitive))
            list << path;
    };

    setEditorFiles(QStringLiteral("definitions"), {filePath()});
    setEditorFiles(QStringLiteral("emulator"), {});

    const QString type = typeId();
    if (type == QLatin1String("char")) {
        setEditorFiles(QStringLiteral("sprites"), {resolve(def.value(files, QStringLiteral("sprite")))});
        setEditorFiles(QStringLiteral("animations"), {resolve(def.value(files, QStringLiteral("anim")))});
        setEditorFiles(QStringLiteral("commands"), {resolve(def.value(files, QStringLiteral("cmd")))});
        setEditorFiles(QStringLiteral("sounds"), {resolve(def.value(files, QStringLiteral("sound")))});
        QStringList states;
        addUnique(states, resolve(def.value(files, QStringLiteral("cns"))));
        addUnique(states, resolve(def.value(files, QStringLiteral("st"))));
        for (const QString& st : def.values(files, QStringLiteral("st[0-9]+")))
            addUnique(states, resolve(st));
        setEditorFiles(QStringLiteral("states"), states);
    } else if (type == QLatin1String("stage")) {
        setEditorFiles(QStringLiteral("sprites"), {resolve(def.value(QStringLiteral("BGdef"), QStringLiteral("spr")))});
        setEditorFiles(QStringLiteral("animations"), {filePath()});
        setEditorFiles(QStringLiteral("backgrounds"), {filePath()});
    } else if (type == QLatin1String("storyboard")) {
        setEditorFiles(QStringLiteral("sprites"),
                       {resolve(def.value(QStringLiteral("SceneDef"), QStringLiteral("spr")))});
        setEditorFiles(QStringLiteral("animations"), {filePath()});
        setEditorFiles(QStringLiteral("backgrounds"), {filePath()});
    } else if (type == QLatin1String("motif")) {
        QStringList definitions{filePath()};
        addUnique(definitions, resolveData(def.value(files, QStringLiteral("select"))));
        setEditorFiles(QStringLiteral("definitions"), definitions);
        setEditorFiles(QStringLiteral("sprites"), {resolve(def.value(files, QStringLiteral("spr")))});
        setEditorFiles(QStringLiteral("animations"), {filePath()});
        setEditorFiles(QStringLiteral("backgrounds"), {filePath()});
        setEditorFiles(QStringLiteral("sounds"), {resolve(def.value(files, QStringLiteral("snd")))});
    } else if (type == QLatin1String("fight")) {
        QStringList sprites;
        addUnique(sprites, resolve(def.value(files, QStringLiteral("sff"))));
        addUnique(sprites, resolve(def.value(files, QStringLiteral("fightfx.sff"))));
        QStringList anims{filePath()};
        addUnique(anims, resolve(def.value(files, QStringLiteral("fightfx.air"))));
        QStringList sounds;
        addUnique(sounds, resolve(def.value(files, QStringLiteral("snd"))));
        addUnique(sounds, resolve(def.value(files, QStringLiteral("common.snd"))));
        setEditorFiles(QStringLiteral("sprites"), sprites);
        setEditorFiles(QStringLiteral("animations"), anims);
        setEditorFiles(QStringLiteral("sounds"), sounds);
    }
}

} // namespace OFS::Mugen
