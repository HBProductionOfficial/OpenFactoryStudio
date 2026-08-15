#include "engines/mugen/dialogs/AddAnimationDialog.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>

namespace OFS::Mugen {

AddAnimationDialog::AddAnimationDialog(const QList<int>& taken, int suggested, QWidget* parent)
    : QDialog(parent)
    , m_taken(taken)
    , m_name(new QLineEdit(this))
    , m_number(new QSpinBox(this))
    , m_time(new QSpinBox(this))
{
    setObjectName(QStringLiteral("AddAnim"));
    setWindowTitle(tr("Add animation"));
    m_name->setObjectName(QStringLiteral("name"));
    m_number->setObjectName(QStringLiteral("number"));
    m_time->setObjectName(QStringLiteral("time"));
    for (QSpinBox* box : {m_number, m_time})
        box->setRange(-300000, 300000);
    int number = suggested;
    while (m_taken.contains(number) && number < 300000)
        ++number;
    m_number->setValue(number);

    auto label = [this](const QString& text, const char* name) {
        auto* l = new QLabel(text, this);
        l->setObjectName(QLatin1String(name));
        return l;
    };
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setObjectName(QStringLiteral("buttonBox"));
    connect(buttons, &QDialogButtonBox::accepted, this, &AddAnimationDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QGridLayout(this);
    layout->addWidget(label(tr("Name:"), "label"), 0, 0, 1, 2);
    layout->addWidget(m_name, 1, 0, 1, 2);
    layout->addWidget(label(tr("Number:"), "label_2"), 2, 0);
    layout->addWidget(label(tr("Time:"), "label_3"), 2, 1);
    layout->addWidget(m_number, 3, 0);
    layout->addWidget(m_time, 3, 1);
    layout->addWidget(buttons, 4, 0, 1, 2);
    m_name->setFocus();
    resize(271, 154);
}

QString AddAnimationDialog::name() const
{
    return m_name->text().trimmed();
}

int AddAnimationDialog::number() const
{
    return m_number->value();
}

int AddAnimationDialog::time() const
{
    return m_time->value();
}

void AddAnimationDialog::accept()
{
    if (m_taken.contains(number())) {
        QMessageBox::warning(this, windowTitle(), tr("Animation %1 already exists.").arg(number()));
        m_number->setFocus();
        return;
    }
    QDialog::accept();
}

} // namespace OFS::Mugen
