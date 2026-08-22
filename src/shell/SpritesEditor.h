#pragma once

#include "core/sprites/SpriteImages.h"

#include <QObject>
#include <QPointer>

#include <functional>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListView;
class QStandardItemModel;
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

    bool imageMode() const;
    bool confirmLeave();
    void copyImage();
    void pasteImage();

Q_SIGNALS:
    void imageModeChanged(bool on);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool performImage(const QString& actionId);
    void bindImageControls(QWidget* panel);
    void updateMode();
    void refreshLayers();
    void imageModeSwitched();
    void syncTool();
    void saveLayersAsSprites();
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
    QPointer<QWidget> m_panel;
    QPointer<QListView> m_layers;
    QPointer<QComboBox> m_blend;
    QPointer<QSpinBox> m_opacity;
    QPointer<QWidget> m_foreground;
    QPointer<QWidget> m_background;
    QStandardItemModel* m_layerModel = nullptr;
    bool m_updating = false;
};

} // namespace OFS
