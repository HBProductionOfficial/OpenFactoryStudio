# ofscore: the application framework shared by the shell and every engine plugin.
configure_file(Version.h.in "${PROJECT_BINARY_DIR}/src/core/Version.h" @ONLY)

add_library(ofscore SHARED
    CoreExport.h
    "${PROJECT_BINARY_DIR}/src/core/Version.h"
    app/Paths.h
    app/Paths.cpp
    actions/ActionRegistry.h
    actions/ActionRegistry.cpp
    engine/Engine.h
    engine/Engine.cpp
    engine/EngineManager.h
    engine/EngineManager.cpp
    preset/ControlRegistry.h
    preset/ControlRegistry.cpp
    preset/PresetBuilder.h
    preset/PresetBuilder.cpp
    preset/PresetModel.h
    preset/PresetEngine.h
    preset/PresetEngine.cpp
    settings/Config.h
    settings/Config.cpp
    settings/History.h
    settings/History.cpp
    settings/ShortcutSettings.h
    settings/ShortcutSettings.cpp
    settings/XmlRecordFile.h
    settings/XmlRecordFile.cpp
    theme/ColorSet.h
    theme/ColorSet.cpp
    theme/IconSet.h
    theme/IconSet.cpp
    theme/StyleSheet.h
    theme/StyleSheet.cpp
)
add_library(OFS::core ALIAS ofscore)
ofs_target_defaults(ofscore)
target_compile_definitions(ofscore PRIVATE OFS_CORE_LIBRARY)
set_target_properties(ofscore PROPERTIES PREFIX "")
target_link_libraries(ofscore
    PUBLIC
        Qt6::Core Qt6::Gui Qt6::Widgets
        OFS::formats
    PRIVATE
        OFS::sol2)
