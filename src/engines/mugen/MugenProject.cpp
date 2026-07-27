#include "engines/mugen/MugenProject.h"

#include "engines/mugen/DefFile.h"

#include <QDir>
#include <QFileInfo>

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
    setVersionId(detectVersion(def));
    setVersionDeclared(!def.value(QStringLiteral("Info"), QStringLiteral("mugenversion")).trimmed().isEmpty());
    setEditorIds(editorsForType(typeId()));

    QString name = def.value(QStringLiteral("Info"), QStringLiteral("name"));
    if (name.isEmpty() && typeId() == QLatin1String("stage"))
        name = def.value(QStringLiteral("Info"), QStringLiteral("displayname"));
    setName(name);
    collectFiles(def);
    return true;
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
    if (typeId == QLatin1String("stage"))
        return {def, spr, anim, bg, emul};
    if (typeId == QLatin1String("motif") || typeId == QLatin1String("storyboard"))
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
        setEditorFiles(QStringLiteral("sounds"),
                       {resolve(def.value(QStringLiteral("SceneDef"), QStringLiteral("snd")))});
    } else if (type == QLatin1String("motif")) {
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
