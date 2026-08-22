#pragma once

#include "core/CoreExport.h"

#include <QString>

class QWidget;

namespace OFS {

class Config;

OFS_CORE_EXPORT bool askQuestion(QWidget* parent, Config& config, const QString& key, const QString& text);
OFS_CORE_EXPORT bool askQuestion(QWidget* parent, Config& config, const QString& key, const QString& title,
                                 const QString& text);

} // namespace OFS
