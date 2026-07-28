#include "shell/DocumentationPopup.h"

#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

namespace OFS {

DocumentationPopup::DocumentationPopup(QWidget* parent)
    : QFrame(parent, Qt::Popup)
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
    resize(520, 360);
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
    QPoint pos = globalPos + QPoint(8, 8);
    if (const QScreen* screen = QGuiApplication::screenAt(globalPos)) {
        const QRect area = screen->availableGeometry();
        if (pos.x() + popup->width() > area.right())
            pos.setX(qMax(area.left(), area.right() - popup->width()));
        if (pos.y() + popup->height() > area.bottom())
            pos.setY(qMax(area.top(), globalPos.y() - popup->height() - 8));
    }
    popup->move(pos);
    popup->show();
    return popup;
}

} // namespace OFS
