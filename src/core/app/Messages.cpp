#include "core/app/Messages.h"

#include "core/settings/Config.h"

#include <QApplication>
#include <QCheckBox>
#include <QMessageBox>

namespace OFS {

bool askQuestion(QWidget* parent, Config& config, const QString& key, const QString& text)
{
    const QString stored = config.value(key);
    if (!stored.isEmpty())
        return stored == QLatin1String("1");

    QMessageBox box(QMessageBox::Question, QApplication::applicationDisplayName(), text,
                    QMessageBox::Yes | QMessageBox::No, parent);
    box.setDefaultButton(QMessageBox::Yes);
    auto* remember = new QCheckBox(QCoreApplication::translate("OFS::Messages", "Don't ask again"), &box);
    box.setCheckBox(remember);
    const bool yes = box.exec() == QMessageBox::Yes;
    if (remember->isChecked())
        config.setValue(key, yes ? QStringLiteral("1") : QStringLiteral("2"));
    return yes;
}

} // namespace OFS
