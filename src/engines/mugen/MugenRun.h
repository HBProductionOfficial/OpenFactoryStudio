#pragma once

#include "core/engine/Engine.h"

namespace OFS::Mugen {

// Run in engine for M.U.G.E.N: copies the project with its unsaved changes into the engine folder
// under an unused name ("ofs_0", "ofs_1"...) that the run configuration gets as %0. Characters go to
// chars/<name>/, stages to stages/<name>.def and screenpacks to data/<name>/. Nothing that exists is
// overwritten, so a different file at a needed path makes the copy fail.
RunCopy prepareRunCopy(const Project& project, const QString& engineDir);

} // namespace OFS::Mugen
