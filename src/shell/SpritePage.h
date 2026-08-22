#pragma once

#include "core/sprites/SpriteFile.h"

#include <QPointer>
#include <QWidget>

namespace OFS {

class Config;
class GraphicsView;

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

Q_SIGNALS:
    void currentChanged(int index);
    void statusChanged();

private:
    void refresh();
    QImage spriteImage(int index) const;

    QPointer<SpriteFile> m_file;
    GraphicsView* m_view;
    int m_current = 0;
    bool m_showTransparent = false;
    int m_onion = -1;
    double m_onionOpacity = 0.74;
    QPoint m_dragOffset;
    bool m_settingsApplied = false;
    QPoint m_cursor;
};

} // namespace OFS
