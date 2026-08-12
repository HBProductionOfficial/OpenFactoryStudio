#pragma once

#include "core/CoreExport.h"
#include "core/code/CodeOutline.h"

#include <QList>
#include <QString>

namespace OFS {

struct OFS_CORE_EXPORT VariableUser
{
    QString number;
    QString name; // the comment line above the StateDef, empty when there is none
    QString icon;
    int line = -1;

    // "810 - Kung Fu Throw - Throwing the opponent", or "810" without a name.
    QString caption() const;
};

struct OFS_CORE_EXPORT VariableUsage
{
    QList<QList<VariableUser>> integers;
    QList<QList<VariableUser>> floats;

    bool isEmpty() const { return integers.isEmpty() && floats.isEmpty(); }
};

OFS_CORE_EXPORT Outline variableOutline(const VariableUsage& usage);

struct OFS_CORE_EXPORT VariableLogOptions
{
    enum class Format
    {
        Number,
        Name,
        NumberAndName,
    };
    Format format = Format::Number;
    bool onlyUsed = true;
    bool onePerLine = false;
    bool floats = true;
    bool integers = true;
};

// The comment block inserted by Insert variable usage log (LF line ends), for example:
//   ;---------------------- Integer variable list ----------------------
//
//   ; var(2):= 810
//   ;var(11):= 9205, 9206
//
OFS_CORE_EXPORT QString variableLog(const VariableUsage& usage, const VariableLogOptions& options);

} // namespace OFS
