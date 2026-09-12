#pragma once

#include <QLabel>
#include <QStatusBar>

class QProgressBar;

namespace OFS {

// Status bar section that reports clicks.
class EClickableLabel : public QLabel
{
    Q_OBJECT

public:
    using QLabel::QLabel;

Q_SIGNALS:
    void clicked();

protected:
    void mouseReleaseEvent(QMouseEvent* event) override;
};

// Four text sections of equal width filled by the active editor, then a progress bar for background tasks. Sections
// keep their text until their owner rewrites them.
class StatusBar : public QStatusBar
{
    Q_OBJECT

public:
    static constexpr int SectionCount = 4;

    explicit StatusBar(QWidget* parent = nullptr);

    void setSection(int index, const QString& text);
    QString section(int index) const;
    EClickableLabel* sectionLabel(int index) const { return m_sections[index]; }
    QProgressBar* progressBar() const { return m_progress; }

    void clearSections();
    // The progress bar moves while tasks run and shows the progress when only one task knows it.
    void setTasks(const QStringList& names, int progress = -1);
    const QStringList& tasks() const { return m_tasks; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    EClickableLabel* m_sections[SectionCount];
    QProgressBar* m_progress;
    QStringList m_tasks;
};

} // namespace OFS
