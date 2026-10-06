#include "engines/mugen/emulator/EmulatorSession.h"

#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/sounds/SoundFile.h"
#include "core/sprites/SpriteFile.h"
#include "engines/mugen/MugenTypes.h"
#include "engines/mugen/runtime/Files.h"
#include "engines/mugen/runtime/IniText.h"
#include "formats/palette/PaletteFormats.h"
#include "formats/sff/SffDocument.h"
#include "formats/snd/SndDocument.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextDocument>

#include <algorithm>

namespace OFS::Mugen {

using namespace Runtime;

namespace {

std::shared_ptr<SoundDocument> loadSounds(const QString& path)
{
    if (path.isEmpty())
        return nullptr;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return nullptr;
    const QByteArray data = file.readAll();
    auto snd = Formats::SndDocument::read(
        Formats::ByteSpan(reinterpret_cast<const std::byte*>(data.constData()), std::size_t(data.size())));
    if (!snd)
        return nullptr;
    return std::make_shared<SoundDocument>(SoundDocument::fromSnd(snd.value()));
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

QString EmulatorSession::defaultCharacter(const QString& engineDir)
{
    const QString preferred = QDir(engineDir).filePath(QStringLiteral("chars/kfm/kfm.def"));
    if (QFileInfo(preferred).isFile())
        return preferred;
    QStringList defs;
    QDirIterator it(QDir(engineDir).filePath(QStringLiteral("chars")), {QStringLiteral("*.def")}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext())
        defs << it.next();
    std::sort(defs.begin(), defs.end());
    return defs.value(0);
}

bool EmulatorSession::canPlay(const QString& projectType)
{
    return projectType == MugenType::Character || projectType == MugenType::Stage || projectType == MugenType::Fight;
}

QString EmulatorSession::textOf(const QString& path) const
{
    if (m_project) {
        if (auto* code = qobject_cast<CodeFile*>(m_project->file(path)))
            return code->document()->toPlainText();
    }
    return readText(path);
}

bool EmulatorSession::loadStage(const QString& path, const QString& engineDir, QString* error)
{
    const QString text = textOf(path);
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
    const IniSection* about = ini.section(QStringLiteral("Info"));
    m_stage.name = about ? unquote(about->value(QStringLiteral("name"))) : QString();
    m_stage.displayName =
        about ? unquote(about->value(QStringLiteral("displayname"), about->value(QStringLiteral("name")))) : QString();
    m_stage.author = about ? unquote(about->value(QStringLiteral("author"))) : QString();
    m_stage.boundLeft = number(camera, "boundleft", -150);
    m_stage.boundRight = number(camera, "boundright", 150);
    m_stage.p1x = number(players, "p1startx", -70);
    m_stage.p2x = number(players, "p2startx", 70);
    const QString folder = QFileInfo(path).absolutePath();
    const IniSection* bgdef = ini.section(QStringLiteral("BGdef"));
    if (bgdef) {
        const QString sff = resolve(bgdef->value(QStringLiteral("spr")), folder, engineDir);
        if (auto* file = m_project ? qobject_cast<SpriteFile*>(m_project->file(sff)) : nullptr)
            m_stage.sprites =
                std::shared_ptr<const SpriteDocument>(std::shared_ptr<const SpriteDocument>(), &file->document());
        else if (!sff.isEmpty())
            m_stage.sprites = readSprites(sff);
    }
    const int group = m_stage.bg.indexOf(QStringLiteral("BG"));
    if (group >= 0)
        m_stage.simulator = std::make_unique<BgSimulator>(m_stage.bg.groups()[group]);
    m_stage.animations = AirDocument::parse(text);
    loadStageLook(ini);

    // The music: a file in the engine folder (usually in sound/), else beside the stage. bgmvolume is in
    // percent. bgvolume (MUGEN 1.0) goes from -255 to 255 and 0 keeps the volume as it is.
    m_stage.music = BackgroundMusic::Track();
    if (const IniSection* musicSection = ini.section(QStringLiteral("Music"))) {
        m_stage.music.file =
            BackgroundMusic::find(musicSection->value(QStringLiteral("bgmusic")),
                                  {engineDir, folder, QDir(engineDir).filePath(QStringLiteral("sound"))});
        const float offset = std::clamp(number(musicSection, "bgvolume", 0), -255.0f, 255.0f);
        m_stage.music.volume = std::max(0.0f, number(musicSection, "bgmvolume", 100) / 100) * (255 + offset) / 255;
        m_stage.music.loopStart = musicSection->value(QStringLiteral("bgmloopstart")).toLongLong();
        m_stage.music.loopEnd = musicSection->value(QStringLiteral("bgmloopend")).toLongLong();
    }
    return true;
}

void EmulatorSession::loadStageLook(const IniText& ini)
{
    auto numbers = [](const IniSection* section, const char* key, int count, float fallback) {
        QList<float> out(count, fallback);
        if (!section || !section->has(QLatin1String(key)))
            return out;
        const QStringList parts = section->value(QLatin1String(key)).split(QLatin1Char(','));
        for (int i = 0; i < count && i < parts.size(); ++i) {
            bool ok = false;
            const float value = parts[i].trimmed().toFloat(&ok);
            if (ok)
                out[i] = value;
        }
        return out;
    };
    const IniSection* camera = ini.section(QStringLiteral("Camera"));
    Match::CameraSettings& zoom = m_stage.camera;
    zoom = Match::CameraSettings();
    zoom.zoomOut = number(camera, "zoomout", 1);
    zoom.zoomIn = number(camera, "zoomin", 1);
    zoom.startZoom = number(camera, "startzoom", 1);
    zoom.tension = number(camera, "tension", 50);
    zoom.boundHigh = number(camera, "boundhigh", 0);

    Shadows& look = m_stage.shadows;
    look = Shadows();
    const IniSection* shadow = ini.section(QStringLiteral("Shadow"));
    look.intensity = std::clamp(int(number(shadow, "intensity", 128)), 0, 256);
    // MUGEN 1.1 no longer reads the shadow's colour.
    CharacterData version;
    if (const IniSection* about = ini.section(QStringLiteral("Info")))
        version.setMugenVersion(unquote(about->value(QStringLiteral("mugenversion"))));
    if (!version.isMugen11()) {
        const QList<float> color = numbers(shadow, "color", 3, 0);
        for (std::size_t i = 0; i < 3; ++i)
            look.color[i] = std::clamp(int(color[qsizetype(i)]), 0, 255);
    }
    look.yScale = number(shadow, "yscale", 0.4f);
    look.xShear = number(shadow, "xshear", 0);
    if (shadow && shadow->has(QStringLiteral("fade.range"))) {
        const QList<float> range = numbers(shadow, "fade.range", 2, 0);
        look.fade = range[0] < range[1];
        look.fadeTop = range[0];
        look.fadeBottom = range[1];
    }
    const IniSection* reflection = ini.section(QStringLiteral("Reflection"));
    const bool reflect = number(reflection, "reflect", 0) != 0 || number(shadow, "reflect", 0) != 0;
    look.reflection = std::clamp(int(number(reflection, "intensity", reflect ? 128 : 0)), 0, 256);
}

void EmulatorSession::loadFightFx(const QString& engineDir)
{
    const QString air = QDir(engineDir).filePath(QStringLiteral("data/fightfx.air"));
    const QString sff = QDir(engineDir).filePath(QStringLiteral("data/fightfx.sff"));
    if (!QFileInfo(air).isFile())
        return;
    m_fightFx = std::make_shared<CharacterData>();
    m_fightFx->setAnimations(readText(air));
    m_fightFxSprites = readSprites(sff);
}

void EmulatorSession::loadCommonSounds(const QString& engineDir)
{
    // fight.def names the common sounds. MUGEN looks for them beside it, then in the engine folder.
    const QString fightDef = QDir(engineDir).filePath(QStringLiteral("data/fight.def"));
    const IniText fight = IniText::parse(readText(fightDef));
    QString name = QStringLiteral("common.snd");
    if (const IniSection* files = fight.section(QStringLiteral("Files"));
        files && files->has(QStringLiteral("common.snd")))
        name = files->value(QStringLiteral("common.snd"));
    m_commonSounds = loadSounds(resolve(name, QFileInfo(fightDef).path(), engineDir));
    const IniText config = IniText::parse(readText(QDir(engineDir).filePath(QStringLiteral("data/mugen.cfg"))));
    const IniSection* sound = config.section(QStringLiteral("Sound"));
    m_stereo = number(sound, "stereoeffects", 1) != 0;
    m_panningWidth = std::max(1.0f, number(sound, "panningwidth", 240));
}

RoundSettings EmulatorSession::roundSettings(const Options& options) const
{
    RoundSettings settings;
    const QString dir = options.engineDir;
    const IniText fight = IniText::parse(textOf(m_fightDef));
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
    settings.callTime = m_fightScreen.callTime();
    settings.p1x = m_stage.p1x;
    settings.p2x = m_stage.p2x;
    return settings;
}

bool EmulatorSession::start(Project& project, const Options& options, QString* error)
{
    m_started = false;
    m_options = options;
    // Drop the previous match before the players: its helpers and explods point into their data, and
    // dropping the players first crashed a later restart at random.
    m_match = std::make_unique<Match>(1);
    m_players.clear();
    m_effects.clear();
    m_soundCache.clear();
    m_music = BackgroundMusic();
    m_project = &project;
    const QString engineDir = options.engineDir;
    if (engineDir.isEmpty() || !QFileInfo(QDir(engineDir).filePath(QStringLiteral("data/common1.cns"))).isFile()) {
        if (error)
            *error = QStringLiteral("Choose the engine directory: a M.U.G.E.N folder (with data/common1.cns).");
        return false;
    }
    const QString type = project.typeId();
    const QString character = type == MugenType::Character ? project.filePath() : defaultCharacter(engineDir);
    if (character.isEmpty()) {
        if (error)
            *error = QStringLiteral("The engine directory has no character to play.");
        return false;
    }
    m_fightDef =
        type == MugenType::Fight ? project.filePath() : QDir(engineDir).filePath(QStringLiteral("data/fight.def"));

    auto data = std::make_shared<CharacterData>();
    Project* source = &project;
    data->setTextSource([source](const QString& path) -> std::optional<QString> {
        if (auto* code = qobject_cast<CodeFile*>(source->file(path)))
            return code->document()->toPlainText();
        return std::nullopt;
    });
    QString loadError;
    if (!data->load(character, engineDir, &loadError)) {
        if (error)
            *error = loadError;
        return false;
    }
    Player p1;
    p1.data = data;
    p1.name = data->displayName();
    if (auto* sprites = qobject_cast<SpriteFile*>(project.file(data->spriteFile())))
        p1.sprites = &sprites->document();
    else if ((p1.ownSprites = readSprites(data->spriteFile())))
        p1.sprites = p1.ownSprites.get();
    if (auto* sounds = qobject_cast<SoundFile*>(project.file(data->soundFile())))
        p1.sounds = &sounds->document();
    else if ((p1.ownSounds = loadSounds(data->soundFile())))
        p1.sounds = p1.ownSounds.get();

    Player p2;
    p2.data = data;
    p2.sprites = p1.sprites;
    loadPalettes(p1);
    p2.ownSprites = p1.ownSprites;
    p2.acts = p1.acts;
    p2.shared = p1.shared;
    p2.sounds = p1.sounds;
    p2.ownSounds = p1.ownSounds;
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
                p2.ownSprites = readSprites(other->spriteFile());
                p2.sprites = p2.ownSprites.get();
                p2.ownSounds = loadSounds(other->soundFile());
                p2.sounds = p2.ownSounds.get();
                loadPalettes(p2);
                p2.name = other->displayName();
            }
        }
    }

