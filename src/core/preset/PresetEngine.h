#pragma once

#include "core/CoreExport.h"
#include "core/preset/PresetModel.h"

#include <QString>

namespace OFS::Preset {

// Runs preset scripts (Lua) and collects the interface they describe.
//
// Scripts use the "OFS" table, also named "FF" for existing presets:
//
//   function apply()
//       local tb = OFS.toolBar("main")
//       tb.iconSize = 32
//       tb:addButton("file_open")
//
//       local grid = OFS.gridLayout()
//       grid:addControl("spr_add", 0, 0):icon(24):flat()
//       grid:addControl("spr_sel", 1, 0, 1, -1):horizontal()
//       OFS.panel("sprites").layout = grid
//   end
//   iconset = "Flat-Dark"; stylesheet = "Dark"; colorset = "Dark"
class OFS_CORE_EXPORT PresetEngine
{
public:
    static bool runFile(const QString& path, Model& model);
    static bool runString(const QString& source, const QString& chunkName, Model& model);
};

} // namespace OFS::Preset
