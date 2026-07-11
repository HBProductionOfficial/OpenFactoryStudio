#pragma once

#include "core/CoreExport.h"

#include <QColor>
#include <QList>
#include <QString>

namespace OFS {

class ColorSet;

// The color roles edited in Options > Colors, in the order the list shows them.
struct OFS_CORE_EXPORT ColorRole
{
    QString key;
    const char* caption;
    bool text; // Bold and Italic apply
    QColor fallback;

    QString displayName() const;

    static const QList<ColorRole>& all();
    static const ColorRole* find(const QString& roleKey);

    static ColorSet complete(const ColorSet& set);
};

} // namespace OFS
