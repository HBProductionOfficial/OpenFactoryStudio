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
    SOURCES formats/UtilTest.cpp formats/ImageTest.cpp ${OFS_FORMATS_FIXTURES}
    LIBS OFS::formats)

ofs_add_test(NAME formats_codecs CATCH2
    SOURCES formats/CodecTest.cpp ${OFS_FORMATS_FIXTURES}
    LIBS OFS::formats)

ofs_add_test(NAME formats_documents CATCH2
    SOURCES
        formats/PaletteFormatsTest.cpp
        formats/SffTest.cpp
        ${OFS_FORMATS_FIXTURES}
    LIBS OFS::formats)

foreach(test IN ITEMS formats_util formats_codecs formats_documents)
    target_include_directories(test_${test} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
endforeach()

ofs_add_test(NAME core_engines
    SOURCES core/EngineManagerTest.cpp
    LIBS OFS::core)

ofs_add_test(NAME gui_mainwindow GUI LABELS gui
    SOURCES gui/MainWindowTest.cpp
    LIBS OFS::shell)

# The engine-loading tests need the plugins next to the test executables.
get_property(_ofs_plugins GLOBAL PROPERTY OFS_ENGINE_PLUGINS)
add_dependencies(test_core_engines ofs_data ${_ofs_plugins})
add_dependencies(test_gui_mainwindow ofs_data ${_ofs_plugins})
