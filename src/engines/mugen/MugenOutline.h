#pragma once

#include "core/code/CodeOutline.h"

namespace OFS {
class SyntaxDatabase;
}

namespace OFS::Mugen {

// The code block tree of a M.U.G.E.N text file. A StateDef shows its number and the name from the
// comment above it ("200 - Stand Light Punch"), with its controllers under it. Every [Command] goes
// under one "Commands" entry, and a localised section shows its language ("Quotes [ja]").
Outline mugenOutline(const QString& text, const SyntaxDatabase* syntax = nullptr, const QString& fileType = {});

QString valueTypeIcon(const QString& type);

} // namespace OFS::Mugen
