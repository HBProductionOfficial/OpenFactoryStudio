#include "shell/BusyWindow.h"

#include <QCoreApplication>

namespace OFS {

BusyWindow::BusyWindow(const QString& title, QWidget* parent)
    : QProgressDialog(title, QString(), 0, 0, parent)
{
    setObjectName(QStringLiteral("busyWindow"));
    setWindowTitle(title);
    setCancelButton(nullptr);
    setWindowModality(Qt::NonModal);
    setMinimumDuration(0);
    setAutoClose(false);
    setAutoReset(false);
    resize(315, 117);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowFlag(Qt::WindowDoesNotAcceptFocus);
    show();
    // Paint it now because the work blocks the event loop.
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

BusyWindow::~BusyWindow()
{
    close();
}

} // namespace OFS
