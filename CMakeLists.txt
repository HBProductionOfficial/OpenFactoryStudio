cmake_minimum_required(VERSION 3.26...3.31)

project(OpenFactoryStudio
    VERSION 0.0.1
    DESCRIPTION "Open Factory Studio - character, stage and screenpack editor for M.U.G.E.N"
    HOMEPAGE_URL "https://github.com/HBProductionOfficial/OpenFactoryStudio"
    LANGUAGES C CXX)

set(OFS_DISPLAY_NAME "Open Factory Studio")
set(OFS_ORGANIZATION "Open Factory Studio")
set(OFS_REPOSITORY "HBProductionOfficial/OpenFactoryStudio")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(OfsOptions)
include(OfsWarnings)
include(OfsOutput)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets Svg Network Concurrent Core5Compat)
qt_standard_project_setup(REQUIRES 6.8)

include(OfsDependencies)
include(OfsHelpers)

add_subdirectory(src)
ofs_stage_data()

if(OFS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(OFS_BUILD_TOOLS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/CMakeLists.txt")
    add_subdirectory(tools)
endif()

include(OfsInstall)
include(OfsDeploy)
