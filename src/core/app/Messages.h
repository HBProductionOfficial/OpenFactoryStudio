#pragma once

#include "core/CoreExport.h"

#include <QString>

class QWidget;

namespace OFS {

class Config;

// A showmsg_* setting (Options > Messages): "0" or empty asks, "1" answers Yes, anything else No.
enum class StoredAnswer
{
    Ask,
    Always,
    Never
};
OFS_CORE_EXPORT StoredAnswer storedAnswer(const Config& config, const QString& key);

// Yes/No question. If the user ticks "Don't show this again", the answer is stored.
OFS_CORE_EXPORT bool askQuestion(QWidget* parent, Config& config, const QString& key, const QString& text);
OFS_CORE_EXPORT bool askQuestion(QWidget* parent, Config& config, const QString& key, const QString& title,
                                 const QString& text);

} // namespace OFS
