#pragma once

#include <QFrame>

class QTextBrowser;

namespace OFS {

// Reference page shown when Ctrl+clicking a state controller or trigger in a code editor. A frameless window that
// closes when another window is activated or on Esc.
class DocumentationPopup : public QFrame
{
    Q_OBJECT

public:
    explicit DocumentationPopup(QWidget* parent = nullptr);

    bool loadPage(const QString& filePath);
    QTextBrowser* browser() const { return m_browser; }

    // The popup deletes itself when closed. Returns nullptr when the page cannot be read.
    static DocumentationPopup* showPage(const QString& filePath, const QPoint& globalPos, QWidget* parent);

protected:
    bool event(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QTextBrowser* m_browser;
};

} // namespace OFS
