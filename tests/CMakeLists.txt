# Test labels:
#   unit    fast, self-contained tests (default CI set)
#   gui     widget tests, run with the offscreen platform plugin
#   corpus  tests over local MUGEN data (OFS_CORPUS_KFM / OFS_CORPUS_MUGEN); skipped when unset
#   oracle  tests that run Elecbyte's command-line tools against our output (Windows only)

ofs_add_test(NAME formats_crc CATCH2
    SOURCES formats/CrcTest.cpp
    LIBS OFS::formats)

set(OFS_FORMATS_FIXTURES fixtures/FixtureFactory.h fixtures/FixtureFactory.cpp)

ofs_add_test(NAME formats_util CATCH2
    SOURCES formats/UtilTest.cpp formats/ImageTest.cpp formats/ResizeTest.cpp ${OFS_FORMATS_FIXTURES}
    LIBS OFS::formats)

ofs_add_test(NAME formats_codecs CATCH2
    SOURCES formats/CodecTest.cpp formats/GifTest.cpp formats/BmpTest.cpp ${OFS_FORMATS_FIXTURES}
    LIBS OFS::formats)

ofs_add_test(NAME formats_documents CATCH2
    SOURCES
        formats/MakerTest.cpp
        formats/PaletteFormatsTest.cpp
        formats/SffTest.cpp
        formats/SndWavFntTest.cpp
        formats/TruncationTest.cpp
        ${OFS_FORMATS_FIXTURES}
    LIBS OFS::formats)

ofs_add_test(NAME formats_corpus CATCH2 LABELS corpus TIMEOUT 3600
    SOURCES formats/CorpusTest.cpp formats/TestEnv.h
    LIBS OFS::formats)

ofs_add_test(NAME formats_oracle CATCH2 LABELS oracle TIMEOUT 900
    SOURCES formats/OracleTest.cpp formats/MakerOracleTest.cpp formats/OracleTools.h formats/TestEnv.h
        ${OFS_FORMATS_FIXTURES}
    LIBS OFS::formats)

ofs_add_test(NAME formats_audio CATCH2
    SOURCES
        audio/AudioTestUtil.h
        audio/SampleBufferTest.cpp
        audio/AudioCodecTest.cpp
        audio/ResamplerTest.cpp
        audio/AudioEditTest.cpp
        audio/WaveformPeaksTest.cpp
    LIBS OFS::audio)

ofs_add_test(NAME formats_audio_corpus CATCH2 LABELS corpus TIMEOUT 3600
    SOURCES audio/AudioCorpusTest.cpp formats/TestEnv.h
    LIBS OFS::audio)

ofs_add_test(NAME core_sprites
    SOURCES core/SpriteDocumentTest.cpp ${OFS_FORMATS_FIXTURES}
    LIBS OFS::core)
ofs_add_test(NAME core_image_document GUI
    SOURCES core/ImageDocumentTest.cpp
    LIBS OFS::core)
ofs_add_test(NAME core_air_document GUI
    SOURCES core/AirDocumentTest.cpp
    LIBS OFS::core)
ofs_add_test(NAME core_sound_file
    SOURCES core/SoundFileTest.cpp
    LIBS OFS::core)
ofs_add_test(NAME core_waveform_view GUI
    SOURCES core/WaveformViewTest.cpp
    LIBS OFS::core)
ofs_add_test(NAME core_palette_edits
    SOURCES core/PaletteEditsTest.cpp
    LIBS OFS::core)
ofs_add_test(NAME core_bg_document
    SOURCES core/BgDocumentTest.cpp
    LIBS OFS::core)
ofs_add_test(NAME core_translations
    SOURCES core/TranslationsTest.cpp
    LIBS OFS::core)
ofs_add_test(NAME core_zip_archive
    SOURCES core/ZipArchiveTest.cpp
    LIBS OFS::core ofs_miniz)
ofs_add_test(NAME core_bg_edits
    SOURCES core/BgEditsTest.cpp
    LIBS OFS::core)
ofs_add_test(NAME core_bg_renderer GUI
    SOURCES core/BgRendererTest.cpp
    LIBS OFS::core)
ofs_add_test(NAME core_palette_ops
    SOURCES core/PaletteOpsTest.cpp
    LIBS OFS::core)
ofs_add_test(NAME core_palette_widget GUI
    SOURCES core/PaletteWidgetTest.cpp
    LIBS OFS::core)
ofs_add_test(NAME core_image_canvas GUI
    SOURCES core/ImageCanvasTest.cpp
    LIBS OFS::core)
ofs_add_test(NAME core_graphics_view GUI
    SOURCES core/GraphicsViewTest.cpp
    LIBS OFS::core)
ofs_add_test(NAME core_sprite_edits
    SOURCES core/SpriteEditsTest.cpp ${OFS_FORMATS_FIXTURES}
    LIBS OFS::core)

