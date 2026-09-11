#pragma once

#include "core/anims/AirDocument.h"
#include "core/audio/AudioMixer.h"
#include "core/bg/BgDocument.h"
#include "core/bg/BgSimulator.h"
#include "core/sounds/SoundDocument.h"
#include "core/sprites/SpriteDocument.h"
#include "engines/mugen/emulator/FightScreen.h"
#include "engines/mugen/runtime/AnimPlayer.h"

#include <QColor>
#include <QImage>
#include <QList>
#include <QMap>
#include <QPointF>
#include <QRect>
#include <QString>

#include <functional>
#include <memory>
#include <optional>

namespace OFS {
class BgRenderer;
class Project;
} // namespace OFS

namespace OFS::Mugen {

// Plays a storyboard like the engine, on a canvas of the storyboard's local size.
class StoryboardPlayer
{
public:
    struct Layer
    {
        int anim = -1;
        QString text;
        int font = -1;
        int bank = 0;
        int align = 1;
        QColor color{255, 255, 255};
        int textDelay = 0;
        QPointF offset;
        int startTime = 0;
        int endTime = -1; // -1: the scene's end
        Runtime::AnimPlayer player;
    };

    struct Sound
    {
        int group = -1;
        int number = 0;
        int startTime = 0;
        float volume = 1;
        int pan = 0; // -127 left to 127 right
    };

    struct Scene
    {
        QString name;
        int endTime = 0;
        int fadeInTime = 0;
        QColor fadeInColor{0, 0, 0};
        int fadeOutTime = 0;
        QColor fadeOutColor{0, 0, 0};
        std::optional<QColor> clearColor;
        QPointF layerAllPos;
        QRect window;
        QMap<int, Layer> layers; // by number: drawn from the lowest
        QList<Sound> sounds;
        std::optional<QString> bgm; // a file to play from the scene's start ("": stop the music)
        bool bgmLoop = false;
        QString bgName;
    };

    StoryboardPlayer();
    ~StoryboardPlayer();
    StoryboardPlayer(StoryboardPlayer&&) noexcept;
    StoryboardPlayer& operator=(StoryboardPlayer&&) noexcept;

    bool start(Project& project, const QString& defPath, const QString& engineDir, QString* error = nullptr);
    bool isStarted() const { return m_started; }
    bool isFinished() const { return m_finished; }

    void tick();
    void playSounds(AudioMixer& mixer);
    QImage render();

    QSize canvasSize() const { return m_size; }
    const QList<Scene>& scenes() const { return m_scenes; }
    int sceneIndex() const { return m_scene; }
    int sceneTime() const { return m_time; }

    static constexpr int Sounds = -3;
    static constexpr int Music = -4;

private:
    void enterScene(int index);
    QString textOf(const QString& path) const;

    bool m_started = false;
    bool m_finished = false;
    Project* m_project = nullptr;
    QString m_path;
    QSize m_size{320, 240};
    std::shared_ptr<const SpriteDocument> m_sprites;
    std::shared_ptr<const SoundDocument> m_sounds;
    QMap<int, FightScreen::Font> m_fonts;
    AirDocument m_animations;
    BgDocument m_bg;
    QList<Scene> m_scenes;
    int m_scene = 0;
    int m_time = 0;
    std::unique_ptr<BgSimulator> m_bgSimulator;
    std::unique_ptr<BgRenderer> m_bgRenderer;
    QImage m_canvas;
    QHash<QPair<const FightScreen::Font*, int>, QImage> m_fontImages;
    QHash<qint64, std::pair<QImage, QPoint>> m_spriteCache;
    QList<Sound> m_pendingSounds;
    std::optional<QString> m_pendingBgm;
    bool m_pendingBgmLoop = false;
    QHash<QPair<int, int>, std::shared_ptr<const AudioMixer::Sound>> m_soundCache;
};

} // namespace OFS::Mugen
