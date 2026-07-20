#pragma once

#include <QDialog>

class QCheckBox;
class QPlainTextEdit;

namespace OFS {

// Edit > Find/Replace (multi-line): finds text that spans several lines in the current code editor.
class MultiLineFindDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MultiLineFindDialog(QWidget* parent = nullptr);

    QString findText() const;
    void setFindText(const QString& text);
    QString replaceText() const;
    bool isCaseSensitive() const;

Q_SIGNALS:
    void findRequested(bool backward);
    void replaceRequested(bool backward);
    void replaceAllRequested();

private:
    QPlainTextEdit* m_find;
    QCheckBox* m_case;
    QPlainTextEdit* m_replace;
};

} // namespace OFS
