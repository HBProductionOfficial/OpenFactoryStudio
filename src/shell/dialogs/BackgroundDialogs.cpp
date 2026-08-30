#include "shell/dialogs/BackgroundDialogs.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace OFS {

AddBackgroundDialog::AddBackgroundDialog(QWidget* parent)
    : QDialog(parent)
    , m_name(new QLineEdit(this))
    , m_type(new QComboBox(this))
{
    setObjectName(QStringLiteral("AddBG"));
    setWindowTitle(tr("Add background"));
    auto* nameLabel = new QLabel(tr("Name:"), this);
    nameLabel->setObjectName(QStringLiteral("label"));
    auto* typeLabel = new QLabel(tr("Type:"), this);
    typeLabel->setObjectName(QStringLiteral("label_2"));
    m_name->setObjectName(QStringLiteral("name"));
    m_type->setObjectName(QStringLiteral("type"));
    m_type->addItems({tr("Normal"), tr("Animation"), tr("Parallax"), tr("Dummy")});
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setObjectName(QStringLiteral("buttonBox"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* form = new QFormLayout;
    form->addRow(nameLabel, m_name);
    form->addRow(typeLabel, m_type);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addStretch(1);
    layout->addWidget(buttons);
    resize(286, 191);
}

QString AddBackgroundDialog::name() const
{
    return m_name->text().trimmed();
}

int AddBackgroundDialog::type() const
{
    return m_type->currentIndex();
}

} // namespace OFS
