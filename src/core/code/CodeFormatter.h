#pragma once

#include "core/CoreExport.h"

#include <QString>
#include <QStringList>

#include <functional>

namespace OFS {

class SyntaxDatabase;

// Edit > Code > Auto-indent selection and Auto-format selection.
//
// Both trim the lines. After the first section header, '=' signs and inline comments are lined
// up with tabs (tab stops every 4 columns): '=' at the first stop after the longest key, comments
// one stop after the longest code. [Data], [Size], [Velocity] and [Movement] are lined up as one
// group. Auto-format also puts one or two spaces after '=' (one when the value had at most one),
// and spaces expressions around binary operators and after commas.
namespace CodeFormatter {

using ReformatPredicate = std::function<bool(const QString& header, const QString& typeValue, const QString& key)>;

enum class Mode
{
    Indent,
    Format
};

OFS_CORE_EXPORT QStringList process(const QStringList& lines, int first, int last, Mode mode,
                                    const ReformatPredicate& reformat = {});

OFS_CORE_EXPORT QString formatExpression(const QString& value);

OFS_CORE_EXPORT ReformatPredicate syntaxPredicate(const SyntaxDatabase* syntax, const QString& fileType);

} // namespace CodeFormatter

} // namespace OFS
