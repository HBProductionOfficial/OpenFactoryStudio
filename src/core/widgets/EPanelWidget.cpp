#include "core/widgets/EPanelWidget.h"

#include <QLabel>
#include <QVBoxLayout>

EPanelWidget::EPanelWidget(QWidget* parent)
    : QFrame(parent)
    , m_layout(new QVBoxLayout(this))
    , m_title(new QLabel(this))
    , m_content(new QWidget(this))
    , m_body(new QVBoxLayout(m_content))
{
    m_title->setObjectName(QStringLiteral("panelTitle"));
    m_title->setAlignment(Qt::AlignCenter);
    QFont f = m_title->font();
    f.setBold(true);
    m_title->setFont(f);
    m_title->setMinimumHeight(30);

    m_content->setObjectName(QStringLiteral("panelContent"));
    m_body->setContentsMargins(0, 0, 0, 0);
    m_body->setSpacing(0);

    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    m_layout->addWidget(m_title);
    m_layout->addWidget(m_content, 1);
}

QString EPanelWidget::title() const
{
    return m_title->text();
}

void EPanelWidget::setTitle(const QString& title)
{
    m_title->setText(title);
}

int EPanelWidget::leftMargin() const
{
    return m_layout->contentsMargins().left();
}

int EPanelWidget::topMargin() const
{
    return m_layout->contentsMargins().top();
}

int EPanelWidget::rightMargin() const
{
    return m_layout->contentsMargins().right();
}

int EPanelWidget::bottomMargin() const
{
    return m_layout->contentsMargins().bottom();
}

void EPanelWidget::setLeftMargin(int margin)
{
    QMargins m = m_layout->contentsMargins();
    m.setLeft(margin);
    m_layout->setContentsMargins(m);
}

void EPanelWidget::setTopMargin(int margin)
{
    QMargins m = m_layout->contentsMargins();
    m.setTop(margin);
    m_layout->setContentsMargins(m);
}

void EPanelWidget::setRightMargin(int margin)
{
    QMargins m = m_layout->contentsMargins();
    m.setRight(margin);
    m_layout->setContentsMargins(m);
}

void EPanelWidget::setBottomMargin(int margin)
{
    QMargins m = m_layout->contentsMargins();
    m.setBottom(margin);
    m_layout->setContentsMargins(m);
}
