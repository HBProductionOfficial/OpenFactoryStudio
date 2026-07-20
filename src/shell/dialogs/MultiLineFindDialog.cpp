#include "shell/dialogs/MultiLineFindDialog.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace OFS {

namespace {

QPushButton* button(const QString& text, const char* objectName, QWidget* parent)
{
    auto* b = new QPushButton(text, parent);
    b->setObjectName(QString::fromLatin1(objectName));
    b->setAutoDefault(false);
    return b;
}

} // namespace

MultiLineFindDialog::MultiLineFindDialog(QWidget* parent)
    : QDialog(parent, Qt::Tool)
{
    setObjectName(QStringLiteral("FindDlgMlt"));
    setWindowTitle(tr("Find/Replace (multi-line)"));
    resize(400, 556);

    auto* findGroup = new QGroupBox(tr("Find:"), this);
    findGroup->setObjectName(QStringLiteral("find_grp"));
    m_find = new QPlainTextEdit(findGroup);
    m_find->setObjectName(QStringLiteral("findTxt"));
    m_case = new QCheckBox(tr("Case sensitive"), findGroup);
    m_case->setObjectName(QStringLiteral("case_2"));
    QPushButton* next = button(tr("Next"), "next_but", findGroup);
    QPushButton* previous = button(tr("Previous"), "previous_but", findGroup);
    auto* findLayout = new QGridLayout(findGroup);
    findLayout->addWidget(m_find, 0, 0, 1, 4);
    findLayout->addWidget(m_case, 1, 0);
    findLayout->setColumnStretch(1, 1);
    findLayout->addWidget(next, 1, 2);
    findLayout->addWidget(previous, 1, 3);

    auto* replaceGroup = new QGroupBox(tr("Replace:"), this);
    replaceGroup->setObjectName(QStringLiteral("replace_grp"));
    m_replace = new QPlainTextEdit(replaceGroup);
    m_replace->setObjectName(QStringLiteral("replaceTxt"));
    QPushButton* replacePrevious = button(tr("Previous"), "previous_but_2", replaceGroup);
    QPushButton* replaceNext = button(tr("Next"), "next_but_2", replaceGroup);
    QPushButton* replaceAll = button(tr("All"), "all_but", replaceGroup);
    auto* replaceLayout = new QGridLayout(replaceGroup);
    replaceLayout->addWidget(m_replace, 0, 0, 1, 4);
    replaceLayout->setColumnStretch(0, 1);
    replaceLayout->addWidget(replacePrevious, 1, 1);
    replaceLayout->addWidget(replaceNext, 1, 2);
    replaceLayout->addWidget(replaceAll, 1, 3);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(findGroup, 1);
    layout->addWidget(replaceGroup, 1);

    connect(next, &QPushButton::clicked, this, [this] { Q_EMIT findRequested(false); });
    connect(previous, &QPushButton::clicked, this, [this] { Q_EMIT findRequested(true); });
    connect(replaceNext, &QPushButton::clicked, this, [this] { Q_EMIT replaceRequested(false); });
    connect(replacePrevious, &QPushButton::clicked, this, [this] { Q_EMIT replaceRequested(true); });
    connect(replaceAll, &QPushButton::clicked, this, &MultiLineFindDialog::replaceAllRequested);
}

QString MultiLineFindDialog::findText() const
{
    return m_find->toPlainText();
}

void MultiLineFindDialog::setFindText(const QString& text)
{
    m_find->setPlainText(text);
}

QString MultiLineFindDialog::replaceText() const
{
    return m_replace->toPlainText();
}

bool MultiLineFindDialog::isCaseSensitive() const
{
    return m_case->isChecked();
}

} // namespace OFS
