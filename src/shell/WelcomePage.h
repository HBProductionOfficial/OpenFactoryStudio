#pragma once

#include <QWidget>

class QCommandLinkButton;
class QGridLayout;
class QToolButton;

namespace OFS {

class History;

// Start page shown when no project is open. "Edit" lets the user remove recent projects from the history.
class WelcomePage : public QWidget
{
    Q_OBJECT

public:
    explicit WelcomePage(History& history, QWidget* parent = nullptr);

    void refresh();
    bool isEditing() const;

Q_SIGNALS:
    void openProjectRequested(const QString& filePath);
    void getStartedRequested();
    void contributeRequested();

private:
    void setEditing(bool editing);
    void removeSelected();

    History& m_history;
    QGridLayout* m_grid;
    QToolButton* m_edit;
    QToolButton* m_remove;
    QList<QCommandLinkButton*> m_buttons;
};

} // namespace OFS
