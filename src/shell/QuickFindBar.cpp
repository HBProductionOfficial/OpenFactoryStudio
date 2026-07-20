#include "shell/QuickFindBar.h"

#include "core/settings/Config.h"
#include "core/theme/IconSet.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QToolButton>

namespace OFS {

namespace {

QToolButton* optionButton(QWidget* parent, const char* objectName, const QString& toolTip, Config& config,
                          const QString& key)
{
    auto* button = new QToolButton(parent);
    button->setObjectName(QString::fromLatin1(objectName));
    button->setToolTip(toolTip);
    button->setCheckable(true);
    button->setAutoRaise(true);
    button->setChecked(config.boolValue(key));
    QObject::connect(button, &QToolButton::toggled, parent, [&config, key](bool on) { config.setValue(key, on); });
    return button;
}

} // namespace

QuickFindBar::QuickFindBar(Config& config, QWidget* parent)
    : QFrame(parent)
    , m_case(optionButton(this, "quickSearchCase", tr("Case sensitive"), config, QStringLiteral("quicksearch_case")))
    , m_regExp(optionButton(this, "quickSearchRegExp", tr("Regular expression"), config,
                            QStringLiteral("quicksearch_regexp")))
    , m_backward(
          optionButton(this, "quickSearchBackward", tr("Backward search"), config, QStringLiteral("quicksearch_back")))
    , m_circular(optionButton(this, "quickSearchCircular", tr("Circular search"), config,
                              QStringLiteral("quicksearch_circular")))
    , m_find(new QLineEdit(this))
    , m_findButton(new QPushButton(tr("Find"), this))
    , m_replace(new QLineEdit(this))
    , m_replaceButton(new QPushButton(tr("Replace"), this))
    , m_replaceAllButton(new QPushButton(tr("Replace All"), this))
    , m_close(new QToolButton(this))
{
    setObjectName(QStringLiteral("quickFind"));
    m_find->setObjectName(QStringLiteral("quickSearchFind"));
    m_findButton->setObjectName(QStringLiteral("quickSearchFindBut"));
    m_replace->setObjectName(QStringLiteral("quickSearchReplace"));
    m_replaceButton->setObjectName(QStringLiteral("quickSearchReplaceBut"));
    m_replaceAllButton->setObjectName(QStringLiteral("quickSearchReplaceAllBut"));
    m_close->setObjectName(QStringLiteral("quickSearchClose"));
    m_close->setAutoRaise(true);
    m_find->setClearButtonEnabled(true);
    m_replace->setClearButtonEnabled(true);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);
    for (QToolButton* b : {m_case, m_regExp, m_backward, m_circular})
        layout->addWidget(b);
    layout->addWidget(m_find, 1);
    layout->addWidget(m_findButton);
    layout->addWidget(m_replace, 1);
    layout->addWidget(m_replaceButton);
    layout->addWidget(m_replaceAllButton);
    layout->addStretch();
    layout->addWidget(m_close);

    connect(m_find, &QLineEdit::returnPressed, this, &QuickFindBar::findRequested);
    connect(m_findButton, &QPushButton::clicked, this, &QuickFindBar::findRequested);
    connect(m_replace, &QLineEdit::returnPressed, this, &QuickFindBar::replaceRequested);
    connect(m_replaceButton, &QPushButton::clicked, this, &QuickFindBar::replaceRequested);
    connect(m_replaceAllButton, &QPushButton::clicked, this, &QuickFindBar::replaceAllRequested);
    connect(m_close, &QToolButton::clicked, this, &QuickFindBar::closeRequested);
    connect(m_find, &QLineEdit::textChanged, this, [this] { setNotFound(false); });
}

QString QuickFindBar::findText() const
{
    return m_find->text();
}

void QuickFindBar::setFindText(const QString& text)
{
    m_find->setText(text);
}

QString QuickFindBar::replaceText() const
{
    return m_replace->text();
}

SearchOptions QuickFindBar::options() const
{
    SearchOptions o;
    o.caseSensitive = m_case->isChecked();
    o.regExp = m_regExp->isChecked();
    return o;
}

bool QuickFindBar::isBackward() const
{
    return m_backward->isChecked();
}

bool QuickFindBar::isCircular() const
{
    return m_circular->isChecked();
}

void QuickFindBar::setIcons(const IconSet& icons)
{
    m_case->setIcon(icons.icon(QStringLiteral("quick_search_case")));
    m_regExp->setIcon(icons.icon(QStringLiteral("quick_search_regexp")));
    m_backward->setIcon(icons.icon(QStringLiteral("quick_search_backward")));
    m_circular->setIcon(icons.icon(QStringLiteral("quick_search_circular")));
    m_close->setIcon(icons.icon(QStringLiteral("quick_search_close")));
}

void QuickFindBar::activate()
{
    show();
    m_find->setFocus(Qt::ShortcutFocusReason);
    m_find->selectAll();
}

void QuickFindBar::setNotFound(bool notFound)
{
    if (m_find->property("notFound").toBool() == notFound)
        return;
    m_find->setProperty("notFound", notFound);
    // Style sheets may color the box through QLineEdit[notFound="true"].
    m_find->style()->unpolish(m_find);
    m_find->style()->polish(m_find);
}

} // namespace OFS
