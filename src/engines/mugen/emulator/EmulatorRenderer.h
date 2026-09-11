#pragma once

#include "engines/mugen/emulator/FightScreen.h"

#include <QHash>
#include <QImage>
#include <QPoint>
#include <QSize>

#include <memory>

class QPainter;

namespace OFS {
class BgRenderer;
class SpriteDocument;
} // namespace OFS

namespace OFS::Mugen {

class EmulatorSession;

// Draws the Emulator's match like the engine shows it on its 320x240 screen.
class EmulatorRenderer
{
public:
    EmulatorRenderer();
    ~EmulatorRenderer();

    static constexpr QSize Screen{320, 240};

    QImage render(const EmulatorSession& session, bool debugInfo = false);
    // Call after sprites are edited.
    void clearCache();

private:
    struct Sprite
    {
        QImage image;
        QPoint axis;
    };
    struct SpriteKey
    {
        int group = 0;
        int number = 0;
        const Formats::Palette* palette = nullptr;
        bool operator==(const SpriteKey&) const = default;
        friend size_t qHash(const SpriteKey& k, size_t seed = 0)
        {
            return qHashMulti(seed, k.group, k.number, k.palette);
        }
    };
    const Sprite* sprite(const SpriteDocument* sprites, int group, int number,
                         const Formats::Palette* palette = nullptr);
    const QImage& fontImage(const FightScreen::Font& font, int bank);

    QHash<const SpriteDocument*, QHash<SpriteKey, Sprite>> m_cache;
    QHash<QPair<const FightScreen::Font*, int>, QImage> m_fonts;
    std::unique_ptr<BgRenderer> m_stage;
    const void* m_stageKey = nullptr;
};

} // namespace OFS::Mugen
