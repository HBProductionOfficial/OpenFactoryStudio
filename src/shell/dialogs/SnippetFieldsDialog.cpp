#include "shell/dialogs/SnippetFieldsDialog.h"

#include "core/code/Snippet.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

#include <limits>

namespace OFS {

SnippetFieldsDialog::SnippetFieldsDialog(const Snippet& snippet, QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("SnipFieldsDlg"));
    setWindowTitle(snippet.name);

    auto* form = new QFormLayout;
    for (const SnippetField& field : snippet.fields) {
        QWidget* input = nullptr;
        switch (field.type) {
        case SnippetField::Type::Int: {
            auto* spin = new QSpinBox(this);
            spin->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
            input = spin;
            break;
        }
        case SnippetField::Type::Items: {
            auto* combo = new QComboBox(this);
            for (int i = 0; i < field.values.size(); ++i)
                combo->addItem(i < field.valueTitles.size() ? field.valueTitles.at(i) : field.values.at(i),
                               field.values.at(i));
            input = combo;
            break;
        }
        case SnippetField::Type::String:
            input = new QLineEdit(this);
            break;
        }
        input->setObjectName(field.name);
        form->addRow(field.title.isEmpty() ? field.name : field.title, input);
        m_inputs.insert(field.name, input);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

QHash<QString, QString> SnippetFieldsDialog::values() const
{
    QHash<QString, QString> result;
    for (auto it = m_inputs.cbegin(); it != m_inputs.cend(); ++it) {
        if (auto* spin = qobject_cast<QSpinBox*>(it.value()))
            result.insert(it.key(), QString::number(spin->value()));
        else if (auto* combo = qobject_cast<QComboBox*>(it.value()))
            result.insert(it.key(), combo->currentData().toString());
        else if (auto* edit = qobject_cast<QLineEdit*>(it.value()))
            result.insert(it.key(), edit->text());
    }
    return result;
}

bool SnippetFieldsDialog::ask(QWidget* parent, const Snippet& snippet, QHash<QString, QString>* values)
{
    SnippetFieldsDialog dialog(snippet, parent);
    if (dialog.exec() != QDialog::Accepted)
        return false;
    *values = dialog.values();
    return true;
}

} // namespace OFS
