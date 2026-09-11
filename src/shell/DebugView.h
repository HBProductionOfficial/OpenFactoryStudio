#pragma once

#include "core/debug/DebugTypes.h"

class QTreeView;

namespace OFS::DebugView {

// Values update in place while the rows stay the same. Otherwise the tree is rebuilt and keeps what was expanded.
void showVariables(QTreeView* tree, const QList<Debug::Variable>& variables);
void showWatch(QTreeView* tree, const QList<Debug::Variable>& rows);
void showStack(QTreeView* tree, const QList<Debug::StackFrame>& frames);
QString currentPath(const QTreeView* tree);

} // namespace OFS::DebugView
