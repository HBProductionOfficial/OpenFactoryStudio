# Audio codecs, resampling and the Sounds editor's edits. No Qt.
add_library(ofsaudio SHARED
    AudioExport.h
    SampleBuffer.h
    SampleBuffer.cpp
    AudioCodec.h
    AudioCodec.cpp
    CodecInternal.h
    WavCodec.cpp
    VorbisCodec.cpp
    FlacCodec.cpp
    Mp3Codec.cpp
    Resampler.h
    Resampler.cpp
    AudioEdit.h
    AudioEdit.cpp
    WaveformPeaks.h
    WaveformPeaks.cpp
)
add_library(OFS::audio ALIAS ofsaudio)
ofs_target_defaults(ofsaudio)
target_link_libraries(ofsaudio
    PUBLIC OFS::formats
    PRIVATE ofs_audio_decoders ofs_samplerate ofs_vorbis ofs_flac)
target_compile_definitions(ofsaudio PRIVATE OFS_AUDIO_LIBRARY)
set_target_properties(ofsaudio PROPERTIES PREFIX "" AUTOMOC OFF CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
