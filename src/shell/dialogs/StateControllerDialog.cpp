#include "shell/dialogs/StateControllerDialog.h"

#include "core/code/CodeEditor.h"
#include "core/code/CodeLine.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

namespace OFS {

StateControllerDialog::StateControllerDialog(const SyntaxDatabase* syntax, QWidget* parent)
    : QDialog(parent)
    , m_editor(new CodeEditor(this))
{
    setObjectName(QStringLiteral("SCtrlEdit"));
    setWindowTitle(tr("State controller"));
    resize(393, 342);
    m_editor->setSyntax(syntax, QStringLiteral("state"));
    m_editor->setPlainText(QStringLiteral("[State ]\ntype = "));

    auto* ok = new QPushButton(tr("OK"), this);
    ok->setObjectName(QStringLiteral("pushButton_2"));
    auto* cancel = new QPushButton(tr("Cancel"), this);
    cancel->setObjectName(QStringLiteral("pushButton"));
    cancel->setDefault(true);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addWidget(cancel);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_editor, 1);
    layout->addLayout(buttons);
    m_editor->setFocus();
}

QString StateControllerDialog::text() const
{
    return m_editor->toPlainText();
}

void StateControllerDialog::setText(const QString& text)
{
    m_editor->setPlainText(text);
}

QString StateControllerDialog::controllerType(const QString& text)
{
    for (const QString& lineText : text.split(QLatin1Char('\n'))) {
        const CodeLine line = CodeLine::parse(lineText);
        if (line.kind == CodeLine::Kind::KeyValue && line.key.compare(QLatin1String("type"), Qt::CaseInsensitive) == 0)
            return line.value.section(QLatin1Char(' '), 0, 0).trimmed();
    }
    return {};
}

} // namespace OFS
