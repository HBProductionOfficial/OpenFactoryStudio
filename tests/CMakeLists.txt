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

ofs_add_test(NAME core_preset
    SOURCES core/PresetEngineTest.cpp
    LIBS OFS::core)

ofs_add_test(NAME core_actions_preset GUI
    SOURCES core/ActionsPresetTest.cpp
    LIBS OFS::core)

ofs_add_test(NAME core_preset_coverage GUI
    SOURCES core/PresetCoverageTest.cpp
    LIBS OFS::core)

ofs_add_test(NAME mugen_project
    SOURCES mugen/MugenProjectTest.cpp
    LIBS OFS::mugen)

ofs_add_test(NAME gui_mainwindow GUI LABELS gui
    SOURCES gui/MainWindowTest.cpp
    LIBS OFS::shell)

# The engine-loading tests need the plugins next to the test executables.
get_property(_ofs_plugins GLOBAL PROPERTY OFS_ENGINE_PLUGINS)
add_dependencies(test_core_engines ofs_data ${_ofs_plugins})
add_dependencies(test_gui_mainwindow ofs_data ${_ofs_plugins})
add_dependencies(test_core_preset_coverage ofs_data ${_ofs_plugins})

ofs_add_test(NAME shell_update_checker
    SOURCES gui/UpdateCheckerTest.cpp
    LIBS OFS::shell)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME data_mugen_syntax
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/validate_syntax.py")
    set_tests_properties(data_mugen_syntax PROPERTIES LABELS unit)
    add_test(NAME data_mugen_syntax_corpus
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/validate_syntax.py"
                --corpus-env --strict)
    set_tests_properties(data_mugen_syntax_corpus PROPERTIES LABELS corpus SKIP_RETURN_CODE 77)
else()
    message(STATUS "Python 3 not found: the engine data tests are disabled")
endif()
