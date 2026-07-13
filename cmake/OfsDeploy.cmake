# deploy-dev copies Qt and the C++ runtime next to bin/ofs.exe. Elsewhere the RPATH finds Qt.
if(WIN32)
    find_program(OFS_WINDEPLOYQT windeployqt HINTS "${_ofs_qt_bin}" NO_DEFAULT_PATH)
    find_program(OFS_WINDEPLOYQT windeployqt)
    if(OFS_WINDEPLOYQT)
        set(_ofs_deploy_cmds
            COMMAND "${OFS_WINDEPLOYQT}" --no-translations --no-system-d3d-compiler --no-compiler-runtime --svg
                    --dir "${OFS_OUTPUT_DIR}" "$<TARGET_FILE:ofs>"
            # the tests run with the offscreen platform plugin
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${_ofs_qt_bin}/../plugins/platforms/qoffscreen$<$<CONFIG:Debug>:$<$<BOOL:${MSVC}>:d>>.dll"
                    "${OFS_OUTPUT_DIR}/platforms/")
        if(NOT MSVC)
            get_filename_component(_ofs_cxx_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
            foreach(dll IN ITEMS libc++.dll libunwind.dll libwinpthread-1.dll libstdc++-6.dll libgcc_s_seh-1.dll)
                if(EXISTS "${_ofs_cxx_bin}/${dll}")
                    list(APPEND _ofs_deploy_cmds
                        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_ofs_cxx_bin}/${dll}" "${OFS_OUTPUT_DIR}")
                endif()
            endforeach()
        endif()
        add_custom_target(deploy-dev ${_ofs_deploy_cmds}
            DEPENDS ofs
            COMMENT "Deploying Qt and runtime libraries into ${OFS_OUTPUT_DIR}"
            VERBATIM)
    endif()
endif()
