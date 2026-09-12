# Helper functions shared by all first-party targets.

# Folders Windows needs on PATH to run programs from the build tree (Qt, MinGW runtime).
set(OFS_RUNTIME_DIRS "")
if(WIN32)
    get_filename_component(_ofs_qt_bin "${Qt6_DIR}/../../../bin" ABSOLUTE)
    list(APPEND OFS_RUNTIME_DIRS "${_ofs_qt_bin}")
    if(NOT MSVC)
        get_filename_component(_ofs_cxx_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
        list(APPEND OFS_RUNTIME_DIRS "${_ofs_cxx_bin}")
    endif()
endif()

function(ofs_target_defaults target)
    target_link_libraries(${target} PRIVATE OFS::project_options)
    target_include_directories(${target} PUBLIC
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>"
        "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src>")
endfunction()

# ofs_add_engine_plugin(<name> SOURCES ... [LIBS ...])
function(ofs_add_engine_plugin name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    set(target ofs_${name})
    add_library(${target} MODULE ${ARG_SOURCES})
    ofs_target_defaults(${target})
    target_link_libraries(${target} PRIVATE OFS::core ${ARG_LIBS})
    set(out "${OFS_OUTPUT_DIR}/engines/${name}")
    set_target_properties(${target} PROPERTIES
        PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY "${out}"
        RUNTIME_OUTPUT_DIRECTORY "${out}")
    foreach(cfg IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(${target} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY_${cfg} "${out}"
            RUNTIME_OUTPUT_DIRECTORY_${cfg} "${out}")
    endforeach()
    set_property(GLOBAL APPEND PROPERTY OFS_ENGINE_PLUGINS ${target})
endfunction()

# ofs_add_test(NAME <name> SOURCES ... [LIBS ...] [LABELS ...] [TIMEOUT <seconds>] [GUI] [CATCH2])
function(ofs_add_test)
    cmake_parse_arguments(ARG "GUI;CATCH2" "NAME;TIMEOUT" "SOURCES;LIBS;LABELS" ${ARGN})
    set(target test_${ARG_NAME})
    add_executable(${target} ${ARG_SOURCES})
    ofs_target_defaults(${target})
    target_link_libraries(${target} PRIVATE ${ARG_LIBS})
    if(ARG_CATCH2)
        target_link_libraries(${target} PRIVATE Catch2::Catch2WithMain)
    else()
        target_link_libraries(${target} PRIVATE Qt6::Test)
    endif()

    set(env "OFS_CONFIG_DIR=${CMAKE_BINARY_DIR}/test-config/${ARG_NAME}")
    if(NOT ARG_CATCH2)
        # Without a console, QtTest on Windows logs to the debugger. Keep it in the test output.
        list(APPEND env "QT_FORCE_STDERR_LOGGING=1")
    endif()
    set(envmod "")
    foreach(dir IN LISTS OFS_RUNTIME_DIRS)
        list(APPEND envmod "PATH=path_list_prepend:${dir}")
    endforeach()
    if(ARG_GUI)
        list(APPEND env "QT_QPA_PLATFORM=offscreen")
    endif()
    set(labels ${ARG_LABELS})
    if(NOT labels)
        set(labels unit)
    endif()
    set(extra_properties "")
    if(ARG_TIMEOUT)
        list(APPEND extra_properties TIMEOUT ${ARG_TIMEOUT})
    endif()

    # An empty ENVIRONMENT_MODIFICATION would swallow the next keyword, so pass it only when set.
    string(REPLACE ";" "\;" labels_esc "${labels}")
    string(REPLACE ";" "\;" env_esc "${env}")
    string(REPLACE ";" "\;" envmod_esc "${envmod}")
    set(props LABELS "${labels_esc}" ENVIRONMENT "${env_esc}")
    if(envmod)
        list(APPEND props ENVIRONMENT_MODIFICATION "${envmod_esc}")
    endif()

    if(ARG_CATCH2)
        # catch_discover_tests() flattens PROPERTIES into one list, so a value with several items
        # would shift the rest and break SKIP_RETURN_CODE.
        list(LENGTH env env_items)
        list(LENGTH labels label_items)
        if(env_items GREATER 1 OR label_items GREATER 1)
            message(FATAL_ERROR "ofs_add_test(${ARG_NAME}): Catch2 tests take one label and no GUI flag")
        endif()
        include(Catch)
        set(dlpaths "")
        if(OFS_RUNTIME_DIRS)
            set(dlpaths DL_PATHS ${OFS_RUNTIME_DIRS})
        endif()
        catch_discover_tests(${target}
            WORKING_DIRECTORY "${OFS_OUTPUT_DIR}"
            TEST_PREFIX "${ARG_NAME}:"
            PROPERTIES LABELS "${labels}" ENVIRONMENT "${env}" ${extra_properties}
            DISCOVERY_MODE PRE_TEST
            ${dlpaths})
    else()
        add_test(NAME ${ARG_NAME} COMMAND ${target} WORKING_DIRECTORY "${OFS_OUTPUT_DIR}")
        set_tests_properties(${ARG_NAME} PROPERTIES ${props} ${extra_properties})
    endif()
endfunction()

# Copies data/ into bin/data on every build so the build tree runs with the latest data.
function(ofs_stage_data)
    add_custom_target(ofs_data ALL
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
                "${PROJECT_SOURCE_DIR}/data" "${OFS_DATA_OUTPUT_DIR}"
        COMMENT "Staging data directory"
        VERBATIM)

    # data/languages/<code>/*.ts -> .qm. The update_translations target refreshes the .ts files.
    file(GLOB _ofs_ts "${PROJECT_SOURCE_DIR}/data/languages/*/*.ts")
    if(NOT _ofs_ts)
        return()
    endif()
    find_package(Qt6 QUIET COMPONENTS LinguistTools)
    if(NOT TARGET Qt6::lrelease)
        message(STATUS "Qt LinguistTools not found: translations are not compiled")
        return()
    endif()
    set(_ofs_qm)
    foreach(_ts IN LISTS _ofs_ts)
        file(RELATIVE_PATH _rel "${PROJECT_SOURCE_DIR}/data" "${_ts}")
        string(REGEX REPLACE "\\.ts$" ".qm" _rel_qm "${_rel}")
        set(_qm "${OFS_DATA_OUTPUT_DIR}/${_rel_qm}")
        add_custom_command(OUTPUT "${_qm}"
            COMMAND Qt6::lrelease -silent "${_ts}" -qm "${_qm}"
            DEPENDS "${_ts}"
            VERBATIM)
        list(APPEND _ofs_qm "${_qm}")
    endforeach()
    add_custom_target(ofs_translations ALL DEPENDS ${_ofs_qm})
    add_dependencies(ofs_translations ofs_data)
    if(TARGET Qt6::lupdate)
        add_custom_target(update_translations
            COMMAND Qt6::lupdate -I "${PROJECT_SOURCE_DIR}/src" -locations relative -no-obsolete
                    "${PROJECT_SOURCE_DIR}/src" -ts ${_ofs_ts}
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            COMMENT "Updating the translation sources"
            VERBATIM)
    endif()
endfunction()
