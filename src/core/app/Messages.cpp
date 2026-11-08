#include "core/app/Messages.h"

#include "core/settings/Config.h"

#include <QApplication>
#include <QCheckBox>
#include <QEvent>
#include <QMessageBox>
#include <QTextDocument>

namespace OFS {

namespace {

// A box gets Polish before it lays out its text the first time, so nothing of the text is read
// as HTML.
class PlainTextMessages : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Polish) {
            if (auto* box = qobject_cast<QMessageBox*>(watched); box && box->textFormat() == Qt::AutoText)
                box->setTextFormat(Qt::PlainText);
        }
        return false;
    }
};

} // namespace

void showPlainTextMessages(QApplication& app)
{
    const QString name = QStringLiteral("ofsPlainTextMessages");
    if (app.findChild<QObject*>(name, Qt::FindDirectChildrenOnly))
        return;
    auto* filter = new PlainTextMessages(&app);
    filter->setObjectName(name);
    app.installEventFilter(filter);
}

QString asPlainText(const QString& text)
{
    return Qt::mightBeRichText(text) ? Qt::convertFromPlainText(text, Qt::WhiteSpaceNormal) : text;
}

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

bool confirmDelete(QWidget* parent, Config& config, const QString& title, const QString& text)
{
    const QString key = QStringLiteral("confirm_delete");
    if (!config.boolValue(key, true))
        return true;
    QMessageBox box(QMessageBox::Question, title, text, QMessageBox::Yes | QMessageBox::No, parent);
    box.setDefaultButton(QMessageBox::Yes);
    auto* dontAsk = new QCheckBox(QCoreApplication::translate("OFS::Messages", "Don't show this again"), &box);
    box.setCheckBox(dontAsk);
    const bool yes = box.exec() == QMessageBox::Yes;
    if (dontAsk->isChecked())
        config.setValue(key, false);
    return yes;
}

} // namespace OFS
