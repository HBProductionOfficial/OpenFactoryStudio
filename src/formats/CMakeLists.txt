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
)
add_library(OFS::formats ALIAS ofsformats)
ofs_target_defaults(ofsformats)
target_compile_definitions(ofsformats PRIVATE OFS_FORMATS_LIBRARY)
set_target_properties(ofsformats PROPERTIES PREFIX "" AUTOMOC OFF CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
