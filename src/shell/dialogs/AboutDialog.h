#pragma once

#include <QDialog>

class QTabWidget;
class QTextBrowser;

namespace OFS {

// Help > About... The GPL text is embedded. Third party notices.pdf comes with the program.
class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);

    static void showAbout(QWidget* parent = nullptr);

    QTabWidget* tabs() const { return m_tabs; }
    QTextBrowser* legalText() const { return m_legal; }
    QTextBrowser* licensesText() const { return m_licenses; }

    // "0.4.0 (Qt 6.8.3)"
    static QString versionText();
    // The embedded GPL text, empty when the resource is missing.
    static QString licenseText();

private:
    QTabWidget* m_tabs;
    QTextBrowser* m_legal;
    QTextBrowser* m_licenses;
};

} // namespace OFS
