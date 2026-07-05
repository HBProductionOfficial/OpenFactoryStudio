# Compile settings for every first-party target, added by ofs_target_defaults().

add_library(ofs_project_options INTERFACE)
add_library(OFS::project_options ALIAS ofs_project_options)

target_compile_definitions(ofs_project_options INTERFACE
    QT_NO_KEYWORDS
    QT_USE_QSTRINGBUILDER
    QT_NO_CAST_FROM_BYTEARRAY
    QT_NO_URL_CAST_FROM_STRING
    QT_DISABLE_DEPRECATED_UP_TO=0x060800
    $<$<PLATFORM_ID:Windows>:NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE>)

if(MSVC)
    target_compile_options(ofs_project_options INTERFACE /W4 /utf-8 /permissive- /Zc:__cplusplus /Zc:preprocessor
        /wd4251 /wd4275)
    if(OFS_WERROR)
        target_compile_options(ofs_project_options INTERFACE /WX)
    endif()
else()
    target_compile_options(ofs_project_options INTERFACE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
    if(OFS_WERROR)
        target_compile_options(ofs_project_options INTERFACE -Werror)
    endif()
    if(OFS_SANITIZERS)
        target_compile_options(ofs_project_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ofs_project_options INTERFACE -fsanitize=address,undefined)
    endif()
endif()
