#include "shell/StatusBar.h"

#include <QMouseEvent>
#include <QProgressBar>

namespace OFS {

void EClickableLabel::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()))
        Q_EMIT clicked();
    QLabel::mouseReleaseEvent(event);
}

StatusBar::StatusBar(QWidget* parent)
    : QStatusBar(parent)
    , m_progress(new QProgressBar(this))
{
    setObjectName(QStringLiteral("statusBar"));
    for (int i = 0; i < SectionCount; ++i) {
        m_sections[i] = new EClickableLabel(this);
        m_sections[i]->setObjectName(QStringLiteral("statusSection%1").arg(i + 1));
        m_sections[i]->setMinimumWidth(1);
        addWidget(m_sections[i], 1);
    }
    m_progress->setObjectName(QStringLiteral("statusProgress"));
    m_progress->setFixedSize(105, 10);
    m_progress->setTextVisible(false);
    m_progress->setRange(0, 100);
    m_progress->setValue(-1);
    addPermanentWidget(m_progress);
}

void StatusBar::setSection(int index, const QString& text)
{
    if (index >= 0 && index < SectionCount)
        m_sections[index]->setText(text);
}

QString StatusBar::section(int index) const
{
    return index >= 0 && index < SectionCount ? m_sections[index]->text() : QString();
}

void StatusBar::clearSections()
{
    for (EClickableLabel* label : m_sections)
        label->clear();
}

} // namespace OFS
