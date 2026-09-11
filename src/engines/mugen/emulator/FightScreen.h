#pragma once

#include "core/anims/AirDocument.h"
#include "core/sounds/SoundDocument.h"
#include "core/sprites/SpriteDocument.h"
#include "engines/mugen/runtime/AnimPlayer.h"
#include "formats/fnt/FntDocument.h"

#include <QHash>
#include <QImage>
#include <QList>
#include <QMap>
#include <QPair>
#include <QPointF>
#include <QRectF>
#include <QString>

#include <array>
#include <functional>
#include <memory>
#include <optional>

class QPainter;

namespace OFS::Mugen {

namespace Runtime {
class Character;
class Match;
struct IniSection;
} // namespace Runtime

// The engine's fight screen (data/fight.def), run along the match and drawn on the 320x240 screen.
class FightScreen
{
public:
    struct Font
    {
        // FNT v1 (one image of all glyphs), or FNT v2: a bitmap font (glyphs are sprites 0,<code>, colour
        // banks are palettes 0,<bank>) or a TrueType font.
        enum class Kind
        {
            Fnt,
            Bitmap,
            TrueType,
        };
        Kind kind = Kind::Fnt;
        std::shared_ptr<const SpriteDocument> sprites;
        QString family;
        Formats::Image image;
        Formats::FntInfo info;
        QHash<int, QPair<int, int>> glyphs;
        int bankColors = 0;
        int banks = 1;
        int textWidth(const QString& text) const;
        Formats::Palette palette(int bank) const;
        static std::optional<Font> load(const QString& path);
        QImage render(int bank) const;
        // align 1 starts at `pos`, 0 centres on it, -1 ends there.
        void draw(QPainter& painter, const QImage& image, QPointF pos, int align, QPointF scale, const QString& text,
                  int bank = 0) const;
    };

    struct Element
    {
        int anim = -1;
        int sprGroup = -1;
        int sprNumber = 0;
        int font = -1;
        int bank = 0;
        int align = 0;
        QString text;
        QPointF offset;
        int facing = 1;
        int vfacing = 1;
        QPointF scale{1, 1};
        int layer = 0;
        int displayTime = -2; // negative: as long as the animation plays
        int sndGroup = -1;
        int sndNumber = 0;
        Runtime::AnimPlayer player;
        int count = -1;

        bool hasAnim() const { return anim >= 0 || sprGroup >= 0; }
        bool hasText() const { return font >= 0 && !text.isEmpty(); }
    };

    struct Bar
    {
        QPointF pos;
        Element bg0;
        Element bg1;
        Element mid;
        Element front;
        Element counter;
        int range[2] = {0, 0};
        std::array<QPair<int, int>, 3> levelSounds{{{-1, 0}, {-1, 0}, {-1, 0}}};
        float top = 1;
        float midFill = 1;
        float midMin = 1;
        float old = 1;
        int midTime = 0;
        int level = 0;
    };

    struct Face
    {
        QPointF pos;
        Element bg;
        Element face; // a sprite of the character's own (9000,0)
    };

    struct Combo
    {
        QPointF pos;
        float startX = 0;
        Element counter;
        bool shake = false;
        Element text;
        int displayTime = 90;
        int shown = 0;
        float slide = 0;
        int rest = 0;
        int shakeTime = 0;
    };

    struct WinIcon
    {
        QPointF pos;
        QPointF iconOffset;
        Element counter;
        std::array<Element, 8> icons; // n, s, h, throw, c, t, suicide, perfect
    };

    struct Side
    {
        const Runtime::Character* character = nullptr;
        const SpriteDocument* sprites = nullptr;
        QString name;
    };

    struct Item
    {
        const SpriteDocument* sprites = nullptr;
        int group = -1;
        int number = 0;
        const Font* font = nullptr;
        int bank = 0;
        int align = 0;
        QString text;
        QPointF pos;
        bool flipH = false;
        bool flipV = false;
        QPointF scale{1, 1};
        AirTransparency transparency;
        std::optional<QRectF> clip;
        int layer = 0;
    };

