#pragma once

#include "core/syntax/Issue.h"

#include <QList>
#include <QString>

#include <memory>

namespace OFS {
class SyntaxDatabase;
}

namespace OFS::Mugen {

// Syntax tree of a M.U.G.E.N expression. Positions refer to the parsed line.
struct ExprNode
{
    enum class Kind
    {
        Number,
        String,
        Trigger,
        Unary,
        Binary,
        Interval, // op ("=" / "!="), bounds "[", "(", children: value, low, high
        Redirect, // name (parent, root, helper, target, partner, enemy, enemynear, playerid),
                  // children: [argument], expression
        Special,  // trigger with a special comparison form (AnimElem = n, op t; Command = "x"; ...)
        Keyword,
    };

    Kind kind = Kind::Number;
    QString text;
    int start = 0;
    int length = 0;
    QList<std::shared_ptr<ExprNode>> children;
};

using ExprPtr = std::shared_ptr<ExprNode>;

// Parses M.U.G.E.N expressions and checks them against the syntax database's trigger list.
class ExpressionParser
{
public:
    struct Token
    {
        enum class Kind
        {
            Number,
            String,
            Identifier,
            Operator,
            Comma,
            OpenParen,
            CloseParen,
            OpenBracket,
            CloseBracket,
            End,
        };
        Kind kind = Kind::End;
        QString text;
        int start = 0;
        int length = 0;
    };

    ExpressionParser(const SyntaxDatabase* syntax, const QString& version);

    ExprPtr parse(const QString& text, int offset, int line, IssueList& issues);

    static QList<QPair<QString, int>> splitTopLevel(const QString& text, int offset);

private:
    ExprPtr parseExpression(int minPrecedence);
    ExprPtr parseUnary();
    ExprPtr parsePrimary();
    ExprPtr parseTrigger(const Token& name);
    ExprPtr parseComparisonTail(ExprPtr left, const Token& op);
    bool parseSpecialForm(const QString& type, const Token& name, ExprPtr& node);

    const Token& peek(int ahead = 0) const;
    Token next();
    bool atEnd() const;
    bool accept(const QString& text);
    void error(int start, int length, const QString& message);

    const SyntaxDatabase* m_syntax;
    QString m_version;
    QString m_text;
    int m_offset = 0;
    int m_line = 0;
    QList<Token> m_tokens;
    int m_pos = 0;
    IssueList* m_issues = nullptr;
    bool m_failed = false;
};

} // namespace OFS::Mugen
