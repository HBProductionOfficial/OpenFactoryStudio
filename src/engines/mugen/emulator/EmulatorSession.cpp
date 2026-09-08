#include "engines/mugen/emulator/EmulatorSession.h"

#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/sprites/SpriteFile.h"
#include "engines/mugen/runtime/IniText.h"
#include "formats/sff/SffDocument.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextDocument>

#include <algorithm>

namespace OFS::Mugen {

using namespace Runtime;

namespace {

QString readText(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    QString encoding;
    QByteArray bom;
    return CodeFile::decode(file.readAll(), &encoding, &bom).replace(QLatin1String("\r\n"), QLatin1String("\n"));
}

std::shared_ptr<SpriteDocument> loadSprites(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return nullptr;
    const QByteArray data = file.readAll();
    auto sff = Formats::SffDocument::read(
        Formats::ByteSpan(reinterpret_cast<const std::byte*>(data.constData()), std::size_t(data.size())));
    if (!sff)
        return nullptr;
    return std::make_shared<SpriteDocument>(SpriteDocument::fromSff(sff.value()));
}

QString resolve(const QString& name, const QString& base, const QString& engineDir)
{
    const QString cleaned = QDir::fromNativeSeparators(unquote(name).trimmed());
    if (cleaned.isEmpty())
        return QString();
    for (const QString& candidate : {QDir(base).filePath(cleaned), QDir(engineDir).filePath(cleaned),
                                     QDir(engineDir).filePath(QStringLiteral("data/") + cleaned)}) {
        if (QFileInfo(candidate).isFile())
            return QDir::cleanPath(candidate);
    }
    return QString();
}

float number(const IniSection* section, const char* key, float fallback)
{
    if (!section || !section->has(QLatin1String(key)))
        return fallback;
    bool ok = false;
    const float value = section->value(QLatin1String(key)).split(QLatin1Char(',')).value(0).trimmed().toFloat(&ok);
    return ok ? value : fallback;
}

} // namespace

EmulatorSession::EmulatorSession() = default;
EmulatorSession::~EmulatorSession() = default;

QString EmulatorSession::engineFolderOf(const QString& projectFile)
{
    // <engine>/chars/<name>/<file>. The engine folder is the one with data/common1.cns.
    QDir dir = QFileInfo(projectFile).absoluteDir();
    for (int up = 0; up < 4 && dir.cdUp(); ++up) {
        if (QFileInfo(dir.filePath(QStringLiteral("data/common1.cns"))).isFile())
            return QDir::cleanPath(dir.absolutePath());
    }
    return QString();
}

QString EmulatorSession::defaultStage(const QString& engineDir)
{
    const QString preferred = QDir(engineDir).filePath(QStringLiteral("stages/stage0.def"));
    if (QFileInfo(preferred).isFile())
        return preferred;
    const QFileInfoList stages = QDir(QDir(engineDir).filePath(QStringLiteral("stages")))
                                     .entryInfoList({QStringLiteral("*.def")}, QDir::Files, QDir::Name);
    return stages.isEmpty() ? QString() : stages.first().absoluteFilePath();
}

bool EmulatorSession::loadStage(const QString& path, const QString& engineDir, QString* error)
{
    const QString text = readText(path);
    if (text.isEmpty()) {
        if (error)
            *error = QStringLiteral("cannot read the stage %1").arg(QDir::toNativeSeparators(path));
        return false;
    }
    m_stage.path = path;
    m_stage.bg = BgDocument::parse(text.split(QLatin1Char('\n')));
    const IniText ini = IniText::parse(text);
    const IniSection* info = ini.section(QStringLiteral("StageInfo"));
    const IniSection* camera = ini.section(QStringLiteral("Camera"));
    const IniSection* players = ini.section(QStringLiteral("PlayerInfo"));
    m_stage.zoffset = number(info, "zoffset", 200);
    m_stage.boundLeft = number(camera, "boundleft", -150);
    m_stage.boundRight = number(camera, "boundright", 150);
    m_stage.p1x = number(players, "p1startx", -70);
    m_stage.p2x = number(players, "p2startx", 70);
    const QString folder = QFileInfo(path).absolutePath();
    const IniSection* bgdef = ini.section(QStringLiteral("BGdef"));
    if (bgdef) {
        const QString sff = resolve(bgdef->value(QStringLiteral("spr")), folder, engineDir);
        if (!sff.isEmpty())
            m_stage.sprites = loadSprites(sff);
    }
    const int group = m_stage.bg.indexOf(QStringLiteral("BG"));
    if (group >= 0)
        m_stage.simulator = std::make_unique<BgSimulator>(m_stage.bg.groups()[group]);
    m_stage.animations = AirDocument::parse(text);
    return true;
}

void EmulatorSession::loadFightFx(const QString& engineDir)
{
    const QString air = QDir(engineDir).filePath(QStringLiteral("data/fightfx.air"));
    const QString sff = QDir(engineDir).filePath(QStringLiteral("data/fightfx.sff"));
    if (!QFileInfo(air).isFile())
        return;
    m_fightFx = std::make_shared<CharacterData>();
    m_fightFx->setAnimations(readText(air));
    m_fightFxSprites = loadSprites(sff);
}

RoundSettings EmulatorSession::roundSettings(const Options& options) const
{
    RoundSettings settings;
    const QString dir = options.engineDir;
    const IniText fight = IniText::parse(readText(QDir(dir).filePath(QStringLiteral("data/fight.def"))));
    const IniText config = IniText::parse(readText(QDir(dir).filePath(QStringLiteral("data/mugen.cfg"))));
    const IniSection* round = fight.section(QStringLiteral("Round"));
    settings.wins = int(number(round, "match.wins", 2));
    settings.startWait = int(number(round, "start.waittime", 30));
    settings.ctrlTime = int(number(round, "ctrl.time", 30));
    settings.overHitTime = int(number(round, "over.hittime", 10));
    settings.overWaitTime = int(number(round, "over.waittime", 45));
    settings.overWinTime = int(number(round, "over.wintime", 45));
    settings.overTime = int(number(round, "over.time", 210));
    const IniSection* time = fight.section(QStringLiteral("Time"));
    settings.framesPerCount = int(number(time, "framespercount", 60));
    settings.time = int(number(config.section(QStringLiteral("Options")), "time", 99));
    if (options.rounds > 0)
        settings.wins = options.rounds;
    if (options.time != 0)
        settings.time = options.time;
    if (options.training)
        settings.time = -1;
    settings.p1x = m_stage.p1x;
    settings.p2x = m_stage.p2x;
    return settings;
}

bool EmulatorSession::start(Project& project, const Options& options, QString* error)
{
    m_started = false;
    m_options = options;
    m_players.clear();
    m_effects.clear();
    const QString engineDir = options.engineDir;
    if (engineDir.isEmpty() || !QFileInfo(QDir(engineDir).filePath(QStringLiteral("data/common1.cns"))).isFile()) {
        if (error)
            *error = QStringLiteral("Choose the engine directory: a M.U.G.E.N folder (with data/common1.cns).");
        return false;
    }

    auto data = std::make_shared<CharacterData>();
    Project* source = &project;
    data->setTextSource([source](const QString& path) -> std::optional<QString> {
        if (auto* code = qobject_cast<CodeFile*>(source->file(path)))
            return code->document()->toPlainText();
        return std::nullopt;
    });
    QString loadError;
    if (!data->load(project.filePath(), engineDir, &loadError)) {
        if (error)
            *error = loadError;
        return false;
    }
    Player p1;
    p1.data = data;
    p1.name = data->displayName();
    if (auto* sprites = qobject_cast<SpriteFile*>(project.file(data->spriteFile())))
        p1.sprites = &sprites->document();
    else if ((p1.ownSprites = loadSprites(data->spriteFile())))
        p1.sprites = p1.ownSprites.get();

    Player p2;
    p2.data = data;
    p2.sprites = p1.sprites;
    p2.ownSprites = p1.ownSprites;
    p2.name = p1.name;
    if (options.randomEnemy) {
        QStringList defs;
        QDirIterator it(QDir(engineDir).filePath(QStringLiteral("chars")), {QStringLiteral("*.def")}, QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext())
            defs << it.next();
        std::sort(defs.begin(), defs.end());
        if (!defs.isEmpty()) {
            auto other = std::make_shared<CharacterData>();
            const QString pick = defs[int(options.seed % std::uint32_t(defs.size()))];
            if (other->load(pick, engineDir)) {
                p2.data = other;
                p2.ownSprites = loadSprites(other->spriteFile());
                p2.sprites = p2.ownSprites.get();
                p2.name = other->displayName();
            }
        }
    }

    const QString stage = defaultStage(engineDir);
    if (stage.isEmpty() || !loadStage(stage, engineDir, error)) {
        if (error && error->isEmpty())
            *error = QStringLiteral("The engine directory has no stage.");
        return false;
    }
    loadFightFx(engineDir);

    m_match = std::make_unique<Match>(options.seed ? options.seed : 1);
    m_match->setBounds(m_stage.boundLeft - 160, m_stage.boundRight + 160);
    if (m_fightFx)
        m_match->setFightFx(m_fightFx.get());
    p1.character = std::make_unique<Character>(*p1.data, m_match.get());
    p2.character = std::make_unique<Character>(*p2.data, m_match.get());
    m_players.push_back(std::move(p1));
    m_players.push_back(std::move(p2));
    m_match->startMatch(roundSettings(options));
    m_started = true;
    return true;
}

const SpriteDocument* EmulatorSession::spritesOf(const Character& character) const
{
    const Character* player = character.root() ? character.root() : &character;
    for (const Player& p : m_players) {
        if (p.character.get() == player)
            return p.sprites;
    }
    return nullptr;
}

void EmulatorSession::tick(const InputState& p1, const InputState& p2)
{
    if (!m_started)
        return;
    m_players[0].character->setInput(p1);
    m_players[1].character->setInput(p2);
    m_match->tick();
    if (m_stage.simulator)
        m_stage.simulator->step();
    for (Effect& e : m_effects)
        e.player.step();
    m_effects.erase(std::remove_if(m_effects.begin(), m_effects.end(),
                                   [](const Effect& e) { return e.player.isEmpty() || e.player.ended(); }),
                    m_effects.end());
    for (const SpawnEvent& spawn : m_match->spawns()) {
        Effect e;
        e.pos = spawn.pos;
        e.common = spawn.common;
        e.playerId = spawn.playerId;
        e.anim = spawn.anim;
        e.under = spawn.under;
        const CharacterData* source = nullptr;
        if (spawn.common) {
            source = m_fightFx.get();
        } else if (Character* owner = m_match->playerById(spawn.playerId)) {
            source = &owner->data();
        }
        const AirAnimation* animation = source ? source->animation(spawn.anim) : nullptr;
        if (!animation)
            continue;
        QList<int> times;
        for (const AirFrame& frame : animation->frames)
            times << frame.time;
        e.player = AnimPlayer(times, animation->loopStartFrame());
        m_effects << e;
    }
}

} // namespace OFS::Mugen
