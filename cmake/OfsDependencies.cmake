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

# --- miniz (MIT), vendored in third_party/miniz -------------------------------------------------
add_library(ofs_miniz STATIC "${PROJECT_SOURCE_DIR}/third_party/miniz/miniz.c")
target_include_directories(ofs_miniz SYSTEM PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/miniz>")
set_target_properties(ofs_miniz PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(ofs_miniz PRIVATE /W0)
else()
    target_compile_options(ofs_miniz PRIVATE -w)
endif()

# --- hqx (LGPL-2.1-or-later, used under the GPL) -------------------------------------------------
# On Windows, DLL_EXPORT stops its header from marking the functions as DLL imports.
FetchContent_Declare(hqx
    URL https://github.com/grom358/hqx/archive/refs/tags/v1.2.tar.gz
    URL_HASH SHA256=5a44f1745b7fb0321c1d244822d79505df7cf85cfe383ee70b16f5bcd6803396
    SOURCE_SUBDIR _ofs_no_build)
FetchContent_MakeAvailable(hqx)
add_library(ofs_hqx STATIC
    "${hqx_SOURCE_DIR}/src/init.c"
    "${hqx_SOURCE_DIR}/src/hq2x.c"
    "${hqx_SOURCE_DIR}/src/hq3x.c"
    "${hqx_SOURCE_DIR}/src/hq4x.c")
add_library(OFS::hqx ALIAS ofs_hqx)
set_target_properties(ofs_hqx PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(ofs_hqx SYSTEM PUBLIC "$<BUILD_INTERFACE:${hqx_SOURCE_DIR}/src>")
if(WIN32)
    target_compile_definitions(ofs_hqx PRIVATE DLL_EXPORT)
endif()
if(MSVC)
    target_compile_options(ofs_hqx PRIVATE /W0)
else()
    target_compile_options(ofs_hqx PRIVATE -w)
endif()

# --- uchardet (MPL-1.1 / GPL-2.0-or-later / LGPL-2.1-or-later, used under the GPL) ------------
# Release 0.0.8 is vendored because its server refuses automated downloads.
set(uchardet_SOURCE_DIR "${PROJECT_SOURCE_DIR}/third_party/uchardet")
file(GLOB _ofs_uchardet_sources "${uchardet_SOURCE_DIR}/src/*.cpp" "${uchardet_SOURCE_DIR}/src/LangModels/*.cpp")
add_library(ofs_uchardet STATIC ${_ofs_uchardet_sources})
add_library(OFS::uchardet ALIAS ofs_uchardet)
set_target_properties(ofs_uchardet PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(ofs_uchardet SYSTEM PUBLIC "$<BUILD_INTERFACE:${uchardet_SOURCE_DIR}/src>")
target_compile_definitions(ofs_uchardet PRIVATE VERSION="0.0.8")
if(MSVC)
    target_compile_options(ofs_uchardet PRIVATE /W0)
else()
    target_compile_options(ofs_uchardet PRIVATE -w)
endif()

# --- Audio -------------------------------------------------------------------------------------
function(ofs_third_party_defaults target)
    set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W0)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
    else()
        target_compile_options(${target} PRIVATE -w)
    endif()
    if(UNIX)
        target_link_libraries(${target} PRIVATE m)
    endif()
endfunction()

# dr_flac, dr_mp3 (public domain / MIT-0) and stb_vorbis (MIT / public domain). The dr_libs
# implementation file is written into the build tree so the vendored files stay as they are.
set(_ofs_dr_libs_impl "${CMAKE_BINARY_DIR}/third_party/ofs_dr_libs.c")
file(CONFIGURE OUTPUT "${_ofs_dr_libs_impl}" CONTENT [[
/* Written by cmake/OfsDependencies.cmake: compiles the vendored dr_libs decoders. */
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"
]])
add_library(ofs_audio_decoders STATIC "${_ofs_dr_libs_impl}" "${PROJECT_SOURCE_DIR}/third_party/stb_vorbis/stb_vorbis.c")
ofs_third_party_defaults(ofs_audio_decoders)
target_include_directories(ofs_audio_decoders SYSTEM PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/dr_libs>"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/stb_vorbis>")
target_compile_definitions(ofs_audio_decoders PUBLIC DR_FLAC_NO_STDIO DR_MP3_NO_STDIO STB_VORBIS_NO_STDIO)

# --- libsamplerate (BSD-2-Clause), sample rate conversion -------------------------------------
FetchContent_Declare(libsamplerate
    URL https://github.com/libsndfile/libsamplerate/releases/download/0.2.2/libsamplerate-0.2.2.tar.xz
    URL_HASH SHA256=3258da280511d24b49d6b08615bbe824d0cacc9842b0e4caf11c52cf2b043893
    SOURCE_SUBDIR _ofs_no_build)
FetchContent_MakeAvailable(libsamplerate)
add_library(ofs_samplerate STATIC
    "${libsamplerate_SOURCE_DIR}/src/samplerate.c"
    "${libsamplerate_SOURCE_DIR}/src/src_linear.c"
    "${libsamplerate_SOURCE_DIR}/src/src_sinc.c"
    "${libsamplerate_SOURCE_DIR}/src/src_zoh.c")
ofs_third_party_defaults(ofs_samplerate)
target_include_directories(ofs_samplerate SYSTEM PUBLIC "$<BUILD_INTERFACE:${libsamplerate_SOURCE_DIR}/include>")
target_compile_definitions(ofs_samplerate PRIVATE
    PACKAGE="libsamplerate" VERSION="0.2.2" HAVE_STDBOOL_H=1 CPU_CLIPS_POSITIVE=0 CPU_CLIPS_NEGATIVE=0
    ENABLE_SINC_FAST_CONVERTER ENABLE_SINC_MEDIUM_CONVERTER ENABLE_SINC_BEST_CONVERTER)

# --- libogg and libvorbis (BSD-3-Clause), Ogg Vorbis encoding ---------------------------------
# No vorbisfile because stb_vorbis decodes. config_types.h replaces the one libogg's build writes.
FetchContent_Declare(ogg
    URL https://github.com/xiph/ogg/releases/download/v1.3.6/libogg-1.3.6.tar.xz
    URL_HASH SHA256=5c8253428e181840cd20d41f3ca16557a9cc04bad4a3d04cce84808677fa1061
    SOURCE_SUBDIR _ofs_no_build)
FetchContent_Declare(vorbis
    URL https://github.com/xiph/vorbis/releases/download/v1.3.7/libvorbis-1.3.7.tar.xz
    URL_HASH SHA256=b33cc4934322bcbf6efcbacf49e3ca01aadbea4114ec9589d1b1e9d20f72954b
    SOURCE_SUBDIR _ofs_no_build)
FetchContent_MakeAvailable(ogg vorbis)
block(SCOPE_FOR VARIABLES)
    set(INCLUDE_INTTYPES_H 0)
    set(INCLUDE_STDINT_H 1)
    set(INCLUDE_SYS_TYPES_H 0)
    set(SIZE16 int16_t)
    set(USIZE16 uint16_t)
    set(SIZE32 int32_t)
    set(USIZE32 uint32_t)
    set(SIZE64 int64_t)
    set(USIZE64 uint64_t)
    configure_file("${ogg_SOURCE_DIR}/include/ogg/config_types.h.in"
        "${CMAKE_BINARY_DIR}/third_party/ogg/include/ogg/config_types.h" @ONLY)
endblock()
add_library(ofs_ogg STATIC "${ogg_SOURCE_DIR}/src/bitwise.c" "${ogg_SOURCE_DIR}/src/framing.c")
ofs_third_party_defaults(ofs_ogg)
target_include_directories(ofs_ogg SYSTEM PUBLIC
    "$<BUILD_INTERFACE:${ogg_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/third_party/ogg/include>")

set(_ofs_vorbis_sources
    analysis bitrate block codebook envelope floor0 floor1 info lookup lpc lsp mapping0 mdct psy registry res0
    sharedbook smallft synthesis vorbisenc window)
list(TRANSFORM _ofs_vorbis_sources PREPEND "${vorbis_SOURCE_DIR}/lib/")
list(TRANSFORM _ofs_vorbis_sources APPEND ".c")
add_library(ofs_vorbis STATIC ${_ofs_vorbis_sources})
ofs_third_party_defaults(ofs_vorbis)
target_include_directories(ofs_vorbis SYSTEM PUBLIC "$<BUILD_INTERFACE:${vorbis_SOURCE_DIR}/include>")
target_include_directories(ofs_vorbis PRIVATE "${vorbis_SOURCE_DIR}/lib")
target_link_libraries(ofs_vorbis PUBLIC ofs_ogg)

# --- libFLAC (BSD-3-Clause), FLAC encoding ----------------------------------------------------
# The definitions below replace the config.h of libFLAC's own build.
FetchContent_Declare(flac
    URL https://github.com/xiph/flac/releases/download/1.5.0/flac-1.5.0.tar.xz
    URL_HASH SHA256=f2c1c76592a82ffff8413ba3c4a1299b6c7ab06c734dee03fd88630485c2b920
    SOURCE_SUBDIR _ofs_no_build)
FetchContent_MakeAvailable(flac)
set(_ofs_flac_sources
    bitmath bitreader bitwriter cpu crc fixed float format lpc md5 memory metadata_object stream_decoder
    stream_encoder stream_encoder_framing window)
list(TRANSFORM _ofs_flac_sources PREPEND "${flac_SOURCE_DIR}/src/libFLAC/")
list(TRANSFORM _ofs_flac_sources APPEND ".c")
if(WIN32)
    list(APPEND _ofs_flac_sources "${flac_SOURCE_DIR}/src/share/win_utf8_io/win_utf8_io.c")
endif()
add_library(ofs_flac STATIC ${_ofs_flac_sources})
ofs_third_party_defaults(ofs_flac)
target_include_directories(ofs_flac SYSTEM PUBLIC "$<BUILD_INTERFACE:${flac_SOURCE_DIR}/include>")
target_include_directories(ofs_flac PRIVATE "${flac_SOURCE_DIR}/src/libFLAC/include")
target_compile_definitions(ofs_flac
    PUBLIC FLAC__NO_DLL
    PRIVATE PACKAGE_VERSION="1.5.0" FLAC__HAS_OGG=0 FLAC__NO_ASM FLAC__HAS_X86INTRIN=0 FLAC__HAS_NEONINTRIN=0
            FLAC__HAS_A64NEONINTRIN=0 ENABLE_64_BIT_WORDS=0 CPU_IS_BIG_ENDIAN=0 WORDS_BIGENDIAN=0 HAVE_LROUND=1
            HAVE_STDINT_H=1 HAVE_INTTYPES_H=1)
if(NOT MSVC)
    # Every target but MSVC has fseeko. MSVC uses _fseeki64.
    target_compile_definitions(ofs_flac PRIVATE HAVE_FSEEKO=1)
endif()

# --- miniaudio (public domain / MIT-0), vendored in third_party/miniaudio ----------------------
add_library(ofs_miniaudio STATIC "${PROJECT_SOURCE_DIR}/third_party/miniaudio/miniaudio.c")
ofs_third_party_defaults(ofs_miniaudio)
target_include_directories(ofs_miniaudio SYSTEM PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/miniaudio>")
target_compile_definitions(ofs_miniaudio PUBLIC
    MA_NO_DECODING MA_NO_ENCODING MA_NO_GENERATION MA_NO_RESOURCE_MANAGER MA_NO_NODE_GRAPH MA_NO_ENGINE)
if(APPLE)
    target_compile_definitions(ofs_miniaudio PUBLIC MA_NO_RUNTIME_LINKING)
    target_link_libraries(ofs_miniaudio PUBLIC "-framework CoreFoundation" "-framework CoreAudio" "-framework AudioToolbox")
elseif(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(ofs_miniaudio PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
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
