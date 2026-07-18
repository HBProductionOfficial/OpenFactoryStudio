add_library(ofsformats SHARED
    FormatsExport.h
    util/ByteReader.h
    util/ByteWriter.h
    util/Bytes.h
    util/Crc.h
    util/Crc.cpp
    util/FileIo.h
    util/FileIo.cpp
    util/Result.h
    image/Color.h
    image/Image.h
    image/Image.cpp
    image/ImageOps.h
    image/ImageOps.cpp
    codecs/DecodeMode.h
    codecs/Lz5.h
    codecs/Lz5.cpp
    codecs/Pcx.h
    codecs/Pcx.cpp
    codecs/Png.h
    codecs/Png.cpp
    codecs/Rle5.h
    codecs/Rle5.cpp
    codecs/Rle8.h
    codecs/Rle8.cpp
    palette/PaletteFormats.h
    palette/PaletteFormats.cpp
)
add_library(OFS::formats ALIAS ofsformats)
ofs_target_defaults(ofsformats)
target_link_libraries(ofsformats PRIVATE ofs_lodepng)
target_compile_definitions(ofsformats PRIVATE OFS_FORMATS_LIBRARY)
set_target_properties(ofsformats PROPERTIES PREFIX "" AUTOMOC OFF CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
