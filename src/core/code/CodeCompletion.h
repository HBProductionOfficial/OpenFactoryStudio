#pragma once

#include "core/CoreExport.h"

#include <QString>
#include <QStringList>

class QTextBlock;

namespace OFS {

class CompletionModels;
class SyntaxDatabase;

struct OFS_CORE_EXPORT CompletionRequest
{
    int start = -1; // -1: no completion
    QStringList words;
    bool isValid() const { return start >= 0 && !words.isEmpty(); }
};

class OFS_CORE_EXPORT CompletionProvider
{
public:
    virtual ~CompletionProvider();
    virtual CompletionRequest complete(const QTextBlock& block, int column) const = 0;
};

// Completion from the syntax database. It offers:
//   - after "type =" in a typed section (state controllers): the type names
//   - after "key =" of a keyword parameter or the expressions of an expkeyword value: its keywords
//   - after "const(" and the other "(" models of completer.xml: their words
//   - from the second letter of a word in an expression value: the trigger names
// Lists are sorted case-insensitively.
class OFS_CORE_EXPORT SyntaxCompletionProvider : public CompletionProvider
{
public:
    SyntaxCompletionProvider(const SyntaxDatabase* syntax, const QString& fileType, const CompletionModels* models);

    CompletionRequest complete(const QTextBlock& block, int column) const override;

private:
    const SyntaxDatabase* m_syntax;
    QString m_fileType;
    const CompletionModels* m_models;
};

} // namespace OFS
