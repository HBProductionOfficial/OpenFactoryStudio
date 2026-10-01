# libarchive (BSD-2-Clause), built static with only what reading needs.
include(FetchContent)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(BUILD_SHARED_LIBS OFF)
foreach(feature IN ITEMS MBEDTLS NETTLE OPENSSL LIBB2 LZ4 LZO LZMA ZSTD ZLIB BZip2 LIBXML2 EXPAT WIN32_XMLLITE
                         PCREPOSIX PCRE2POSIX LIBGCC CNG TAR CPIO CAT UNZIP XATTR ACL ICONV TEST INSTALL WERROR)
    set(ENABLE_${feature} OFF)
endforeach()
set(POSIX_REGEX_LIB NONE CACHE STRING "libarchive: no regular expressions (only the tools use them)" FORCE)
FetchContent_Declare(libarchive
    URL https://github.com/libarchive/libarchive/releases/download/v3.8.1/libarchive-3.8.1.tar.gz
    URL_HASH SHA256=bde832a5e3344dc723cfe9cc37f8e54bde04565bfe6f136bc1bd31ab352e9fab
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(libarchive)
# Users must not declare its functions as DLL imports, because MSVC can't link those.
target_compile_definitions(archive_static INTERFACE LIBARCHIVE_STATIC)

ofs_add_extension_plugin(archive_rar
    SOURCES RarPlugin.cpp archive_rar.json
    LIBS archive_static)
