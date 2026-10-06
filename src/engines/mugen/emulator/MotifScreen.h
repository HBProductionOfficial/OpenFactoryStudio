#pragma once

#include "core/anims/AirDocument.h"
#include "core/audio/AudioMixer.h"
#include "core/bg/BgDocument.h"
#include "core/bg/BgSimulator.h"
#include "core/sounds/SoundDocument.h"
#include "core/sprites/SpriteDocument.h"
#include "engines/mugen/emulator/BackgroundMusic.h"
#include "engines/mugen/emulator/FightScreen.h"
#include "engines/mugen/runtime/Commands.h"

#include <QImage>
#include <QMap>
#include <QString>
#include <QStringList>

#include <memory>

namespace OFS {
class BgRenderer;
class Project;
} // namespace OFS

namespace OFS::Mugen {

namespace Runtime {
struct IniSection;
}

// A motif's (system.def) system screen as the engine shows it.
class MotifScreen
{
public:
    // The Emulator panel's Screen choices, in order.
    enum class Screen
    {
        Menu,
        Versus,
        Continue,
        Victory,
        Win,
        SurvivalResults,
    };

    MotifScreen();
    ~MotifScreen();
    MotifScreen(MotifScreen&&) noexcept;
    MotifScreen& operator=(MotifScreen&&) noexcept;

    bool start(Project& project, const QString& systemDef, const QString& engineDir, Screen screen,
               QString* error = nullptr);
    bool isStarted() const { return m_started; }
    Screen screen() const { return m_screen; }

    void tick(const Runtime::InputState& keys);
    void playSounds(AudioMixer& mixer);
    const BackgroundMusic& music() const { return m_music; }
    QImage render();
    QSize canvasSize() const { return m_size; }

    const QStringList& menuItems() const { return m_items; }
    int menuIndex() const { return m_cursor; }
    int time() const { return m_time; }

    static constexpr int Sounds = -5;

private:
    const Runtime::IniSection* section(const QString& name) const;
    const FightScreen::Font* font(int number) const;
    QString textOf(const QString& path) const;
    void startBackground(const QString& name);
    // fontSpec is "n, bank, align".
    void drawText(QPainter& painter, const QString& fontSpec, QPointF at, const QString& text);
    void drawSprite(QPainter& painter, const SpriteDocument* sprites, int group, int number, QPointF at, int facing,
                    QPointF scale);
    void playSound(const QString& spec);

    bool m_started = false;
    Screen m_screen = Screen::Menu;
    Project* m_project = nullptr;
    QSize m_size{320, 240};
    QString m_text;
    std::shared_ptr<const SpriteDocument> m_sprites;
    std::shared_ptr<const SoundDocument> m_sounds;
    QMap<int, FightScreen::Font> m_fonts;
    AirDocument m_animations;
    BgDocument m_bg;
    std::unique_ptr<BgSimulator> m_bgSimulator;
    std::unique_ptr<BgRenderer> m_bgRenderer;
    struct Ini;
    std::unique_ptr<Ini> m_ini;
    std::shared_ptr<const SpriteDocument> m_character;
    QString m_characterName;
    double m_characterScale = 1;
    BgDocument m_stage;
    std::shared_ptr<const SpriteDocument> m_stageSprites;
    AirDocument m_stageAnimations;
    std::unique_ptr<BgSimulator> m_stageSimulator;
    std::unique_ptr<BgRenderer> m_stageRenderer;
    float m_stageZOffset = 200;
    QHash<QPair<const FightScreen::Font*, int>, QImage> m_fontImages;
    int m_time = 0;
    QStringList m_items;
    QStringList m_itemKeys;
    int m_cursor = 0;
    int m_scroll = 0;
    bool m_yes = true;
    Runtime::InputState m_lastKeys;
    QStringList m_pendingSounds;
    QHash<QString, std::shared_ptr<const AudioMixer::Sound>> m_soundCache;
    BackgroundMusic m_music;
};

} // namespace OFS::Mugen
