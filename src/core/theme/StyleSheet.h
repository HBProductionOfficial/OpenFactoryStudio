#pragma once

#include "core/CoreExport.h"

#include <QString>

namespace OFS {

// Qt style sheets with two extensions used by the interface styles (data/styles/<name>/stylesheet.css):
//
//  * DPI-scaled numbers: "[N]" is replaced by N multiplied by the interface scale and rounded,
//    e.g. "border: [2]px;" becomes "border: 3px;" at 150%. Works for any property, including
//    "qproperty-leftMargin: [8];".
//  * Relative images: url(panel.png) is resolved against the style's folder.
class OFS_CORE_EXPORT StyleSheet
{
public:
    static QString preprocess(const QString& source, double scale, const QString& baseDir);

    static QString load(const QString& styleDir, double scale);
};

} // namespace OFS
