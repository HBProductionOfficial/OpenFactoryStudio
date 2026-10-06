#include "shell/BusyWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QEvent>
#include <QMessageBox>
#include <QProgressBar>
#include <QWindow>

#include <algorithm>

namespace OFS {

BusyWindow::BusyWindow(const QString& title, QWidget* parent, Focus focus)
    : QProgressDialog(QString(), QString(), 0, 0, parent)
{
    setObjectName(QStringLiteral("busyWindow"));
    setWindowTitle(title);
    setWindowFlag(Qt::WindowContextHelpButtonHint);
    setCancelButton(nullptr);
    auto* bar = new QProgressBar(this);
    bar->setObjectName(QStringLiteral("busyBar"));
    bar->setAlignment(Qt::AlignCenter);
    setBar(bar);
    setWindowModality(Qt::NonModal);
    setMinimumDuration(0);
    setAutoClose(false);
    setAutoReset(false);
    resize(294, 76); // same size as Fighter Factory's window
    if (focus == Focus::Leave) {
        setAttribute(Qt::WA_ShowWithoutActivating);
        setWindowFlag(Qt::WindowDoesNotAcceptFocus);
    }
    show();
    // Paint it now because the work blocks the event loop.
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

BusyWindow::~BusyWindow()
{
    close();
}

void BusyWindow::setSteps(int total)
{
    setRange(0, std::max(1, total));
    setValue(0);
    repaint();
}

void BusyWindow::step()
{
    if (value() < maximum())
        setValue(value() + 1);
    repaint();
}

namespace {

// Adds the (?) help button to each dialog when it is polished. The flag goes on the widget and on its native window,
// which may already exist but is still hidden.
class HelpButtons final : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Polish) {
            auto* dialog = qobject_cast<QDialog*>(watched);
            if (dialog && dialog->isWindow() && !qobject_cast<QMessageBox*>(dialog) &&
                !dialog->windowFlags().testFlag(Qt::WindowContextHelpButtonHint)) {
                const Qt::WindowFlags flags = dialog->windowFlags() | Qt::WindowContextHelpButtonHint;
                dialog->overrideWindowFlags(flags);
                if (QWindow* window = dialog->windowHandle())
                    window->setFlags(flags);
            }
        }
        return false;
    }
};

} // namespace

void installDialogHelpButtons(QApplication& app)
{
    app.installEventFilter(new HelpButtons(&app));
}

} // namespace OFS
