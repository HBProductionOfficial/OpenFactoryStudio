# The build tree uses the install layout so the program runs from the build folder:
#   bin/ofs(.exe), bin/*.dll|*.so, bin/engines/<name>/, bin/extensions/, bin/data/
set(OFS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OFS_OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OFS_OUTPUT_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
foreach(cfg IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg} "${OFS_OUTPUT_DIR}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${cfg} "${OFS_OUTPUT_DIR}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${cfg} "${CMAKE_BINARY_DIR}/lib")
endforeach()

set(OFS_DATA_OUTPUT_DIR "${OFS_OUTPUT_DIR}/data")

if(APPLE)
    set(CMAKE_BUILD_RPATH "@loader_path;@loader_path/../..")
elseif(UNIX)
    set(CMAKE_BUILD_RPATH "$ORIGIN;$ORIGIN/../..")
endif()
