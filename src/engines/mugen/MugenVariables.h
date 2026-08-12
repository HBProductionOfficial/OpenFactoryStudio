#pragma once

#include "core/code/VariableUsage.h"

namespace OFS::Mugen {

// Finds the variables each StateDef of a CNS/ST text writes, through VarSet, VarAdd, VarRandom,
// ParentVarSet, ParentVarAdd or a "var(n) := ..." assignment. Reads, sysvar/sysfvar and slots past
// 60 integer and 40 float ones are left out.
VariableUsage scanVariables(const QString& text);

} // namespace OFS::Mugen
