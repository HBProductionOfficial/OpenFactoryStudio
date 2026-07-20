#pragma once

#include <QDialog>
#include <QHash>

namespace OFS {

class Snippet;

// Asks for the field values of a snippet before it is inserted.
class SnippetFieldsDialog : public QDialog
{
    Q_OBJECT

public:
    SnippetFieldsDialog(const Snippet& snippet, QWidget* parent = nullptr);

    // Field name -> value to substitute for @name.
    QHash<QString, QString> values() const;

    static bool ask(QWidget* parent, const Snippet& snippet, QHash<QString, QString>* values);

private:
    QHash<QString, QWidget*> m_inputs;
};

} // namespace OFS
