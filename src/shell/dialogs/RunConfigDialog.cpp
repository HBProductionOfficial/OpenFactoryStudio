#include "shell/dialogs/RunConfigDialog.h"

#include "shell/FileDialogs.h"

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
    QList<ParameterPreset> presets;
    for (const auto& [caption, parameters] : RunConfiguration::parameterPresets())
        presets.append({caption, parameters});
    return presets;
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
    m_name->setObjectName(QStringLiteral("eng_name"));
    m_executable->setObjectName(QStringLiteral("eng_exe"));
    m_icon->setObjectName(QStringLiteral("eng_icon"));
    m_presets->setObjectName(QStringLiteral("eng_params_list"));
    m_parameters->setObjectName(QStringLiteral("eng_params"));
    m_buttons->setObjectName(QStringLiteral("buttonBox"));

    auto* exeBrowse = new QPushButton(tr("..."), this);
    exeBrowse->setObjectName(QStringLiteral("exe_browse"));
    auto* iconBrowse = new QPushButton(tr("..."), this);
    iconBrowse->setObjectName(QStringLiteral("icon_browse"));
    auto label = [this](const QString& text, const char* name) {
        auto* l = new QLabel(text, this);
        l->setObjectName(QString::fromLatin1(name));
        return l;
    };
    for (QPushButton* button : {exeBrowse, iconBrowse}) {
        button->setAutoDefault(false);
        button->setFixedWidth(41);
    }
    for (const ParameterPreset& preset : parameterPresets())
        m_presets->addItem(preset.caption, preset.parameters);

    auto* grid = new QGridLayout(this);
    grid->setColumnStretch(0, 2);
    grid->setColumnStretch(1, 1);
    grid->addWidget(label(tr("Name:"), "name_lb"), 0, 0, 1, 3);
    grid->addWidget(m_name, 1, 0, 1, 3);
    grid->addWidget(label(tr("Executable:"), "exe_lb"), 2, 0, 1, 3);
    grid->addWidget(m_executable, 3, 0, 1, 2);
    grid->addWidget(exeBrowse, 3, 2);
    grid->addWidget(label(tr("Icon:"), "icon_lb"), 4, 0, 1, 3);
    grid->addWidget(m_icon, 5, 0, 1, 2);
    grid->addWidget(iconBrowse, 5, 2);
    grid->addWidget(label(tr("Parameters:"), "params_lb"), 6, 0, 1, 3);
    grid->addWidget(m_presets, 7, 0);
    grid->addWidget(m_parameters, 7, 1, 1, 2);
    grid->addWidget(m_buttons, 8, 0, 1, 3);
    grid->setRowStretch(9, 1);

    connect(m_presets, &QComboBox::activated, this,
            [this](int index) { m_parameters->setText(m_presets->itemData(index).toString()); });
    connect(exeBrowse, &QPushButton::clicked, this, &RunConfigDialog::browseExecutable);
    connect(iconBrowse, &QPushButton::clicked, this, &RunConfigDialog::browseIcon);
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
    // Nothing is checked here. A missing executable is reported when running.
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(true);
}

void RunConfigDialog::browseExecutable()
{
#if defined(Q_OS_WIN)
    const QString filter = tr("MUGENext (*mugen.exe)");
#else
    const QString filter = tr("MUGENext (mugen*)");
#endif
    const QString file =
        FileDialogs::getOpenFileName(this, tr("Executable"), QFileInfo(m_executable->text()).absolutePath(), filter);
    if (!file.isEmpty())
        m_executable->setText(QDir::toNativeSeparators(file));
}

void RunConfigDialog::browseIcon()
{
    const QString file = FileDialogs::getOpenFileName(this, tr("Icon"), QFileInfo(m_icon->text()).absolutePath(),
                                                      tr("Portable Network Graphic (*.png)"));
    if (!file.isEmpty())
        m_icon->setText(QDir::toNativeSeparators(file));
}

} // namespace OFS
