#pragma once

#include <QObject>
#include <QPointer>

class QLabel;
class QScrollBar;
class QSlider;
class QSpinBox;
class QWidget;

namespace OFS {

class OrganizerBinding;
class SoundDocument;

class AppContext;
class SoundPage;

// Left panel of the Sounds editor, bound to the current sound page, plus the editor's actions.
class SoundsEditor : public QObject
{
    Q_OBJECT

public:
    explicit SoundsEditor(AppContext& context, QObject* parent = nullptr);

    void bindPanel(QWidget* panel);
    void setPage(SoundPage* page);
    SoundPage* page() const { return m_page; }
    OrganizerBinding* organizer() const { return m_organizer; }

    void importFromProject(const QList<QPair<QString, const SoundDocument*>>& sources);

    bool perform(const QString& actionId);
    bool confirmLeave();

Q_SIGNALS:
    void stateChanged();

private:
    void refresh();
    bool performFiles(const QString& id);

    AppContext& m_context;
    bool m_empty = true;
    QPointer<SoundPage> m_page;
    QPointer<QScrollBar> m_selector;
    QPointer<QLabel> m_selectorLabel;
    QPointer<QSpinBox> m_group;
    QPointer<QSpinBox> m_number;
    QPointer<QLabel> m_info;
    QPointer<QSlider> m_volume;
    QPointer<QSpinBox> m_cursor;
    QPointer<QSpinBox> m_start;
    QPointer<QSpinBox> m_end;
    bool m_updating = false;
    OrganizerBinding* m_organizer = nullptr;
};

} // namespace OFS
