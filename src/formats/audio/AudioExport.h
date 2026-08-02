#pragma once

#if defined(_WIN32)
#  if defined(OFS_AUDIO_LIBRARY)
#    define OFS_AUDIO_EXPORT __declspec(dllexport)
#  else
#    define OFS_AUDIO_EXPORT __declspec(dllimport)
#  endif
#else
#  define OFS_AUDIO_EXPORT __attribute__((visibility("default")))
#endif
