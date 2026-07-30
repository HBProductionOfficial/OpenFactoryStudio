#include "shell/DocumentationPopup.h"

#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QScreen>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

namespace OFS {

DocumentationPopup::DocumentationPopup(QWidget* parent)
    : QFrame(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_browser(new QTextBrowser(this))
{
    setObjectName(QStringLiteral("docPopup"));
    setAttribute(Qt::WA_DeleteOnClose);
    setFrameShape(QFrame::StyledPanel);
    m_browser->setObjectName(QStringLiteral("docBrowser"));
    m_browser->setOpenExternalLinks(true);
    m_browser->setFrameShape(QFrame::NoFrame);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->addWidget(m_browser);
    resize(640, 480);
}

bool DocumentationPopup::event(QEvent* event)
{
    if (event->type() == QEvent::WindowDeactivate)
        close();
    return QFrame::event(event);
}

void DocumentationPopup::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QFrame::keyPressEvent(event);
}

bool DocumentationPopup::loadPage(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    m_browser->document()->setBaseUrl(QUrl::fromLocalFile(QFileInfo(filePath).absolutePath() + QLatin1Char('/')));
    m_browser->setHtml(QString::fromUtf8(file.readAll()));
    return true;
}

DocumentationPopup* DocumentationPopup::showPage(const QString& filePath, const QPoint& globalPos, QWidget* parent)
{
    auto* popup = new DocumentationPopup(parent);
    if (!popup->loadPage(filePath)) {
        delete popup;
        return nullptr;
    }
    QPoint pos = globalPos;
    if (const QScreen* screen = QGuiApplication::screenAt(globalPos)) {
        const QRect area = screen->availableGeometry();
        pos.setX(qBound(area.left(), pos.x(), qMax(area.left(), area.right() + 1 - popup->width())));
        pos.setY(qBound(area.top(), pos.y(), qMax(area.top(), area.bottom() + 1 - popup->height())));
    }
    popup->move(pos);
    popup->show();
    popup->raise();
    popup->activateWindow();
    return popup;
}

} // namespace OFS
