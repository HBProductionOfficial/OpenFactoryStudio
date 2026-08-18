#include "shell/FileDialogs.h"

#include <QFileDialog>

namespace OFS::FileDialogs {

namespace {
SavePrompt& savePromptOverride()
{
    static SavePrompt prompt;
    return prompt;
}

SavePrompt& openPromptOverride()
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

QString getOpenFileName(QWidget* parent, const QString& caption, const QString& dir, const QString& filter)
{
    if (const SavePrompt& prompt = openPromptOverride())
        return prompt(caption, dir, filter);
    return QFileDialog::getOpenFileName(parent, caption, dir, filter);
}

QStringList getOpenFileNames(QWidget* parent, const QString& caption, const QString& dir, const QString& filter)
{
    if (const SavePrompt& prompt = openPromptOverride()) {
        const QString answer = prompt(caption, dir, filter);
        return answer.isEmpty() ? QStringList() : answer.split(QLatin1Char('\n'));
    }
    return QFileDialog::getOpenFileNames(parent, caption, dir, filter);
}

void setOpenPromptOverride(SavePrompt prompt)
{
    openPromptOverride() = std::move(prompt);
}

} // namespace OFS::FileDialogs
