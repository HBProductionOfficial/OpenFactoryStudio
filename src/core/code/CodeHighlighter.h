#pragma once

#include "core/CoreExport.h"
#include "core/theme/ColorSet.h"

#include <QHash>
#include <QSet>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

#include <array>

namespace OFS {

class SyntaxDatabase;

class OFS_CORE_EXPORT CodeHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    CodeHighlighter(QTextDocument* document, const SyntaxDatabase* syntax, const QString& fileType);

    void setColorSet(const ColorSet& colors);
    void setFileType(const QString& fileType);
    QString fileType() const { return m_fileType; }

    QTextCharFormat format(const QString& role) const { return m_formats.value(role); }

protected:
    void highlightBlock(const QString& text) override;

private:
    struct SectionInfo
    {
        QSet<QString> params;
        QSet<QString> keywords;
        QSet<QString> wildcardParams;
        bool typed = false;
    };
    const SectionInfo& sectionInfo(const QString& header);
    bool isKnownParam(const SectionInfo& info, const QString& key) const;

    enum Role
    {
        Comment,
        Block,
        Keyword,
        Number,
        String,
        Function,
        Constant,
        KeywordValue,
        BlockType,
        Symbol,
        RoleCount
    };

    const SyntaxDatabase* m_syntax;
    QString m_fileType;
    QHash<QString, QTextCharFormat> m_formats;
    std::array<QTextCharFormat, RoleCount> m_roles;
    QHash<QString, SectionInfo> m_sections;
    QHash<QString, QString> m_sectionKeys;
    QSet<QString> m_triggers;
    QSet<QString> m_consts;
};

} // namespace OFS
