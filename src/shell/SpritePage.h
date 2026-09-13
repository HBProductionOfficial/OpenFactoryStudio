#pragma once

#include "core/sprites/SpriteFile.h"

#include <QPointer>
#include <QWidget>

class QStackedWidget;

namespace OFS {

class Config;
class GraphicsView;
class ImageEditor;

// Sprites editor workspace for one sprite file. Dragging the sprite moves its axis, one undo step per drag. In image
// editor mode the image editor takes the workspace's place.
class SpritePage : public QWidget
{
    Q_OBJECT

public:
    explicit SpritePage(SpriteFile* file, QWidget* parent = nullptr);

    SpriteFile* file() const { return m_file; }
    GraphicsView* view() const { return m_view; }

    int current() const { return m_current; }
    void setCurrent(int index);
    // Shows palette index 0 opaque. It is normally transparent.
    void setShowTransparent(bool show);
    void setOnionSkin(int index, double opacity);
    // The zoom level and origin are saved to `config` as they change.
    void applySettings(Config& config);

    QPoint cursorPosition() const { return m_cursor; }

    int currentPalette() const { return m_palette; }
    void setCurrentPalette(int index);
    // Draws the current 8-bit sprite with `palette` instead of its own (nullptr for its own).
    void setDisplayPalette(SpritePalettePtr palette);
    void setColorPicker(bool on);
    bool colorPicker() const { return m_picker; }

    bool imageMode() const;
    bool setImageMode(bool on);
    ImageEditor* imageEditor() const { return m_editor; }
    bool confirmDiscardImage();
    void reloadImage();
    void saveImage(bool allInstances = false);
    // With `currentSaved` the current sprite now holds the canvas and its axis follows the canvas origin.
    void applyFromImage(const QString& text, const SpriteDocument& doc, bool currentSaved);
    QPoint savedOrigin() const { return m_savedOrigin; }

Q_SIGNALS:
    void currentChanged(int index);
    void statusChanged();
    void imageModeChanged(bool on);
    void currentPaletteChanged(int index);
    void colorPicked(int index);

private:
    void refresh();
    QImage spriteImage(int index) const;

    QPointer<SpriteFile> m_file;
    QStackedWidget* m_stack;
    GraphicsView* m_view;
    ImageEditor* m_editor;
    SpritePixelsPtr m_openedPixels;
    SpritePalettePtr m_openedPalette;
    QPoint m_savedOrigin;
    bool m_savingImage = false;
    int m_current = 0;
    bool m_showTransparent = false;
    int m_onion = -1;
    double m_onionOpacity = 0.74;
    QPoint m_dragOffset;
    bool m_settingsApplied = false;
    QPoint m_cursor;
    int m_palette = -1;
    SpritePalettePtr m_displayPalette;
    bool m_picker = false;
};

} // namespace OFS
