#include "shell/dialogs/RunConfigDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace OFS {

QList<RunConfigDialog::ParameterPreset> RunConfigDialog::parameterPresets()
{
    return {
        {tr("Current char vs KFM"), QStringLiteral("%0 kfm")},
        {tr("Current char vs KFM AI"), QStringLiteral("%0 kfm -p2.ai 8")},
        {tr("Current char vs current char"), QStringLiteral("%0 %0")},
        {tr("Current char vs current char AI"), QStringLiteral("%0 %0 -p2.ai 8")},
        {tr("Current motif"), QStringLiteral("-r %0")},
        {tr("KFM vs KFM on current Stage"), QStringLiteral("kfm kfm -s %0")},
        {tr("KFM vs KFM on current motif"), QStringLiteral("-r %0 kfm kfm")},
    };
}

RunConfigDialog::RunConfigDialog(const RunConfiguration& value, bool editing, QWidget* parent)
    : QDialog(parent)
    , m_name(new QLineEdit(this))
    , m_executable(new QLineEdit(this))
    , m_icon(new QLineEdit(this))
    , m_presets(new QComboBox(this))
    , m_parameters(new QLineEdit(this))
    , m_buttons(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this))
{
    setObjectName(QStringLiteral("EngineDlg"));
    setWindowTitle(editing ? tr("Edit") : tr("Add"));
    m_name->setObjectName(QStringLiteral("name"));
    m_executable->setObjectName(QStringLiteral("executable"));
    m_icon->setObjectName(QStringLiteral("icon"));
    m_presets->setObjectName(QStringLiteral("parameterPresets"));
    m_parameters->setObjectName(QStringLiteral("parameters"));

    auto* exeBrowse = new QPushButton(tr("..."), this);
    auto* iconBrowse = new QPushButton(tr("..."), this);
    for (QPushButton* button : {exeBrowse, iconBrowse}) {
        button->setAutoDefault(false);
        button->setFixedWidth(41);
    }
    for (const ParameterPreset& preset : parameterPresets())
        m_presets->addItem(preset.caption, preset.parameters);

    auto* grid = new QGridLayout(this);
    grid->setColumnStretch(0, 2);
    grid->setColumnStretch(1, 1);
    grid->addWidget(new QLabel(tr("Name:"), this), 0, 0, 1, 3);
    grid->addWidget(m_name, 1, 0, 1, 3);
    grid->addWidget(new QLabel(tr("Executable:"), this), 2, 0, 1, 3);
    grid->addWidget(m_executable, 3, 0, 1, 2);
    grid->addWidget(exeBrowse, 3, 2);
    grid->addWidget(new QLabel(tr("Icon:"), this), 4, 0, 1, 3);
    grid->addWidget(m_icon, 5, 0, 1, 2);
    grid->addWidget(iconBrowse, 5, 2);
    grid->addWidget(new QLabel(tr("Parameters:"), this), 6, 0, 1, 3);
    grid->addWidget(m_presets, 7, 0);
    grid->addWidget(m_parameters, 7, 1, 1, 2);
    grid->addWidget(m_buttons, 8, 0, 1, 3);
    grid->setRowStretch(9, 1);

    connect(m_presets, &QComboBox::activated, this,
            [this](int index) { m_parameters->setText(m_presets->itemData(index).toString()); });
    connect(exeBrowse, &QPushButton::clicked, this, &RunConfigDialog::browseExecutable);
    connect(iconBrowse, &QPushButton::clicked, this, &RunConfigDialog::browseIcon);
    connect(m_name, &QLineEdit::textChanged, this, &RunConfigDialog::updateOk);
    connect(m_executable, &QLineEdit::textChanged, this, &RunConfigDialog::updateOk);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (editing) {
        setValue(value);
    } else {
        RunConfiguration initial = value;
        if (initial.parameters.isEmpty())
            initial.parameters = parameterPresets().first().parameters;
        setValue(initial);
    }
    resize(320, 264);
}

RunConfiguration RunConfigDialog::value() const
{
    return {m_name->text().trimmed(), m_executable->text().trimmed(), m_icon->text().trimmed(),
            m_parameters->text().trimmed()};
}

void RunConfigDialog::setValue(const RunConfiguration& value)
{
    m_name->setText(value.name);
    m_executable->setText(value.executable);
    m_icon->setText(value.icon);
    m_parameters->setText(value.parameters);
    const int preset = m_presets->findData(value.parameters);
    if (preset >= 0)
        m_presets->setCurrentIndex(preset);
    updateOk();
}

void RunConfigDialog::updateOk()
{
    m_buttons->button(QDialogButtonBox::Ok)
        ->setEnabled(!m_name->text().trimmed().isEmpty() && !m_executable->text().trimmed().isEmpty());
}

void RunConfigDialog::browseExecutable()
{
#if defined(Q_OS_WIN)
    const QString filter = tr("Programs (*.exe);;All files (*)");
#else
    const QString filter = tr("All files (*)");
#endif
    const QString file =
        QFileDialog::getOpenFileName(this, tr("Executable"), QFileInfo(m_executable->text()).absolutePath(), filter);
    if (file.isEmpty())
        return;
    m_executable->setText(QDir::fromNativeSeparators(file));
    if (m_name->text().trimmed().isEmpty())
        m_name->setText(QFileInfo(file).dir().dirName());
}

void RunConfigDialog::browseIcon()
{
    const QString file =
        QFileDialog::getOpenFileName(this, tr("Icon"), QFileInfo(m_icon->text()).absolutePath(),
                                     tr("Images (*.png *.ico *.bmp *.jpg *.jpeg *.svg);;All files (*)"));
    if (!file.isEmpty())
        m_icon->setText(QDir::fromNativeSeparators(file));
}

} // namespace OFS
