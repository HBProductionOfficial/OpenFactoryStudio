#pragma once

#include <QDialog>

namespace OFS {

class CodeEditor;
class SyntaxDatabase;

// The "State controller" dialog used to write or change a controller template.
class StateControllerDialog : public QDialog
{
    Q_OBJECT

public:
    StateControllerDialog(const SyntaxDatabase* syntax, QWidget* parent = nullptr);

    QString text() const;
    void setText(const QString& text);
    CodeEditor* editor() const { return m_editor; }

    // The controller type written in the text ("type = ChangeState"), used to name a new template.
    static QString controllerType(const QString& text);

private:
    CodeEditor* m_editor;
};

} // namespace OFS
