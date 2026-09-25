#include "shell/FileDialogs.h"

#include "core/app/AppContext.h"
#include "core/settings/Config.h"

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

QString getSaveFileName(QWidget* parent, const QString& caption, const QString& dir, const QString& filter,
                        QFileDialog::Options options)
{
    if (const SavePrompt& prompt = savePromptOverride())
        return prompt(caption, dir, filter);
    return QFileDialog::getSaveFileName(parent, caption, dir, filter, nullptr, options);
}

void setSavePromptOverride(SavePrompt prompt)
{
    savePromptOverride() = std::move(prompt);
}

QString getOpenFileName(QWidget* parent, const QString& caption, const QString& dir, const QString& filter,
                        QFileDialog::Options options)
{
    if (const SavePrompt& prompt = openPromptOverride())
        return prompt(caption, dir, filter);
    return QFileDialog::getOpenFileName(parent, caption, dir, filter, nullptr, options | openDialogOptions());
}

QFileDialog::Options openDialogOptions()
{
    const AppContext* context = AppContext::instance();
    if (context && !context->config().boolValue(QStringLiteral("use_native_open"), true))
        return QFileDialog::DontUseNativeDialog;
    return {};
}

QString getExistingDirectory(QWidget* parent, const QString& caption, const QString& dir)
{
    if (const SavePrompt& prompt = openPromptOverride())
        return prompt(caption, dir, QString());
    return QFileDialog::getExistingDirectory(parent, caption, dir, QFileDialog::ShowDirsOnly | openDialogOptions());
}

QStringList getOpenFileNames(QWidget* parent, const QString& caption, const QString& dir, const QString& filter)
{
    if (const SavePrompt& prompt = openPromptOverride()) {
        const QString answer = prompt(caption, dir, filter);
        return answer.isEmpty() ? QStringList() : answer.split(QLatin1Char('\n'));
    }
    return QFileDialog::getOpenFileNames(parent, caption, dir, filter, nullptr, openDialogOptions());
}

void setOpenPromptOverride(SavePrompt prompt)
{
    openPromptOverride() = std::move(prompt);
}

} // namespace OFS::FileDialogs
