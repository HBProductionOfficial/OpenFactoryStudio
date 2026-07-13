# Test labels:
#   unit    fast, self-contained tests (default CI set)
#   gui     widget tests, run with the offscreen platform plugin
#   corpus  tests over local MUGEN data (OFS_CORPUS_KFM / OFS_CORPUS_MUGEN); skipped when unset
#   oracle  tests that run Elecbyte's command-line tools against our output (Windows only)

ofs_add_test(NAME formats_crc CATCH2
    SOURCES formats/CrcTest.cpp
    LIBS OFS::formats)

ofs_add_test(NAME core_engines
    SOURCES core/EngineManagerTest.cpp
    LIBS OFS::core)

ofs_add_test(NAME core_settings
    SOURCES core/SettingsTest.cpp
    LIBS OFS::core)

ofs_add_test(NAME core_theme GUI
    SOURCES core/ThemeTest.cpp
    LIBS OFS::core)

ofs_add_test(NAME core_theme_data GUI
    SOURCES core/ThemeDataTest.cpp
    LIBS OFS::core)
target_compile_definitions(test_core_theme_data PRIVATE
    OFS_SOURCE_DATA_DIR="${PROJECT_SOURCE_DIR}/data"
    OFS_ICONSET_NAMES="${PROJECT_SOURCE_DIR}/tools/iconsets/required_names.json")

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME iconsets_check
        COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tools/iconsets/check_iconsets.py" --all)
    set_tests_properties(iconsets_check PROPERTIES LABELS unit)
endif()

ofs_add_test(NAME core_preset
    SOURCES core/PresetEngineTest.cpp
    LIBS OFS::core)

ofs_add_test(NAME core_actions_preset GUI
    SOURCES core/ActionsPresetTest.cpp
    LIBS OFS::core)

ofs_add_test(NAME gui_mainwindow GUI LABELS gui
    SOURCES gui/MainWindowTest.cpp
    LIBS OFS::shell)

# The engine-loading tests need the plugins next to the test executables.
get_property(_ofs_plugins GLOBAL PROPERTY OFS_ENGINE_PLUGINS)
add_dependencies(test_core_engines ofs_data ${_ofs_plugins})
add_dependencies(test_gui_mainwindow ofs_data ${_ofs_plugins})