foreach(test IN ITEMS formats_util formats_codecs formats_documents formats_corpus formats_oracle formats_audio
                      formats_audio_corpus core_sprites core_sprite_edits)
    target_include_directories(test_${test} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
endforeach()

ofs_add_test(NAME core_audio
    SOURCES core/AudioPlayerTest.cpp
    LIBS OFS::core)

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

ofs_add_test(NAME core_code GUI
    SOURCES core/CodeTest.cpp
    LIBS OFS::core)

ofs_add_test(NAME core_formatter
    SOURCES core/FormatterTest.cpp
    LIBS OFS::core)

ofs_add_test(NAME core_performance GUI LABELS perf TIMEOUT 600
    SOURCES core/PerformanceTest.cpp
    LIBS OFS::core OFS::mugen)
add_dependencies(test_core_performance ofs_data)

ofs_add_test(NAME core_preset_coverage GUI
    SOURCES core/PresetCoverageTest.cpp
    LIBS OFS::core)

ofs_add_test(NAME mugen_project GUI
    SOURCES mugen/MugenProjectTest.cpp
    LIBS OFS::mugen)
add_dependencies(test_mugen_project ofs_data)

ofs_add_test(NAME mugen_project_templates GUI
    SOURCES mugen/ProjectTemplateTest.cpp
    LIBS OFS::shell OFS::mugen)
add_dependencies(test_mugen_project_templates ofs_data)

ofs_add_test(NAME core_settings_importer
    SOURCES core/SettingsImporterTest.cpp
    LIBS OFS::core)

ofs_add_test(NAME core_tools GUI
    SOURCES core/ToolsTest.cpp
    LIBS OFS::core)

ofs_add_test(NAME gui_mainwindow GUI LABELS gui
    SOURCES gui/MainWindowTest.cpp
    LIBS OFS::shell)

ofs_add_test(NAME gui_options GUI LABELS gui
    SOURCES gui/OptionsDialogTest.cpp
    LIBS OFS::shell)

ofs_add_test(NAME gui_dialogs GUI LABELS gui
    SOURCES gui/DialogsTest.cpp
    LIBS OFS::shell)

ofs_add_test(NAME gui_image_editor GUI LABELS gui
    SOURCES gui/ImageEditorTest.cpp
    LIBS OFS::shell)

ofs_add_test(NAME gui_engine_run GUI LABELS oracle TIMEOUT 300
    SOURCES gui/EngineRunTest.cpp
    LIBS OFS::shell)

# The engine-loading tests need the plugins next to the test executables.
get_property(_ofs_plugins GLOBAL PROPERTY OFS_ENGINE_PLUGINS)
add_dependencies(test_core_engines ofs_data ${_ofs_plugins})
add_dependencies(test_gui_mainwindow ofs_data ${_ofs_plugins})
add_dependencies(test_gui_engine_run ofs_data ${_ofs_plugins})
add_dependencies(test_core_preset_coverage ofs_data ${_ofs_plugins})

ofs_add_test(NAME shell_update_checker
    SOURCES gui/UpdateCheckerTest.cpp
    LIBS OFS::shell)
add_dependencies(test_gui_options ofs_data)

ofs_add_test(NAME gui_first_run GUI LABELS gui
    SOURCES gui/FirstRunDialogTest.cpp
    LIBS OFS::shell)
add_dependencies(test_gui_first_run ofs_data)

ofs_add_test(NAME mugen_syntax
    SOURCES mugen/MugenSyntaxTest.cpp
    LIBS OFS::mugen)

ofs_add_test(NAME mugen_run GUI
    SOURCES mugen/MugenRunTest.cpp
    LIBS OFS::mugen)

ofs_add_test(NAME mugen_tools GUI
    SOURCES mugen/MugenToolsTest.cpp
    LIBS OFS::mugen)

add_executable(fake_mugen stubs/fake_mugen.cpp)
ofs_target_defaults(fake_mugen)
add_dependencies(test_gui_mainwindow fake_mugen)


find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME data_mugen_syntax
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/validate_syntax.py")
    set_tests_properties(data_mugen_syntax PROPERTIES LABELS unit)
    add_test(NAME data_mugen_sctrls
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/gen_sctrl_templates.py" --check)
    set_tests_properties(data_mugen_sctrls PROPERTIES LABELS unit)
    add_test(NAME data_mugen_syntax_corpus
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/validate_syntax.py"
                --corpus-env --strict)
    set_tests_properties(data_mugen_syntax_corpus PROPERTIES LABELS corpus SKIP_RETURN_CODE 77)
else()
    message(STATUS "Python 3 not found: the engine data tests are disabled")
endif()

add_test(NAME mugen_validation_corpus COMMAND ofs-validate --corpus-env --quiet WORKING_DIRECTORY "${OFS_OUTPUT_DIR}")
set_tests_properties(mugen_validation_corpus PROPERTIES LABELS corpus SKIP_RETURN_CODE 77)
if(OFS_RUNTIME_DIRS)
    set(_ofs_envmod "")
    foreach(dir IN LISTS OFS_RUNTIME_DIRS)
        list(APPEND _ofs_envmod "PATH=path_list_prepend:${dir}")
    endforeach()
    set_tests_properties(mugen_validation_corpus PROPERTIES ENVIRONMENT_MODIFICATION "${_ofs_envmod}")
endif()

ofs_add_test(NAME mugen_runtime_expr
    SOURCES mugen/RuntimeExprTest.cpp
    LIBS OFS::mugen_runtime)

ofs_add_test(NAME mugen_project_backup GUI
    SOURCES mugen/ProjectBackupTest.cpp
    LIBS OFS::mugen)

ofs_add_test(NAME mugen_project_watcher GUI
    SOURCES mugen/ProjectWatcherTest.cpp
    LIBS OFS::mugen)

ofs_add_test(NAME core_organize
    SOURCES core/OrganizeTest.cpp
    LIBS OFS::core)

ofs_add_test(NAME mugen_html_export GUI
    SOURCES mugen/HtmlExportTest.cpp
    LIBS OFS::mugen)
add_dependencies(test_mugen_html_export ofs_data)

ofs_add_test(NAME mugen_runtime_character
    SOURCES mugen/RuntimeCharacterTest.cpp
    LIBS OFS::mugen_runtime)

ofs_add_test(NAME mugen_runtime_states
    SOURCES mugen/RuntimeStateTest.cpp
    LIBS OFS::mugen_runtime)
