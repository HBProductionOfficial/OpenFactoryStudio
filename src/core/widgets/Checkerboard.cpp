#include "core/widgets/Checkerboard.h"

#include <QHash>
#include <QPainter>
#include <QPixmap>

namespace OFS {

QBrush checkerboard(int cell, const QColor& first, const QColor& second)
{
    static QHash<QString, QBrush> cache;
    const QString key = QStringLiteral("%1/%2/%3").arg(cell).arg(first.rgba()).arg(second.rgba());
    auto found = cache.constFind(key);
    if (found != cache.cend())
        return *found;
    const int size = std::max(1, cell);
    QPixmap tile(2 * size, 2 * size);
    tile.fill(second);
    QPainter p(&tile);
    p.fillRect(0, 0, size, size, first);
    p.fillRect(size, size, size, size, first);
    p.end();
    return *cache.insert(key, QBrush(tile));
}

} // namespace OFS
