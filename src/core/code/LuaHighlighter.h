#pragma once

#include "core/CoreExport.h"
#include "core/theme/ColorSet.h"

#include <QSyntaxHighlighter>
#include <QTextCharFormat>

namespace OFS {

class OFS_CORE_EXPORT LuaHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit LuaHighlighter(QTextDocument* document);

    void setColorSet(const ColorSet& colors);

protected:
    void highlightBlock(const QString& text) override;

private:
    QTextCharFormat m_keyword;
    QTextCharFormat m_number;
    QTextCharFormat m_string;
    QTextCharFormat m_comment;
    QTextCharFormat m_api;
};

} // namespace OFS
