#pragma once

#include "core/CoreExport.h"
#include "core/syntax/Issue.h"
#include "core/syntax/SyntaxDatabase.h"

#include <QString>
#include <QStringList>

#include <functional>

namespace OFS {

// Checks expression values. The default only checks that parentheses and quotes are balanced.
class OFS_CORE_EXPORT ExpressionChecker
{
public:
    virtual ~ExpressionChecker();

    struct Context
    {
        QString fileType;
        QString section;
        QString blockType;
        QString paramType;
        QString version;
        QStringList list;
        int line = 0;
    };

    virtual void check(const QString& text, int offset, const Context& context, IssueList& issues) const;
};

class OFS_CORE_EXPORT SyntaxValidator
{
public:
    SyntaxValidator(const SyntaxDatabase& syntax, const QString& fileType, const QString& version);

    void setExpressionChecker(const ExpressionChecker* checker) { m_checker = checker; }
    // Case-insensitive, '*' wildcards.
    void setBuiltInSections(const QStringList& sections) { m_builtIn = sections; }
    // e.g. "trigger*".
    void setRepeatableParams(const QStringList& params) { m_repeatable = params; }
    // e.g. triggerall, trigger1..n, persistent and ignorehitpause for state controllers.
    void setTypedSectionParams(const QList<SyntaxParam>& params) { m_typedParams = params; }
    // Storyboards name their background sections after the scene's bg.name.
    void setHeaderMapper(std::function<QString(const QString&)> mapper) { m_headerMapper = std::move(mapper); }
    // Version names for messages ("1.0" as "M.U.G.E.N 1.0"). The id by default.
    void setVersionNamer(std::function<QString(const QString&)> namer) { m_versionNamer = std::move(namer); }

    IssueList validate(const QString& text) const;

    static int errorCount(const IssueList& issues);

private:
    void checkValue(const SyntaxParam& param, const QString& value, int offset, int line, const QString& section,
                    const QString& blockType, IssueList& issues) const;
    bool isBuiltIn(const QString& header) const;

    const SyntaxDatabase& m_syntax;
    QString m_fileType;
    QString m_version;
    const ExpressionChecker* m_checker = nullptr;
    QStringList m_builtIn;
    QStringList m_repeatable;
    QList<SyntaxParam> m_typedParams;
    std::function<QString(const QString&)> m_headerMapper;
    std::function<QString(const QString&)> m_versionNamer;
};

} // namespace OFS
