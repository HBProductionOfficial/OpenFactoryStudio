#pragma once

#include "core/sprites/SpriteImages.h"

#include <QObject>
#include <QPair>
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
class PalettesPanel;
class SpriteExplorer;
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
    void setOrganizerActive(bool active);
    void dropOrganizerRows(int target);
    void bindPalettes(QWidget* panel);
    PalettesPanel* palettes() const { return m_palettes; }
    void bindExplorer(QWidget* panel);
    bool explorerShown() const;
    // The explorer panel is shared with the Animations editor.
    void setExplorerActive(bool active);
    void setPage(SpritePage* page);
    SpritePage* page() const { return m_page; }

    // Outside the project folder it writes a copy. Returns whether the file was written.
    bool saveAs(SpriteFile* file, QWidget* parent);

    // Sprite changes (Options > Messages > Sprite Sharing): whether a new image for a linked
    // sprite goes to all its instances. False for a sprite that is not linked.
    bool shareImageChanges(int index, QWidget* parent);

    void applySettings() { refresh(); }

    bool perform(const QString& actionId);

    bool imageMode() const;
    bool confirmLeave();
    void copyImage();
    void pasteImage();
    // Copies the sprite with its palette, axis and identifiers for this program, and its image for others.
    void copySprite();
    void cutSprite();
    void pasteSprite();
    void importFromProject(const QList<QPair<QString, const SpriteDocument*>>& sources);

Q_SIGNALS:
    void imageModeChanged(bool on);
    void explorerChanged();
    void emptyChanged();

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
    void refreshExplorer();
    void addExplorerLayers(bool all, bool aligned);
    void saveExplorerImages();
    void refresh();
    int axisSign() const;
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
    PalettesPanel* m_palettes;
    bool m_empty = true;
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
    bool m_organizerActive = true;
    QPointer<QWidget> m_panel;
    QPointer<QListView> m_layers;
    QPointer<QComboBox> m_blend;
    QPointer<QSpinBox> m_opacity;
    QPointer<QWidget> m_foreground;
    QPointer<QWidget> m_background;
    QStandardItemModel* m_layerModel = nullptr;
    SpriteExplorer* m_spriteExplorer;
    bool m_explorer = false;
    bool m_explorerActive = false;
    bool m_updating = false;
};

} // namespace OFS
