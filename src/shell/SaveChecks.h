#pragma once

class QWidget;

namespace OFS {

class Config;
class ProjectFile;

// Checks before writing a sprite or sound file. Returns whether the save may go on.
// - "Can't save empty sprite/sound list" refuses an empty file because M.U.G.E.N cannot open it.
// - "Can't save 8-bit sprites without palette" refuses such a file. Unticked, it offers a gray palette instead.
bool confirmSave(ProjectFile* file, Config& config, QWidget* parent);

} // namespace OFS