    const QString stage = type == MugenType::Stage ? project.filePath() : defaultStage(engineDir);
    if (stage.isEmpty() || !loadStage(stage, engineDir, error)) {
        if (error && error->isEmpty())
            *error = QStringLiteral("The engine directory has no stage.");
        return false;
    }
    loadFightFx(engineDir);
    loadCommonSounds(engineDir);
    m_fightScreen.load(engineDir, m_fightDef, [this](const QString& path) { return textOf(path); });
    m_fightScreen.setSkipCalls(options.training);
    m_fightScreen.reset();
    m_training.reset(options.seed);

    m_match = std::make_unique<Match>(options.seed ? options.seed : 1);
    m_match->setBounds(m_stage.boundLeft - 160, m_stage.boundRight + 160);
    m_match->setCamera(m_stage.camera);
    m_match->setScreenHeight(240, m_stage.zoffset);
    m_match->setStageInfo(QStringLiteral("info.name"), m_stage.name);
    m_match->setStageInfo(QStringLiteral("info.displayname"), m_stage.displayName);
    m_match->setStageInfo(QStringLiteral("info.author"), m_stage.author);
    {
        const IniText config = IniText::parse(readText(QDir(engineDir).filePath(QStringLiteral("data/mugen.cfg"))));
        const IniSection* rules = config.section(QStringLiteral("Rules"));
        Match::Rules values;
        values.attackLifeToPowerMul = number(rules, "default.attack.lifetopowermul", values.attackLifeToPowerMul);
        values.getHitLifeToPowerMul = number(rules, "default.gethit.lifetopowermul", values.getHitLifeToPowerMul);
        values.superTargetDefenceMul = number(rules, "super.targetdefencemul", values.superTargetDefenceMul);
        m_match->setRules(values);
    }
    if (m_fightFx)
        m_match->setFightFx(m_fightFx.get());
    p1.character = std::make_unique<Character>(*p1.data, m_match.get());
    p2.character = std::make_unique<Character>(*p2.data, m_match.get());
    m_match->setNoKo(options.training);
    m_match->setExplodRules(explodRules(options.explodMode, engineDir));
    m_players.push_back(std::move(p1));
    m_players.push_back(std::move(p2));
    setTrainingOptions(options.trainingOptions);
    m_match->startMatch(roundSettings(options));
    BackgroundMusic::Track stageMusic = m_stage.music;
    stageMusic.volume *= BackgroundMusic::engineVolume(engineDir);
    m_music.play(stageMusic);
    m_started = true;
    return true;
}

