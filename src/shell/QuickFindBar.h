#pragma once

#include "core/code/TextSearch.h"

#include <QFrame>

class QLineEdit;
class QPushButton;
class QToolButton;

namespace OFS {

class Config;
class IconSet;

// Edit > Find/Replace bar at the bottom of the workspace. Its options are kept in the quicksearch_* settings.
class QuickFindBar : public QFrame
{
    Q_OBJECT

public:
    QuickFindBar(Config& config, QWidget* parent = nullptr);

    QString findText() const;
    void setFindText(const QString& text);
    QString replaceText() const;

    SearchOptions options() const;
    bool isBackward() const;
    bool isCircular() const;

    void setIcons(const IconSet& icons);
    void activate();
    void setNotFound(bool notFound);

Q_SIGNALS:
    void findRequested();
    void replaceRequested();
    void replaceAllRequested();
    void closeRequested();

private:
    QToolButton* m_case;
    QToolButton* m_regExp;
    QToolButton* m_backward;
    QToolButton* m_circular;
    QLineEdit* m_find;
    QPushButton* m_findButton;
    QLineEdit* m_replace;
    QPushButton* m_replaceButton;
    QPushButton* m_replaceAllButton;
    QToolButton* m_close;
};

} // namespace OFS
