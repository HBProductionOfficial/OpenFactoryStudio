#include "core/app/Messages.h"

#include "core/settings/Config.h"

#include <QApplication>
#include <QCheckBox>
#include <QMessageBox>

namespace OFS {

StoredAnswer storedAnswer(const Config& config, const QString& key)
{
    const QString stored = config.value(key).trimmed();
    if (stored.isEmpty() || stored == QLatin1String("0"))
        return StoredAnswer::Ask;
    return stored == QLatin1String("1") ? StoredAnswer::Always : StoredAnswer::Never;
}

bool askQuestion(QWidget* parent, Config& config, const QString& key, const QString& text)
{
    return askQuestion(parent, config, key, QApplication::applicationDisplayName(), text);
}

bool askQuestion(QWidget* parent, Config& config, const QString& key, const QString& title, const QString& text)
{
    const StoredAnswer stored = storedAnswer(config, key);
    if (stored != StoredAnswer::Ask)
        return stored == StoredAnswer::Always;

    QMessageBox box(QMessageBox::Question, title, text, QMessageBox::Yes | QMessageBox::No, parent);
    box.setDefaultButton(QMessageBox::Yes);
    auto* remember = new QCheckBox(QCoreApplication::translate("OFS::Messages", "Don't show this again"), &box);
    box.setCheckBox(remember);
    const bool yes = box.exec() == QMessageBox::Yes;
    if (remember->isChecked())
        config.setValue(key, yes ? QStringLiteral("1") : QStringLiteral("2"));
    return yes;
}

} // namespace OFS