bool EmulatorSession::isMugen11Engine(const QString& engineDir)
{
    const IniText config = IniText::parse(readText(QDir(engineDir).filePath(QStringLiteral("data/mugen.cfg"))));
    const IniSection* video = config.section(QStringLiteral("Video"));
    return video && (video->has(QStringLiteral("stagefit")) || video->has(QStringLiteral("keepaspect")));
}

ExplodRules EmulatorSession::explodRules(int explodMode, const QString& engineDir)
{
    switch (explodMode) {
    case 1:
        return ExplodRules::Mugen10;
    case 2:
    case 3: // MUGENext extends 1.1
        return ExplodRules::Mugen11;
    case 4:
        return ExplodRules::PlayerVersion;
    default:
        // A 1.1 engine follows each character's version. 1.0 knows only its own rules.
        return isMugen11Engine(engineDir) ? ExplodRules::PlayerVersion : ExplodRules::Mugen10;
    }
}

const EmulatorSession::Player* EmulatorSession::playerOf(const Character& character) const
{
    const Character* player = character.root() ? character.root() : &character;
    for (const Player& p : m_players) {
        if (p.character.get() == player)
            return &p;
    }
    return nullptr;
}

const SpriteDocument* EmulatorSession::spritesOf(const Character& character) const
{
    const Player* p = playerOf(character);
    return p ? p->sprites : nullptr;
}