    struct Sound
    {
        int group = 0;
        int number = 0;
    };

    FightScreen();
    ~FightScreen();

    bool load(const QString& engineDir, const QString& fightDef = QString(),
              const std::function<QString(const QString&)>& text = {});
    bool isLoaded() const { return m_loaded; }
    void setSkipCalls(bool skip) { m_skipCalls = skip; }
    // round.time, then the engine's 60 ticks before the Fight call.
    int callTime() const;

    // 1 is a tick every 60th of a second. Slower for slow.time ticks after a KO.
    float gameSpeed(const Runtime::Match& match) const;

    void reset();
    void step(const Runtime::Match& match, const std::array<Side, 2>& sides);
    const QList<Sound>& sounds() const { return m_sounds; }
    QList<Item> items(const Runtime::Match& match, const std::array<Side, 2>& sides) const;

    const SpriteDocument* sprites() const { return m_sprites.get(); }
    const SoundDocument* soundFile() const { return m_soundFile.get(); }
    const AirDocument& animations() const { return m_animations; }
    const Font* font(int number) const;
    const Bar& lifebar(int side) const { return m_life[side == 1 ? 1 : 0]; }
    const Bar& powerbar(int side) const { return m_power[side == 1 ? 1 : 0]; }
    const Combo& combo(int side) const { return m_combo[side == 1 ? 1 : 0]; }

private:
    Element readElement(const Runtime::IniSection* section, const QString& prefix) const;
    Bar readBar(const Runtime::IniSection* section, const QString& prefix) const;
    void start(Element& e) const;
    void stepBar(Bar& bar, float fill, bool gettingHit, bool lagging);
    void play(int group, int number);
    void play(const Element& e) { play(e.sndGroup, e.sndNumber); }
    void addItems(QList<Item>& out, const Element& e, QPointF at, const SpriteDocument* sprites = nullptr,
                  std::optional<QRectF> clip = std::nullopt, const QString& text = QString()) const;
    QRectF fillRect(const Bar& bar, float fill) const;
    bool over(const Element& e) const;

    bool m_loaded = false;
    bool m_skipCalls = false;
    std::shared_ptr<SpriteDocument> m_sprites;
    std::shared_ptr<SoundDocument> m_soundFile;
    AirDocument m_animations;
    QMap<int, Font> m_fonts;
    std::array<Bar, 2> m_life;
    std::array<Bar, 2> m_power;
    std::array<Face, 2> m_face;
    std::array<QPointF, 2> m_namePos;
    std::array<Element, 2> m_name;
    QPointF m_timePos;
    Element m_timeBg;
    Element m_timeCounter;
    int m_framesPerCount = 60;
    std::array<Combo, 2> m_combo;
    std::array<WinIcon, 2> m_winIcon;
    int m_useIconUpTo = 4;

    QPointF m_roundPos;
    int m_roundTime = 0;
    int m_roundSndTime = 0;
    Element m_roundDefault;
    QList<Element> m_rounds;
    int m_fightTime = 0;
    int m_fightSndTime = 0;
    Element m_fight;
    int m_koTime = 0;
    int m_koSndTime = 0;
    Element m_ko;
    Element m_dko;
    Element m_to;
    int m_winTime = 0;
    int m_winSndTime = 0;
    Element m_win;
    Element m_win2;
    Element m_draw;
    int m_overWaitTime = 45;
    int m_slowTime = 60;
    float m_slowSpeed = 0.25f;

    Element* m_roundShown = nullptr;
    int m_roundPhase = 0; // 0 waiting, 1 shown, 2 done
    int m_fightPhase = 0;
    int m_koPhase = 0;
    int m_winPhase = 0;
    Element* m_koShown = nullptr;
    Element* m_winShown = nullptr;
    QList<Sound> m_sounds;
};

} // namespace OFS::Mugen
