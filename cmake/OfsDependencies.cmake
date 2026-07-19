# Third-party dependencies. Every download is pinned by version and SHA-256.
# Single-file libraries are vendored under third_party/ (see "Third party notices.pdf").

include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
set(FETCHCONTENT_QUIET ON)

# --- Lua 5.4 (MIT) -----------------------------------------------------------------------------
# A shared library so the core and the plugins use one Lua runtime.
FetchContent_Declare(lua
    URL https://www.lua.org/ftp/lua-5.4.9.tar.gz
    URL_HASH SHA256=2335b6c582a52654f94612bf10d2f4672805d05329aa6568b1d8cd9e5c6fb8e6)
FetchContent_MakeAvailable(lua)

file(GLOB _ofs_lua_sources "${lua_SOURCE_DIR}/src/*.c")
list(FILTER _ofs_lua_sources EXCLUDE REGEX "/(lua|luac)\\.c$")
add_library(ofslua SHARED ${_ofs_lua_sources})
add_library(OFS::lua ALIAS ofslua)
set_target_properties(ofslua PROPERTIES C_VISIBILITY_PRESET default PREFIX "")
target_include_directories(ofslua SYSTEM PUBLIC "$<BUILD_INTERFACE:${lua_SOURCE_DIR}/src>")
target_compile_definitions(ofslua PUBLIC LUA_COMPAT_5_3)
if(WIN32)
    target_compile_definitions(ofslua PUBLIC LUA_BUILD_AS_DLL)
elseif(APPLE)
    target_compile_definitions(ofslua PRIVATE LUA_USE_MACOSX)
else()
    target_compile_definitions(ofslua PRIVATE LUA_USE_LINUX)
    target_link_libraries(ofslua PRIVATE ${CMAKE_DL_LIBS} m)
endif()
if(MSVC)
    target_compile_options(ofslua PRIVATE /W0)
else()
    target_compile_options(ofslua PRIVATE -w)
endif()

# --- sol2 (MIT), header-only C++/Lua binding ---------------------------------------------------
FetchContent_Declare(sol2
    URL https://github.com/ThePhD/sol2/archive/refs/tags/v3.5.0.tar.gz
    URL_HASH SHA256=86c0f6d2836b184a250fc2907091c076bf53c9603dd291eaebade36cc342e13c
    SOURCE_SUBDIR _ofs_no_build)
FetchContent_MakeAvailable(sol2)
add_library(ofs_sol2 INTERFACE)
add_library(OFS::sol2 ALIAS ofs_sol2)
target_include_directories(ofs_sol2 SYSTEM INTERFACE "$<BUILD_INTERFACE:${sol2_SOURCE_DIR}/include>")
target_compile_definitions(ofs_sol2 INTERFACE SOL_ALL_SAFETIES_ON=1)
target_link_libraries(ofs_sol2 INTERFACE ofslua)

# --- LodePNG (zlib license), vendored in third_party/lodepng ----------------------------------
add_library(ofs_lodepng STATIC "${PROJECT_SOURCE_DIR}/third_party/lodepng/lodepng.cpp")
target_include_directories(ofs_lodepng SYSTEM PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/lodepng>")
target_compile_definitions(ofs_lodepng PUBLIC LODEPNG_NO_COMPILE_DISK LODEPNG_NO_COMPILE_CPP)
if(MSVC)
    target_compile_options(ofs_lodepng PRIVATE /W0)
else()
    target_compile_options(ofs_lodepng PRIVATE -w)
endif()

# --- Catch2 v3 (BSL-1.0), unit tests -----------------------------------------------------------
if(OFS_BUILD_TESTS)
    FetchContent_Declare(Catch2
        URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.16.0.tar.gz
        URL_HASH SHA256=0957cae5821b17ce07f0833aaa52b5137643a8382203221f363a8303c109af34)
    set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    find_package(Qt6 REQUIRED COMPONENTS Test)
endif()
