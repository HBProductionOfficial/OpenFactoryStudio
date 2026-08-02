include(GNUInstallDirs)

if(WIN32)
    set(OFS_INSTALL_BINDIR ".")
    set(OFS_INSTALL_LIBDIR ".")
    set(OFS_INSTALL_PLUGINDIR ".")
    set(OFS_INSTALL_DATADIR "data")
    set(OFS_INSTALL_DOCDIR ".")
else()
    set(OFS_INSTALL_BINDIR "${CMAKE_INSTALL_BINDIR}")
    set(OFS_INSTALL_LIBDIR "${CMAKE_INSTALL_LIBDIR}/openfactorystudio")
    set(OFS_INSTALL_PLUGINDIR "${CMAKE_INSTALL_LIBDIR}/openfactorystudio")
    set(OFS_INSTALL_DATADIR "${CMAKE_INSTALL_DATADIR}/openfactorystudio")
    set(OFS_INSTALL_DOCDIR "${CMAKE_INSTALL_DOCDIR}")
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}/openfactorystudio")
endif()

install(TARGETS ofs ofscore ofsformats ofslua
    BUNDLE DESTINATION .
    RUNTIME DESTINATION "${OFS_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${OFS_INSTALL_LIBDIR}")

get_property(_ofs_plugins GLOBAL PROPERTY OFS_ENGINE_PLUGINS)
foreach(plugin IN LISTS _ofs_plugins)
    string(REGEX REPLACE "^ofs_" "" engine "${plugin}")
    install(TARGETS ${plugin} LIBRARY DESTINATION "${OFS_INSTALL_PLUGINDIR}/engines/${engine}")
endforeach()

install(DIRECTORY "${PROJECT_SOURCE_DIR}/data/" DESTINATION "${OFS_INSTALL_DATADIR}")
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_SOURCE_DIR}/Third party notices.pdf"
    DESTINATION "${OFS_INSTALL_DOCDIR}")
