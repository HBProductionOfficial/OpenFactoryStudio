#pragma once

#include "core/anims/AirDocument.h"
#include "core/bg/BgDocument.h"
#include "core/bg/BgSimulator.h"
#include "core/sprites/SpriteDocument.h"
#include "engines/mugen/runtime/Character.h"
#include "engines/mugen/runtime/CharacterData.h"

#include <QList>
#include <QString>

#include <memory>
#include <optional>
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
        int rounds = 0;           // 0: the engine's fight.def
        int time = 0;             // in counts (0: the engine's mugen.cfg, -1: none)
        bool training = false;
        bool randomEnemy = false;
        std::uint32_t seed = 1;
    };

    struct Player
    {
        std::shared_ptr<Runtime::CharacterData> data;
        std::unique_ptr<Runtime::Character> character;
        const SpriteDocument* sprites = nullptr;
        std::shared_ptr<SpriteDocument> ownSprites;
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
        std::shared_ptr<SpriteDocument> sprites;
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

    Runtime::Match& match() { return *m_match; }
    const Runtime::Match& match() const { return *m_match; }
    const std::vector<Player>& players() const { return m_players; }
    const Stage& stage() const { return m_stage; }
    const QList<Effect>& effects() const { return m_effects; }
    const Runtime::CharacterData* fightFx() const { return m_fightFx.get(); }
    const SpriteDocument* fightFxSprites() const { return m_fightFxSprites.get(); }
    const SpriteDocument* spritesOf(const Runtime::Character& character) const;

    static QString engineFolderOf(const QString& projectFile);
    // stages/stage0.def, else the first stage of the engine folder.
    static QString defaultStage(const QString& engineDir);

private:
    bool loadStage(const QString& path, const QString& engineDir, QString* error);
    void loadFightFx(const QString& engineDir);
    Runtime::RoundSettings roundSettings(const Options& options) const;

    bool m_started = false;
    Options m_options;
    std::unique_ptr<Runtime::Match> m_match;
    std::vector<Player> m_players;
    Stage m_stage;
    std::shared_ptr<Runtime::CharacterData> m_fightFx;
    std::shared_ptr<SpriteDocument> m_fightFxSprites;
    QList<Effect> m_effects;
};

} // namespace OFS::Mugen
