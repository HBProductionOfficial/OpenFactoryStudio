#pragma once

#include "formats/image/Image.h"

#include <QColor>
#include <QDialog>
#include <QPointer>

#include <array>
#include <optional>

class QCheckBox;
class QComboBox;
class QLabel;
class QRadioButton;
class QScrollArea;
class QScrollBar;
class QSlider;
class QToolButton;
class QUndoStack;

namespace OFS {

class AppContext;
class PaletteWidget;
class SpriteFile;

// Tools > Advanced palette editor: two columns, each an image with its palette and its own undo history. Nothing
// reaches the sprite file until Commit changes or Commit sprite changes.
class AdvancedPaletteEditor : public QDialog
{
    Q_OBJECT

public:
    struct Column
    {
        Formats::Image image;
        Formats::Palette palette;
        Formats::Image reference;
    };
    static constexpr int Character = 0;
    static constexpr int ImageOrPalette = 1;

    // `file` is nullptr when the project has no sprite file.
    AdvancedPaletteEditor(AppContext& context, SpriteFile* file, int sprite, int palette, QWidget* parent = nullptr);
    ~AdvancedPaletteEditor() override;

    const Column& column(int which) const { return m_columns[std::size_t(which)]; }
    int checkedColumn() const;
    void setCheckedColumn(int which);
    PaletteWidget* grid(int which) const { return m_grids[std::size_t(which)]; }
    QUndoStack* undoStack(int which) const { return m_undo[std::size_t(which)]; }
    double zoom(int which) const { return m_zoom[std::size_t(which)]; }
    bool hasChanges() const { return m_paletteChanged || m_imageChanged; }

    void change(int which, const QString& text, const Column& after);

public Q_SLOTS:
    void reject() override;

private:
    friend class ColumnCommand;
    void setColumn(int which, const Column& state);
    void refreshColumn(int which);
    QImage render(int which) const;
    void loadSprite(int sprite);
    void loadPalette(int palette);
    bool confirmDiscard();
    void dropPreview();
    void preview();
    void commitPreview();
    void run(const QString& function);
    void commitPalette();
    void commitSprite();
    void openImage();
    void transferColors(int from, int to);
    QToolButton* toolButton(const QString& name, const QString& icon, const QString& text, const QString& tip = {});
    QString folder() const;

    AppContext& m_context;
    QPointer<SpriteFile> m_file;
    std::array<Column, 2> m_columns;
    std::array<PaletteWidget*, 2> m_grids{};
    std::array<QUndoStack*, 2> m_undo{};
    std::array<QScrollArea*, 2> m_areas{};
    std::array<QWidget*, 2> m_views{};
    std::array<double, 2> m_zoom{1.0, 1.0};
    QRadioButton* m_characterOption = nullptr;
    QRadioButton* m_imageOption = nullptr;
    QScrollBar* m_spriteSelector = nullptr;
    QLabel* m_spriteLabel = nullptr;
    QScrollBar* m_paletteSelector = nullptr;
    QLabel* m_paletteLabel = nullptr;
    QComboBox* m_effects = nullptr;
    QCheckBox* m_mask = nullptr;
    QToolButton* m_picker = nullptr;
    QSlider* m_hue = nullptr;
    QSlider* m_saturation = nullptr;
    QSlider* m_red = nullptr;
    QSlider* m_green = nullptr;
    QSlider* m_blue = nullptr;
    QLabel* m_hueLabel = nullptr;
    QLabel* m_saturationLabel = nullptr;
    QLabel* m_redLabel = nullptr;
    QLabel* m_greenLabel = nullptr;
    QLabel* m_blueLabel = nullptr;
    QColor m_maskColor{255, 0, 255};
    int m_sprite = -1;
    int m_palette = -1;
    bool m_paletteChanged = false;
    bool m_imageChanged = false;
    std::optional<Formats::Palette> m_previewBase;
    int m_previewColumn = -1;
    bool m_resettingSliders = false;
};

} // namespace OFS
