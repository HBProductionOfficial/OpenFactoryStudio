#include "engines/mugen/MugenProject.h"

#include "core/anims/AirEdits.h"
#include "core/editors/EditorInfo.h"
#include "core/engine/Engine.h"
#include "core/project/ProjectFile.h"
#include "core/sounds/SoundFile.h"
#include "core/sprites/SpriteFile.h"
#include "engines/mugen/DefFile.h"
#include "engines/mugen/MugenTypes.h"

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
    readInfo(def);
    collectFiles(def);
    // Same as Fighter Factory: editors whose files are all missing are hidden, but never Definitions,
    // Sprites or the Emulator.
    QStringList editors;
    for (const QString& id : editorsForType(typeId())) {
        const QStringList files = editorFiles(id);
        const bool always = id == EditorId::Definitions || id == EditorId::Sprites || id == EditorId::Emulator;
        if (always || std::any_of(files.cbegin(), files.cend(), [](const QString& f) { return QFileInfo::exists(f); }))
            editors << id;
    }
    setEditorIds(editors);
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
    if (typeId == MugenType::Font)
        editors.insert(1, EditorId::Sprites);
    setEditorIds(editors);
    readInfo(def);
    collectFiles(def);
    if (typeId == MugenType::Font)
        setEditorFiles(EditorId::Sprites, {directory() + QLatin1Char('/') + QFileInfo(filePath()).completeBaseName() +
                                           QStringLiteral(".sff")});

    const Formats::SffVersion sffVersion = versionId() == QLatin1String("1.1")   ? Formats::SffVersion::V201
                                           : versionId() == QLatin1String("1.0") ? Formats::SffVersion::V2
                                                                                 : Formats::SffVersion::V1;
    for (const QString& editorId : editors) {
        const EditorInfo info = engine()->editor(editorId);
        for (const QString& path : editorFiles(editorId)) {
            if (path.isEmpty() || file(path))
                continue;
            ProjectFile* document = nullptr;
            if (editorId == EditorId::Sprites) {
                auto* sprites = new SpriteFile(path, editorId, this);
                sprites->createEmpty(sffVersion);
                document = sprites;
            } else if (editorId == EditorId::Sounds) {
                auto* sounds = new SoundFile(path, editorId, this);
                sounds->createEmpty();
                document = sounds;
            } else if (info.workspace == WorkspaceType::Code || editorId == EditorId::Animations ||
                       editorId == EditorId::Backgrounds) {
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
        {QStringLiteral("def"), EditorId::Definitions}, {QStringLiteral("sff"), EditorId::Sprites},
        {QStringLiteral("snd"), EditorId::Sounds},      {QStringLiteral("air"), EditorId::Animations},
        {QStringLiteral("cns"), EditorId::States},      {QStringLiteral("st"), EditorId::States},
        {QStringLiteral("cmd"), EditorId::Commands},
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
    setTypeId(MugenType::Character);
    setVersionId(QStringLiteral("1.1"));
    setVersionDeclared(true);
    setEditorIds({editorId, EditorId::Emulator});
    setEditorFiles(editorId, {filePath()});
    setEditorFiles(EditorId::Emulator, {});
    setName(QString());
    return true;
}

void MugenProject::readInfo(const DefFile& def)
{
    setVersionId(detectVersion(def));
    setVersionDeclared(!def.value(QStringLiteral("Info"), QStringLiteral("mugenversion")).trimmed().isEmpty());
    QString name = def.value(QStringLiteral("Info"), QStringLiteral("name"));
    if (name.isEmpty() && typeId() == MugenType::Stage)
        name = def.value(QStringLiteral("Info"), QStringLiteral("displayname"));
    setName(name);
}

QString MugenProject::detectType(const DefFile& def)
{
    if (def.hasSection(QStringLiteral("SceneDef")))
        return MugenType::Storyboard;
    if (def.hasSection(QStringLiteral("StageInfo")) || def.hasSection(QStringLiteral("Camera")) ||
        def.hasSection(QStringLiteral("PlayerInfo")))
        return MugenType::Stage;
    if (def.hasSection(QStringLiteral("Title Info")) || def.hasSection(QStringLiteral("Select Info")))
        return MugenType::Motif;
    if (def.hasSection(QStringLiteral("FightFX")) || def.hasSection(QStringLiteral("Lifebar")))
        return MugenType::Fight;
    if (def.hasSection(QStringLiteral("Characters")) || def.hasSection(QStringLiteral("ExtraStages")))
        return MugenType::Select;
    if (def.hasSection(QStringLiteral("FNT v2")) ||
        (def.hasSection(QStringLiteral("Def")) && !def.value(QStringLiteral("Def"), QStringLiteral("Type")).isEmpty()))
        return MugenType::Font;
    return MugenType::Character;
}

QString MugenProject::detectVersion(const DefFile& def)
{
    const QString v = def.value(QStringLiteral("Info"), QStringLiteral("mugenversion")).trimmed();
    if (v.isEmpty()) {
        // Same as Fighter Factory: without a mugenversion, a localcoord or the fight screen's [FightFx] means
        // M.U.G.E.N 1.0. Newer features (like the 1.1 camera zoom) still leave it at the DOS beta.
        const bool localcoord =
            !def.value(QStringLiteral("Info"), QStringLiteral("localcoord")).trimmed().isEmpty() ||
            !def.value(QStringLiteral("StageInfo"), QStringLiteral("localcoord")).trimmed().isEmpty();
        return localcoord || def.hasSection(QStringLiteral("FightFx")) ? QStringLiteral("1.0")
                                                                       : QStringLiteral("dosbeta");
    }
    if (v.startsWith(QLatin1String("1.1")))
        return QStringLiteral("1.1");
    if (v.startsWith(QLatin1String("1.")))
        return QStringLiteral("1.0");
    // Date-style versions ("04.14.2001", "2002.04.14") are the Windows/Linux betas.
    return QStringLiteral("winbeta");
}

QStringList MugenProject::editorsForType(const QString& typeId)
{
    const QString def = EditorId::Definitions;
    const QString spr = EditorId::Sprites;
    const QString anim = EditorId::Animations;
    const QString bg = EditorId::Backgrounds;
    const QString cmd = EditorId::Commands;
    const QString st = EditorId::States;
    const QString snd = EditorId::Sounds;
    const QString emul = EditorId::Emulator;

    if (typeId == MugenType::Character)
        return {def, spr, anim, cmd, st, snd, emul};
    if (typeId == MugenType::Stage || typeId == MugenType::Storyboard)
        return {def, spr, anim, bg, emul};
    if (typeId == MugenType::Motif)
        return {def, spr, anim, bg, snd, emul};
    if (typeId == MugenType::Fight)
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

    setEditorFiles(EditorId::Definitions, {filePath()});
    setEditorFiles(EditorId::Emulator, {});

    const QString type = typeId();
    if (type == MugenType::Character) {
        setEditorFiles(EditorId::Sprites, {resolve(def.value(files, QStringLiteral("sprite")))});
        setEditorFiles(EditorId::Animations, {resolve(def.value(files, QStringLiteral("anim")))});
        setEditorFiles(EditorId::Commands, {resolve(def.value(files, QStringLiteral("cmd")))});
        setEditorFiles(EditorId::Sounds, {resolve(def.value(files, QStringLiteral("sound")))});
        QStringList states;
        addUnique(states, resolve(def.value(files, QStringLiteral("cns"))));
        addUnique(states, resolve(def.value(files, QStringLiteral("st"))));
        for (const QString& st : def.values(files, QStringLiteral("st[0-9]+")))
            addUnique(states, resolve(st));
        setEditorFiles(EditorId::States, states);
    } else if (type == MugenType::Stage) {
        setEditorFiles(EditorId::Sprites, {resolve(def.value(QStringLiteral("BGdef"), QStringLiteral("spr")))});
        setEditorFiles(EditorId::Animations, {filePath()});
        setEditorFiles(EditorId::Backgrounds, {filePath()});
    } else if (type == MugenType::Storyboard) {
        setEditorFiles(EditorId::Sprites, {resolve(def.value(QStringLiteral("SceneDef"), QStringLiteral("spr")))});
        setEditorFiles(EditorId::Animations, {filePath()});
        setEditorFiles(EditorId::Backgrounds, {filePath()});
    } else if (type == MugenType::Motif) {
        QStringList definitions{filePath()};
        addUnique(definitions, resolveData(def.value(files, QStringLiteral("select"))));
        setEditorFiles(EditorId::Definitions, definitions);
        setEditorFiles(EditorId::Sprites, {resolve(def.value(files, QStringLiteral("spr")))});
        setEditorFiles(EditorId::Animations, {filePath()});
        setEditorFiles(EditorId::Backgrounds, {filePath()});
        setEditorFiles(EditorId::Sounds, {resolve(def.value(files, QStringLiteral("snd")))});
    } else if (type == MugenType::Fight) {
        QStringList sprites;
        addUnique(sprites, resolve(def.value(files, QStringLiteral("sff"))));
        addUnique(sprites, resolve(def.value(files, QStringLiteral("fightfx.sff"))));
        QStringList anims{filePath()};
        addUnique(anims, resolve(def.value(files, QStringLiteral("fightfx.air"))));
        QStringList sounds;
        addUnique(sounds, resolve(def.value(files, QStringLiteral("snd"))));
        addUnique(sounds, resolve(def.value(files, QStringLiteral("common.snd"))));
        setEditorFiles(EditorId::Sprites, sprites);
        setEditorFiles(EditorId::Animations, anims);
        setEditorFiles(EditorId::Sounds, sounds);
    }
}

} // namespace OFS::Mugen
