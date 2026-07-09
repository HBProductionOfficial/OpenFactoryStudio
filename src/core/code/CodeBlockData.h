#pragma once

#include "core/CoreExport.h"

#include <QString>
#include <QTextBlock>
#include <QTextBlockUserData>

namespace OFS {

class OFS_CORE_EXPORT CodeBlockData : public QTextBlockUserData
{
public:
    QString header;
    bool isHeader = false;
    bool folded = false;

    static CodeBlockData* get(const QTextBlock& block) { return static_cast<CodeBlockData*>(block.userData()); }
    static CodeBlockData* ensure(QTextBlock block);
};

} // namespace OFS
