#pragma once

#include "core/theme/ColorSet.h"

#include <QWidget>

class QCheckBox;
class QListView;
class QPushButton;
class QStandardItemModel;

namespace OFS {

// Options > Colors. Editing a role makes the dialog switch the color set to "Custom".
class ColorsPage : public QWidget
{
    Q_OBJECT

public:
    explicit ColorsPage(QWidget* parent = nullptr);

    // Roles the set lacks take their fallback colors. Clears the modified flag.
    void setColorSet(const ColorSet& set);
    const ColorSet& colorSet() const { return m_set; }

    bool isModified() const { return m_modified; }
    void setModified(bool modified) { m_modified = modified; }

    QString currentRole() const;
    void setCurrentRole(const QString& role);

    void setRoleColor(const QString& role, const QColor& color);
    void setRoleStyle(const QString& role, bool bold, bool italic);

    QListView* list() const { return m_list; }

Q_SIGNALS:
    void edited();
    void saveRequested();

private:
    void refreshRows();
    void updateControls();
    void chooseColor();
    void styleToggled();

    ColorSet m_set;
    bool m_modified = false;
    bool m_updating = false;
    QStandardItemModel* m_model;
    QListView* m_list;
    QCheckBox* m_bold;
    QCheckBox* m_italic;
    QPushButton* m_color;
    QPushButton* m_save;
};

} // namespace OFS
