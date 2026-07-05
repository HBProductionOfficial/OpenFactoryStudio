#pragma once

#if defined(_WIN32)
#  if defined(OFS_FORMATS_LIBRARY)
#    define OFS_FORMATS_EXPORT __declspec(dllexport)
#  else
#    define OFS_FORMATS_EXPORT __declspec(dllimport)
#  endif
#else
#  define OFS_FORMATS_EXPORT __attribute__((visibility("default")))
#endif
