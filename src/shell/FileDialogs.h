#pragma once

#include <QString>

#include <functional>

class QWidget;

namespace OFS::FileDialogs {

// Same as QFileDialog::getSaveFileName unless a replacement is installed. GUI tests use that to answer without a native
// dialog.
QString getSaveFileName(QWidget* parent, const QString& caption, const QString& dir, const QString& filter);

using SavePrompt = std::function<QString(const QString& caption, const QString& dir, const QString& filter)>;
void setSavePromptOverride(SavePrompt prompt);

} // namespace OFS::FileDialogs
