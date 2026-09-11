#pragma once

#include "core/anims/AirDocument.h"
#include "core/audio/AudioMixer.h"
#include "core/bg/BgDocument.h"
#include "core/bg/BgSimulator.h"
#include "core/sounds/SoundDocument.h"
#include "core/sprites/SpriteDocument.h"
#include "engines/mugen/emulator/FightScreen.h"
#include "engines/mugen/emulator/Training.h"
#include "engines/mugen/runtime/Character.h"
#include "engines/mugen/runtime/CharacterData.h"

#include <QList>
#include <QMap>
#include <QSet>
#include <QString>

#include <memory>
#include <optional>
#include <tuple>
#include <vector>

namespace OFS {
class Project;
}

namespace OFS::Mugen {

// The match the Emulator shows, run one tick at a time.
class EmulatorSession
{
public:
    struct Options
    {
        QString engineDir;
        int rounds = 0; // 0: the engine's fight.def
        int time = 0;   // in counts (0: the engine's mugen.cfg, -1: none)
        bool training = false;
        TrainingOptions trainingOptions;
        bool randomEnemy = false;
        // Explod mode: the engine's default, M.U.G.E.N 1.0, M.U.G.E.N 1.1, MUGENext, the player's version.
        int explodMode = 0;
        std::uint32_t seed = 1;
    };

    struct Player
    {
        std::shared_ptr<Runtime::CharacterData> data;
        std::unique_ptr<Runtime::Character> character;
        const SpriteDocument* sprites = nullptr;
        std::shared_ptr<SpriteDocument> ownSprites;
        const SoundDocument* sounds = nullptr;
        std::shared_ptr<SoundDocument> ownSounds;
        // SFF v1: the pal1 to pal12 palettes, and the sprite palettes they replace.
        std::array<std::shared_ptr<const Formats::Palette>, 12> acts;
        QSet<const SpritePalette*> shared;
        QString name;
    };

    struct Effect
    {
        Runtime::Vec2 pos;
        bool common = true;
        int playerId = 0;
        int anim = 0;
        Runtime::AnimPlayer player;
        bool under = false;
    };

    struct Stage
    {
        QString path;
        BgDocument bg;
        std::unique_ptr<BgSimulator> simulator;
        std::shared_ptr<const SpriteDocument> sprites;
        AirDocument animations;
        float zoffset = 200;
        float boundLeft = -150;
        float boundRight = 150;
        float p1x = -70;
        float p2x = 70;
    };

    EmulatorSession();
    ~EmulatorSession();

    bool start(Project& project, const Options& options, QString* error = nullptr);
    bool isStarted() const { return m_started; }

    void tick(const Runtime::InputState& p1, const Runtime::InputState& p2);
    void setTrainingOptions(const TrainingOptions& options);
    const Training& training() const { return m_training; }

    Runtime::Match& match() { return *m_match; }
    const Runtime::Match& match() const { return *m_match; }
    const std::vector<Player>& players() const { return m_players; }
    const Stage& stage() const { return m_stage; }
    const QList<Effect>& effects() const { return m_effects; }
    const Runtime::CharacterData* fightFx() const { return m_fightFx.get(); }
    const SpriteDocument* fightFxSprites() const { return m_fightFxSprites.get(); }
    const SpriteDocument* spritesOf(const Runtime::Character& character) const;
    const Formats::Palette* paletteOf(const Runtime::Character& character, int group, int number) const;
    std::shared_ptr<const AudioMixer::Sound> sound(int playerId, bool common, int group, int index);
    const SoundDocument* commonSounds() const { return m_commonSounds.get(); }
    std::shared_ptr<const AudioMixer::Sound> soundFrom(const SoundDocument* doc, int group, int index);
    const FightScreen& fightScreen() const { return m_fightScreen; }
    std::array<FightScreen::Side, 2> sides() const;
    static constexpr int ScreenSounds = -2;
    void playSounds(AudioMixer& mixer);
    // -1 left to 1 right. 0 without stereo.
    float stereoPan(int playerId, float pan, bool absolute) const;

    static QString engineFolderOf(const QString& projectFile);
    // stages/stage0.def, else the first stage of the engine folder.
    static QString defaultStage(const QString& engineDir);
    // chars/kfm, else the first one.
    static QString defaultCharacter(const QString& engineDir);
    static bool canPlay(const QString& projectType);
    static bool isMugen11Engine(const QString& engineDir);
    static Runtime::ExplodRules explodRules(int explodMode, const QString& engineDir);

private:
    bool loadStage(const QString& path, const QString& engineDir, QString* error);
    static void loadPalettes(Player& player);
    const Player* playerOf(const Runtime::Character& character) const;
    void loadFightFx(const QString& engineDir);
    void loadCommonSounds(const QString& engineDir);
    Runtime::RoundSettings roundSettings(const Options& options) const;
    QString textOf(const QString& path) const;

    bool m_started = false;
    Options m_options;
    Project* m_project = nullptr;
    QString m_fightDef;
    std::unique_ptr<Runtime::Match> m_match;
    std::vector<Player> m_players;
    Stage m_stage;
    std::shared_ptr<Runtime::CharacterData> m_fightFx;
    std::shared_ptr<SpriteDocument> m_fightFxSprites;
    QList<Effect> m_effects;
    std::shared_ptr<SoundDocument> m_commonSounds;
    FightScreen m_fightScreen;
    Training m_training;
    QMap<std::tuple<const SoundDocument*, int, int>, std::shared_ptr<const AudioMixer::Sound>> m_soundCache;
    bool m_stereo = true;
    float m_panningWidth = 240;
};

} // namespace OFS::Mugen
