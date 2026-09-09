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
    const Sprite* sprite(const SpriteDocument* sprites, int group, int number);
    void drawHud(QPainter& p, const QList<FightScreen::Item>& items);
    const QImage& fontImage(const FightScreen::Font& font, int bank);

    QHash<const SpriteDocument*, QHash<qint64, Sprite>> m_cache;
    QHash<QPair<const FightScreen::Font*, int>, QImage> m_fonts;
    std::unique_ptr<BgRenderer> m_stage;
    const void* m_stageKey = nullptr;
};

} // namespace OFS::Mugen
