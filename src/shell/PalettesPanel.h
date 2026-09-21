#pragma once

#include "core/widgets/PaletteWidget.h"
#include "shell/dialogs/PaletteDialogs.h"

#include <QObject>
#include <QPointer>

class QCheckBox;
class QLabel;
class QScrollBar;
class QSpinBox;
class QWidget;

namespace OFS {

class AppContext;
class SpriteDocument;
class SpriteFile;
class SpritePage;

// Palettes panel of the Sprites editor, plus the Palettes menu commands and the Remap tool.
class PalettesPanel : public QObject
{
    Q_OBJECT

public:
    explicit PalettesPanel(AppContext& context, QObject* parent = nullptr);

    void bindPanel(QWidget* panel);
    void setPage(SpritePage* page);
    SpritePage* page() const { return m_page; }
    void setImageMode(bool on);

    // -1 when there is no palette.
    int current() const;
    QList<int> selection() const;
    PaletteWidget* grid() const { return m_grid; }
    RemapDialog* remapDialog() const { return m_remap; }

    static const QStringList& actionIds();
    bool perform(const QString& actionId);

Q_SIGNALS:
    void stateChanged();

private:
    void refresh();
    void updateDisplay();
    void followSprite();
    void editSlot(int slot);
    void slotPressed(int slot, Qt::MouseButton button);
    void applyIdentifiers();
    void apply(const QString& text, const SpriteDocument& doc, bool colorEdit = false);
    void addPalettes();
    void deletePalette();
    QWidget* dialogParent() const;
    QString projectFolder() const;

    AppContext& m_context;
    QPointer<SpritePage> m_page;
    QPointer<QScrollBar> m_selector;
    QPointer<QLabel> m_selectorLabel;
    QPointer<QCheckBox> m_switch;
    QPointer<QSpinBox> m_group;
    QPointer<QSpinBox> m_number;
    QPointer<PaletteWidget> m_grid;
    QPointer<QWidget> m_pickerButton;
    QPointer<RemapDialog> m_remap;
    QList<RemapRow> m_remapRows{RemapRow()};
    bool m_imageMode = false;
    bool m_updating = false;
    bool m_applying = false;
};

} // namespace OFS
