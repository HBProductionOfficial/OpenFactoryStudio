#pragma once

#include <QHash>
#include <QPair>
#include <QString>

#include <array>

namespace OFS::Mugen::Runtime {

// Positions and velocities in single precision, as the engine keeps them.
struct Vec2
{
    float x = 0;
    float y = 0;
};

// PalFX, AllPalFX, BGPalFX: colours added and multiplied (256 = 1), a sine added over a period,
// inversion and saturation (color 0 grey, 256 unchanged).
struct PalFx
{
    int time = 0;
    std::array<int, 3> add{0, 0, 0};
    std::array<int, 3> mul{256, 256, 256};
    std::array<int, 3> sinAdd{0, 0, 0};
    int sinPeriod = 0;
    bool invertAll = false;
    float color = 256;
    int elapsed = 0;

    bool active() const { return time != 0; }
    void tick()
    {
        if (time > 0)
            --time;
        ++elapsed;
    }
};

struct AfterImage
{
    int time = 0;
    int length = 20;
    int timeGap = 1;
    int frameGap = 4;
    QString trans = QStringLiteral("none");
    int palColor = 256;
    bool palInvertAll = false;
    std::array<int, 3> palBright{30, 30, 30};
    std::array<int, 3> palContrast{120, 220, 220};
    std::array<int, 3> palPostBright{0, 0, 0};
    std::array<int, 3> palAdd{10, 10, 25};
    std::array<float, 3> palMul{0.65f, 0.65f, 0.75f};
};

struct DrawState
{
    Vec2 offset;
    QString trans = QStringLiteral("default"); // none, add, add1, addalpha, sub
    std::array<int, 2> alpha{256, 0};
    bool angleDraw = false;
    float angle = 0;
    Vec2 angleScale{1, 1};
    PalFx palFx;
    AfterImage afterImage;
    QHash<QPair<int, int>, QPair<int, int>> remap;
};

struct EnvShake
{
    int time = 0;
    float freq = 60;
    int ampl = -4;
    float phase = 0;
};

struct EnvColor
{
    int time = 0;
    std::array<int, 3> color{255, 255, 255};
    bool under = false;
};

struct SoundEvent
{
    enum class Kind
    {
        Play,
        Stop,
        Pan,
    };
    Kind kind = Kind::Play;
    int playerId = 0;
    bool common = false;
    int group = 0;
    int index = 0;
    int channel = -1;
    bool lowPriority = false;
    int volume = 0;
    float freqMul = 1;
    bool loop = false;
    float pan = 0;
    bool absolutePan = false;
};

struct SpawnEvent
{
    enum class Kind
    {
        Dust,
        GameAnim,
    };
    Kind kind = Kind::Dust;
    int playerId = 0;
    int anim = 0;
    Vec2 pos; // on the stage, in game units
    bool under = false;
};

} // namespace OFS::Mugen::Runtime
