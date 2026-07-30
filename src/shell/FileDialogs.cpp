#include "shell/FileDialogs.h"

#include <QFileDialog>

namespace OFS::FileDialogs {

namespace {
SavePrompt& savePromptOverride()
{
    static SavePrompt prompt;
    return prompt;
}
} // namespace

QString getSaveFileName(QWidget* parent, const QString& caption, const QString& dir, const QString& filter)
{
    if (const SavePrompt& prompt = savePromptOverride())
        return prompt(caption, dir, filter);
    return QFileDialog::getSaveFileName(parent, caption, dir, filter);
}

void setSavePromptOverride(SavePrompt prompt)
{
    savePromptOverride() = std::move(prompt);
}

} // namespace OFS::FileDialogs
