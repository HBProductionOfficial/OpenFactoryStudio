#pragma once

#include "core/CoreExport.h"

#include <QString>

class QApplication;
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

// Asks unless Options > General > Request confirmation on delete is off. True when the delete goes ahead.
OFS_CORE_EXPORT bool confirmDelete(QWidget* parent, Config& config, const QString& title, const QString& text);

// From now on message boxes show their text as plain text. The text often holds names from game
// files and archives, and Qt shows text that looks like HTML as rich text, which loads the images
// it names, even from another computer.
OFS_CORE_EXPORT void showPlainTextMessages(QApplication& app);

// The text for a tooltip or label, so it shows as written even when it looks like HTML. Tooltips
// can't be fixed afterwards like message boxes, so text from files goes through this first.
OFS_CORE_EXPORT QString asPlainText(const QString& text);

} // namespace OFS
