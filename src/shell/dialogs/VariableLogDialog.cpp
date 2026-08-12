#include "shell/dialogs/VariableLogDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QVBoxLayout>

namespace OFS {

VariableLogDialog::VariableLogDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("VarDlg"));
    setWindowTitle(tr("Options"));

    auto* format = new QGroupBox(tr("Format:"), this);
    format->setObjectName(QStringLiteral("groupBox"));
    m_number = new QRadioButton(tr("StateDef number"), format);
    m_number->setObjectName(QStringLiteral("radioButton"));
    m_number->setChecked(true);
    m_name = new QRadioButton(tr("StateDef name"), format);
    m_name->setObjectName(QStringLiteral("radioButton_2"));
    m_numberAndName = new QRadioButton(tr("StateDef number and name"), format);
    m_numberAndName->setObjectName(QStringLiteral("radioButton_3"));
    auto* formatLayout = new QVBoxLayout(format);
    formatLayout->addWidget(m_number);
    formatLayout->addWidget(m_name);
    formatLayout->addWidget(m_numberAndName);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Vertical, this);
    buttons->setObjectName(QStringLiteral("buttonBox"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_onlyUsed = new QCheckBox(tr("Only used variables"), this);
    m_onlyUsed->setObjectName(QStringLiteral("checkBox_3"));
    m_onlyUsed->setChecked(true);
    m_onePerLine = new QCheckBox(tr("One per line"), this);
    m_onePerLine->setObjectName(QStringLiteral("checkBox_4"));
    m_floats = new QCheckBox(tr("Float"), this);
    m_floats->setObjectName(QStringLiteral("checkBox_2"));
    m_floats->setChecked(true);
    m_integers = new QCheckBox(tr("Integer"), this);
    m_integers->setObjectName(QStringLiteral("checkBox"));
    m_integers->setChecked(true);

    auto* layout = new QGridLayout(this);
    layout->addWidget(format, 0, 0, 1, 2);
    layout->addWidget(buttons, 0, 2);
    layout->addWidget(m_onlyUsed, 1, 0);
    layout->addWidget(m_onePerLine, 1, 1);
    layout->addWidget(m_floats, 2, 0);
    layout->addWidget(m_integers, 2, 1);
    resize(380, 166);
}

VariableLogOptions VariableLogDialog::options() const
{
    VariableLogOptions options;
    if (m_name->isChecked())
        options.format = VariableLogOptions::Format::Name;
    else if (m_numberAndName->isChecked())
        options.format = VariableLogOptions::Format::NumberAndName;
    options.onlyUsed = m_onlyUsed->isChecked();
    options.onePerLine = m_onePerLine->isChecked();
    options.floats = m_floats->isChecked();
    options.integers = m_integers->isChecked();
    return options;
}

} // namespace OFS
