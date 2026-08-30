#pragma once

#include "core/CoreExport.h"
#include "core/bg/BgSimulator.h"

#include <QHash>
#include <QImage>
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QSizeF>

#include <optional>

namespace OFS {

class AirDocument;
class SpriteDocument;

struct OFS_CORE_EXPORT BgDrawItem
{
    int element = -1;
    QImage image;
    QPointF topLeft;
    double opacity = 1.0;
    QPainter::CompositionMode mode = QPainter::CompositionMode_SourceOver;
    std::optional<QRectF> clip;
};

struct OFS_CORE_EXPORT BgView
{
    QSizeF screen{320, 240}; // localcoord
    QPointF camera;          // 0,0 shows the start positions
    QRectF cover;            // area endless tiles fill, in screen units. Empty: the screen
    bool windows = true;
    int layer = -1;
};

class OFS_CORE_EXPORT BgRenderer
{
public:
    BgRenderer(const SpriteDocument* sprites, const AirDocument* animations);

    QList<BgDrawItem> render(const BgSimulator& simulator, const BgView& view);
    void clearCache() { m_cache.clear(); }

    // Premultiplied ARGB sprite image and its axis. Index 0 is transparent when `masked`.
    QImage sprite(int group, int number, bool masked, QPoint* axis);

    static QImage project(const QImage& source, double topLeft, double topWidth, double bottomLeft, double bottomWidth,
                          int height, int* x);

private:
    const SpriteDocument* m_sprites;
    const AirDocument* m_animations;
    QHash<qint64, std::pair<QImage, QPoint>> m_cache;
};

} // namespace OFS
