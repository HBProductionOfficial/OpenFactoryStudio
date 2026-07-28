#pragma once

#include <QFrame>

class QTextBrowser;

namespace OFS {

class DocumentationPopup : public QFrame
{
    Q_OBJECT

public:
    explicit DocumentationPopup(QWidget* parent = nullptr);

    bool loadPage(const QString& filePath);
    QTextBrowser* browser() const { return m_browser; }

    // The popup deletes itself when closed. Returns nullptr when the page cannot be read.
    static DocumentationPopup* showPage(const QString& filePath, const QPoint& globalPos, QWidget* parent);

private:
    QTextBrowser* m_browser;
};

} // namespace OFS
