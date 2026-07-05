#pragma once

#include <QtCore/qglobal.h>

#if defined(OFS_CORE_LIBRARY)
#  define OFS_CORE_EXPORT Q_DECL_EXPORT
#else
#  define OFS_CORE_EXPORT Q_DECL_IMPORT
#endif
