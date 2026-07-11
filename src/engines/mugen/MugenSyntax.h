#pragma once

#include "core/syntax/SyntaxValidator.h"

namespace OFS::Mugen {

// Checks expressions, hit attributes, hit and guard flags, and command input sequences.
class MugenExpressionChecker : public ExpressionChecker
{
public:
    explicit MugenExpressionChecker(const SyntaxDatabase* syntax);

    void check(const QString& text, int offset, const Context& context, IssueList& issues) const override;

    static bool isValidKeySequence(const QString& text, QString* problem = nullptr);

private:
    void checkHitAttr(const QString& text, int offset, int line, IssueList& issues) const;
    void checkFlags(const QString& text, int offset, int line, const QString& allowed, IssueList& issues) const;

    const SyntaxDatabase* m_syntax;
};

IssueList validateMugenCode(const SyntaxDatabase& syntax, const QString& fileType, const QString& version,
                            const QString& text);

} // namespace OFS::Mugen
