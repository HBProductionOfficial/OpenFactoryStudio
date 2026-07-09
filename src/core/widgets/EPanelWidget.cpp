#include "core/widgets/EPanelWidget.h"

#include <QLabel>
#include <QVBoxLayout>

EPanelWidget::EPanelWidget(QWidget* parent)
    : QFrame(parent)
    , m_title(new QLabel(this))
    , m_body(new QVBoxLayout)
{
    m_title->setObjectName(QStringLiteral("panelTitle"));
    m_title->setAlignment(Qt::AlignCenter);
    QFont f = m_title->font();
    f.setBold(true);
    m_title->setFont(f);
    m_title->setMinimumHeight(30);

    m_body->setContentsMargins(0, 0, 0, 0);
    m_body->setSpacing(0);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_title);
    layout->addLayout(m_body, 1);
}

QString EPanelWidget::title() const
{
    return m_title->text();
}

void EPanelWidget::setTitle(const QString& title)
{
    m_title->setText(title);
}
