#pragma once

#include "core/sprites/SpriteImages.h"

#include <QObject>
#include <QPointer>

#include <functional>

class QCheckBox;
class QLabel;
class QLineEdit;
class QListView;
class QStringListModel;
class QToolButton;
class QScrollBar;
class QSpinBox;
class QWidget;

namespace OFS {

class AppContext;
class SpriteFile;
class SpritePage;

// Left panel of the Sprites editor in align and image editor mode, plus the editor's actions.
class SpritesEditor : public QObject
{
    Q_OBJECT

public:
    SpritesEditor(AppContext& context, QObject* parent = nullptr);

    void bindPanel(QWidget* panel);
    void bindOrganizer(QWidget* panel);
    void setPage(SpritePage* page);
    SpritePage* page() const { return m_page; }

    // Outside the project folder it writes a copy. Returns whether the file was written.
    bool saveAs(SpriteFile* file, QWidget* parent);

    bool perform(const QString& actionId);

private:
    void refresh();
    bool performDialog(const QString& actionId);
    void addSprites(QWidget* parent, const QString& imageFolder);
    static std::function<AddSpritesOptions::Conflict(int, int)> conflictPrompt(QWidget* parent);
    void applyIdentifiers();
    void applyAxis();
    void updateOnion();
    void updateOnionLabel();
    void refreshOrganizer();
    bool organizePalettes() const;
    QList<int> organizerSelection() const;

    AppContext& m_context;
    QPointer<SpritePage> m_page;
    QPointer<QScrollBar> m_selector;
    QPointer<QLabel> m_selectorLabel;
    QPointer<QSpinBox> m_group;
    QPointer<QSpinBox> m_number;
    QPointer<QSpinBox> m_axisX;
    QPointer<QSpinBox> m_axisY;
    QPointer<QLabel> m_info;
    QPointer<QCheckBox> m_decompress;
    QPointer<QCheckBox> m_showTransparent;
    QPointer<QCheckBox> m_onion;
    QPointer<QScrollBar> m_onionSelector;
    QPointer<QLabel> m_onionLabel;
    QPointer<QLineEdit> m_name;
    QPointer<QListView> m_organizer;
    QPointer<QToolButton> m_organizePalettes;
    QStringListModel* m_organizerModel = nullptr;
    bool m_updating = false;
};

} // namespace OFS