void EmulatorSession::loadPalettes(Player& player)
{
    // MUGEN draws the sprites of an SFF v1 character that share its palette with the chosen palette file
    // (pal1 here). The others keep their own.
    player.acts = {};
    player.shared.clear();
    if (!player.sprites || player.sprites->version() != Formats::SffVersion::V1 || !player.data)
        return;
    const QStringList& files = player.data->paletteFiles();
    for (int i = 0; i < files.size() && i < 12; ++i) {
        QFile file(files[i]);
        if (files[i].isEmpty() || !file.open(QIODevice::ReadOnly))
            continue;
        const QByteArray data = file.readAll();
        auto act = Formats::readAct(
            Formats::ByteSpan(reinterpret_cast<const std::byte*>(data.constData()), std::size_t(data.size())),
            Formats::ActOrder::Reversed);
        if (act)
            player.acts[std::size_t(i)] = std::make_shared<const Formats::Palette>(std::move(act.value()));
    }
    for (const SpriteEntry& entry : player.sprites->sprites()) {
        if (entry.header.samePalette && entry.palette)
            player.shared.insert(entry.palette.get());
    }
}

const Formats::Palette* EmulatorSession::paletteOf(const Character& character, int group, int number) const
{
    return paletteOf(character, group, number, character.paletteRemap());
}

const Formats::Palette* EmulatorSession::paletteOf(const Character& character, int group, int number,
                                                   const PaletteRemap& remap) const
{
    const Player* p = playerOf(character);
    const SpriteDocument* sprites = p ? p->sprites : nullptr;
    const int index = sprites ? sprites->findSprite(group, number) : -1;
    if (index < 0)
        return nullptr;
    const SpriteEntry& entry = sprites->sprites()[index];
    const bool v1 = sprites->version() == Formats::SffVersion::V1;
    const bool act = v1 && p->acts[0] && p->shared.contains(entry.palette.get());
    QPair<int, int> id;
    if (act)
        id = {1, 1};
    else if (entry.palette)
        id = {entry.palette->palette.group, entry.palette->palette.number};
    else
        return nullptr;
    const QPair<int, int> shown = remap.value(id, remap.value(DrawState::AllPalettes, id));
    if (shown == id && !act)
        return nullptr;
    if (v1 && shown.first == 1 && shown.second >= 1 && shown.second <= 12 && p->acts[std::size_t(shown.second - 1)])
        return p->acts[std::size_t(shown.second - 1)].get();
    const int palette = sprites->findPalette(shown.first, shown.second);
    if (palette >= 0)
        return &sprites->palettes()[palette]->palette.colors;
    return act ? p->acts[0].get() : nullptr;
}

