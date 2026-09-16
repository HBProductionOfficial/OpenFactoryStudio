# Developer command-line tools.

add_executable(ofs-sffdump sffdump/main.cpp)
ofs_target_defaults(ofs-sffdump)
target_link_libraries(ofs-sffdump PRIVATE OFS::formats)

# Checks the code files of M.U.G.E.N projects the way the code editors do.
add_executable(ofs-validate validate/main.cpp)
ofs_target_defaults(ofs-validate)
target_link_libraries(ofs-validate PRIVATE OFS::mugen OFS::core)
# Reads the syntax database from the staged data folder.
add_dependencies(ofs-validate ofs_data)

# Screenshots of every editor, dialog and theme: ofs-screenshots --mugen <folder> --out <folder>
add_executable(ofs-screenshots WIN32 screenshots/main.cpp)
ofs_target_defaults(ofs-screenshots)
target_link_libraries(ofs-screenshots PRIVATE OFS::shell OFS::core)
get_property(_ofs_tool_plugins GLOBAL PROPERTY OFS_ENGINE_PLUGINS)
add_dependencies(ofs-screenshots ofs_data ${_ofs_tool_plugins})
