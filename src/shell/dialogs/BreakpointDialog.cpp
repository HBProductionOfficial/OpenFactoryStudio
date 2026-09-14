#include "shell/dialogs/BreakpointDialog.h"

#include "shell/FileDialogs.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

namespace OFS {

using Debug::Breakpoint;
using Debug::BreakpointType;

namespace {

QLabel* label(const QString& text, const char* name, QWidget* parent)
{
    auto* l = new QLabel(text, parent);
    l->setObjectName(QLatin1String(name));
    return l;
}

} // namespace

BreakpointDialog::BreakpointDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("BreakpointDlg"));
    setWindowTitle(tr("Breakpoint"));

    m_type = new QComboBox(this);
    m_type->setObjectName(QStringLiteral("brkType"));
    m_type->addItems(
        {tr("File Name and Line Number"), tr("Function Name"), tr("Break on Memory Access"), tr("Break on Frame")});
    m_file = new QLineEdit(this);
    m_file->setObjectName(QStringLiteral("brkFile"));
    m_browse = new QPushButton(tr("Browse..."), this);
    m_browse->setObjectName(QStringLiteral("brkBrowse"));
    m_line = new QSpinBox(this);
    m_line->setObjectName(QStringLiteral("brkLine"));
    m_line->setRange(0, 9999999);
    m_enabled = new QCheckBox(this);
    m_enabled->setObjectName(QStringLiteral("brkEnabled"));
    m_enabled->setChecked(true);
    m_address = new QLineEdit(QStringLiteral("0"), this);
    m_address->setObjectName(QStringLiteral("brkAddress"));
    m_function = new QLineEdit(this);
    m_function->setObjectName(QStringLiteral("brkFunction"));
    m_condition = new QLineEdit(this);
    m_condition->setObjectName(QStringLiteral("brkCondition"));
    m_ignore = new QSpinBox(this);
    m_ignore->setObjectName(QStringLiteral("brkIgnore"));
    m_ignore->setRange(0, 65535);
    m_oneShot = new QCheckBox(this);
    m_oneShot->setObjectName(QStringLiteral("brkOneShot"));
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttons->setObjectName(QStringLiteral("buttonBox"));

    auto* fileRow = new QHBoxLayout;
    fileRow->addWidget(m_file, 1);
    fileRow->addWidget(m_browse);

    auto* grid = new QGridLayout;
    int row = 0;
    grid->addWidget(label(tr("Breakpoint type:"), "type_lb", this), row, 0);
    grid->addWidget(m_type, row++, 1);
    grid->addWidget(label(tr("File name:"), "file_lb", this), row, 0);
    grid->addLayout(fileRow, row++, 1);
    grid->addWidget(label(tr("Line number:"), "line_lb", this), row, 0);
    grid->addWidget(m_line, row++, 1);
    grid->addWidget(label(tr("Enabled:"), "enabled_lb", this), row, 0);
    grid->addWidget(m_enabled, row++, 1);
    grid->addWidget(label(tr("Address:"), "address_lb", this), row, 0);
    grid->addWidget(m_address, row++, 1);
    grid->addWidget(label(tr("Function:"), "function_lb", this), row, 0);
    grid->addWidget(m_function, row++, 1);
    grid->addWidget(label(tr("Condition:"), "condition_lb", this), row, 0);
    grid->addWidget(m_condition, row++, 1);
    grid->addWidget(label(tr("Ignore count:"), "ignore_lb", this), row, 0);
    grid->addWidget(m_ignore, row++, 1);
    grid->addWidget(label(tr("One shot only:"), "one_shot_lb", this), row, 0);
    grid->addWidget(m_oneShot, row++, 1);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(row, 1);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(grid, 1);
    layout->addWidget(m_buttons);
    resize(702, 450);

    connect(m_type, &QComboBox::currentIndexChanged, this, [this] {
        if (m_type->currentIndex() != int(BreakpointType::Memory) && m_address->text() == QLatin1String("0"))
            m_address->clear();
        updateFields();
    });
    for (QLineEdit* edit : {m_file, m_address, m_function})
        connect(edit, &QLineEdit::textChanged, this, &BreakpointDialog::updateFields);
    connect(m_line, &QSpinBox::valueChanged, this, &BreakpointDialog::updateFields);
    connect(m_browse, &QPushButton::clicked, this, [this] {
        const QString start = m_file->text().isEmpty() ? QString() : QFileInfo(m_file->text()).absolutePath();
        const QString file = FileDialogs::getOpenFileName(this, tr("Choose file..."), start, tr("All files (*.*)"));
        if (!file.isEmpty())
            m_file->setText(QDir::toNativeSeparators(file));
    });
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    updateFields();
}

void BreakpointDialog::setBreakpoint(const Breakpoint& breakpoint, bool editing)
{
    m_original = breakpoint;
    m_type->setCurrentIndex(int(breakpoint.type));
    m_type->setEnabled(!editing);
    m_file->setText(QDir::toNativeSeparators(breakpoint.file));
    m_line->setValue(breakpoint.line);
    m_enabled->setChecked(breakpoint.enabled);
    m_address->setText(breakpoint.address);
    m_function->setText(breakpoint.function);
    m_condition->setText(breakpoint.condition);
    m_ignore->setValue(breakpoint.ignoreCount);
    m_oneShot->setChecked(breakpoint.oneShot);
    updateFields();
}

Breakpoint BreakpointDialog::breakpoint() const
{
    Breakpoint b = m_original;
    b.type = BreakpointType(m_type->currentIndex());
    b.file = b.type == BreakpointType::FileLine ? QDir::fromNativeSeparators(m_file->text().trimmed()) : QString();
    b.line = b.type == BreakpointType::FileLine ? m_line->value() : 0;
    b.function = b.type == BreakpointType::Function ? m_function->text().trimmed() : QString();
    b.address = b.type == BreakpointType::Memory ? m_address->text().trimmed() : QString();
    b.enabled = m_enabled->isChecked();
    b.condition = m_condition->text().trimmed();
    b.ignoreCount = m_ignore->value();
    b.oneShot = m_oneShot->isChecked();
    return b;
}

void BreakpointDialog::updateFields()
{
    const auto type = BreakpointType(m_type->currentIndex());
    const bool fileLine = type == BreakpointType::FileLine;
    m_file->setEnabled(fileLine);
    m_browse->setEnabled(fileLine);
    m_line->setEnabled(fileLine);
    m_function->setEnabled(type == BreakpointType::Function);
    m_address->setEnabled(type == BreakpointType::Memory);
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(breakpoint().isValid());
}

} // namespace OFS