std::shared_ptr<const AudioMixer::Sound> EmulatorSession::sound(int playerId, bool common, int group, int index)
{
    const SoundDocument* doc = nullptr;
    if (common) {
        doc = m_commonSounds.get();
    } else if (const Character* who = m_match ? m_match->playerById(playerId) : nullptr) {
        const Character* player = who->root() ? who->root() : who;
        for (const Player& p : m_players) {
            if (p.character.get() == player)
                doc = p.sounds;
        }
    }
    return soundFrom(doc, group, index);
}

std::shared_ptr<const AudioMixer::Sound> EmulatorSession::soundFrom(const SoundDocument* doc, int group, int index)
{
    if (!doc || group < 0)
        return nullptr;
    const auto key = std::make_tuple(doc, group, index);
    if (auto cached = m_soundCache.constFind(key); cached != m_soundCache.constEnd())
        return cached.value();
    std::shared_ptr<const AudioMixer::Sound> prepared;
    if (const int at = doc->find(group, index); at >= 0) {
        if (auto decoded = SoundEdits::decode(doc->sounds()[at]))
            prepared = AudioMixer::prepare(decoded.value().buffer);
    }
    m_soundCache.insert(key, prepared);
    return prepared;
}

float EmulatorSession::stereoPan(int playerId, float pan, bool absolute) const
{
    if (!m_stereo)
        return 0;
    float x = pan;
    if (!absolute) {
        const Character* who = m_match ? m_match->playerById(playerId) : nullptr;
        if (!who)
            return 0;
        x = float(who->pos().x()) + pan * float(who->facing()) - m_match->cameraX();
    }
    return std::clamp(x / (m_panningWidth / 2), -1.0f, 1.0f);
}

void EmulatorSession::playSounds(AudioMixer& mixer)
{
    if (!m_started)
        return;
    for (const SoundEvent& e : m_match->sounds()) {
        const float pan = stereoPan(e.playerId, e.pan, e.absolutePan);
        switch (e.kind) {
        case SoundEvent::Kind::Play:
            // volume: 128 makes a sound a quarter louder and -128 a quarter softer. volumescale is a percentage
            // of that.
            if (auto s = sound(e.playerId, e.common, e.group, e.index))
                mixer.play(s, e.playerId, e.channel,
                           std::max(0.0f, 1.0f + float(e.volume) * 0.25f / 128.0f) * e.volumeScale / 100.0f, pan,
                           e.loop, e.lowPriority, std::clamp(e.freqMul, 0.01f, 5.0f));
            break;
        case SoundEvent::Kind::Stop:
            mixer.stop(e.playerId, e.channel);
            break;
        case SoundEvent::Kind::Pan:
            mixer.setPan(e.playerId, e.channel, pan);
            break;
        }
    }
    for (const FightScreen::Sound& s : m_fightScreen.sounds()) {
        if (auto prepared = soundFrom(m_fightScreen.soundFile(), s.group, s.number))
            mixer.play(prepared, ScreenSounds, -1);
    }
    m_music.update(mixer, m_match->isAsserted(QStringLiteral("nomusic")));
}

void EmulatorSession::setTrainingOptions(const TrainingOptions& options)
{
    m_options.trainingOptions = options;
    m_training.setOptions(options);
    if (m_players.size() > 1 && m_players[1].character) {
        const bool ai = m_options.training && options.control == TrainingOptions::Control::AI;
        m_players[1].character->setAiLevel(ai ? std::clamp(options.aiLevel, 1, 8) : 0);
    }
}

std::array<FightScreen::Side, 2> EmulatorSession::sides() const
{
    std::array<FightScreen::Side, 2> out;
    for (std::size_t i = 0; i < out.size() && i < m_players.size(); ++i) {
        out[i].character = m_players[i].character.get();
        out[i].sprites = m_players[i].sprites;
        out[i].name = m_players[i].name;
    }
    return out;
}

void EmulatorSession::tick(const InputState& p1, const InputState& p2)
{
    if (!m_started)
        return;
    m_players[0].character->setInput(p1);
    m_players[1].character->setInput(m_options.training ? m_training.dummyInput(*m_match, *m_players[1].character, p2)
                                                        : p2);
    m_match->tick();
    if (m_options.training) {
        for (int side = 0; side < 2; ++side)
            m_training.refill(*m_match, *m_players[std::size_t(side)].character, side);
    }
    m_fightScreen.step(*m_match, sides());
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
