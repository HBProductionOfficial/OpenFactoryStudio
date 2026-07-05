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

function(ofs_add_test)
    cmake_parse_arguments(ARG "GUI;CATCH2" "NAME" "SOURCES;LIBS;LABELS" ${ARGN})
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

    # An empty ENVIRONMENT_MODIFICATION would swallow the next keyword, so pass it only when set.
    string(REPLACE ";" "\;" labels_esc "${labels}")
    string(REPLACE ";" "\;" env_esc "${env}")
    string(REPLACE ";" "\;" envmod_esc "${envmod}")
    set(props LABELS "${labels_esc}" ENVIRONMENT "${env_esc}")
    if(envmod)
        list(APPEND props ENVIRONMENT_MODIFICATION "${envmod_esc}")
    endif()

    if(ARG_CATCH2)
        include(Catch)
        set(dlpaths "")
        if(OFS_RUNTIME_DIRS)
            set(dlpaths DL_PATHS ${OFS_RUNTIME_DIRS})
        endif()
        catch_discover_tests(${target}
            WORKING_DIRECTORY "${OFS_OUTPUT_DIR}"
            TEST_PREFIX "${ARG_NAME}:"
            PROPERTIES ${props}
            DISCOVERY_MODE PRE_TEST
            ${dlpaths})
    else()
        add_test(NAME ${ARG_NAME} COMMAND ${target} WORKING_DIRECTORY "${OFS_OUTPUT_DIR}")
        set_tests_properties(${ARG_NAME} PROPERTIES ${props})
    endif()
endfunction()

# Copies data/ into bin/data on every build so the build tree runs with the latest data.
function(ofs_stage_data)
    add_custom_target(ofs_data ALL
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
                "${PROJECT_SOURCE_DIR}/data" "${OFS_DATA_OUTPUT_DIR}"
        COMMENT "Staging data directory"
        VERBATIM)
endfunction()
