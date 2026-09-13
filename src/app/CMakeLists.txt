set(OFS_APP_SOURCES main.cpp gpu_hints.cpp)
if(WIN32)
    configure_file(ofs.rc.in "${CMAKE_CURRENT_BINARY_DIR}/ofs.rc" @ONLY)
    list(APPEND OFS_APP_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/ofs.rc")
endif()

qt_add_executable(ofs WIN32 MACOSX_BUNDLE ${OFS_APP_SOURCES})
ofs_target_defaults(ofs)
target_link_libraries(ofs PRIVATE OFS::shell OFS::core)
set_target_properties(ofs PROPERTIES
    OUTPUT_NAME ofs
    MACOSX_BUNDLE_BUNDLE_NAME "${OFS_DISPLAY_NAME}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.openfactorystudio.ofs"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")

# Engine and extension plugins and data must be built whenever the application is.
get_property(_ofs_plugins GLOBAL PROPERTY OFS_ENGINE_PLUGINS)
get_property(_ofs_extensions GLOBAL PROPERTY OFS_EXTENSION_PLUGINS)
if(_ofs_plugins OR _ofs_extensions)
    add_dependencies(ofs ${_ofs_plugins} ${_ofs_extensions})
endif()
