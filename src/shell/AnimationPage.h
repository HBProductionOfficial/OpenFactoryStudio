#pragma once

#include "core/anims/AirDocument.h"
#include "core/anims/AirEdits.h"
#include "core/widgets/GraphicsView.h"

#include <QHash>
#include <QPointer>
#include <QWidget>

#include <functional>

class QStackedWidget;
class QTimer;

namespace OFS {

class CodeFile;
class CodePage;
class Config;
class Project;
class SpriteFile;
class TimelineWidget;

// Animations editor workspace for one AIR file. The file text is the model: every visual edit rewrites the lines it
// touches as one undo step.
class AnimationPage : public QWidget
{
    Q_OBJECT

public:
    AnimationPage(CodeFile* file, CodePage* code, Project* project, QWidget* parent = nullptr);
    ~AnimationPage() override;

    CodeFile* file() const { return m_file; }
    CodePage* codePage() const { return m_code; }
    GraphicsView* view() const { return m_view; }
    TimelineWidget* timeline() const { return m_timeline; }
    const AirDocument& document() const { return m_doc; }

    // Current animation (index in the document) and frame, -1 when there is none.
    int current() const { return m_current; }
    int currentFrame() const { return m_frame; }
    void setCurrent(int animation);
    void setCurrentFrame(int frame);
    const AirAnimation* animation() const;
    const AirFrame* frame() const;

    bool edit(const std::function<void(QStringList&)>& change);

    bool textMode() const;
    void setTextMode(bool on);

    AirEdits::BoxRef selectedBox() const { return m_selected; }
    void setSelectedBox(const AirEdits::BoxRef& box);
    // Box drawing mode: 1 attack, 2 collision, 0 off.
    int drawingBoxes() const { return m_drawing; }
    void setDrawingBoxes(int type);

    // Playback at 60 ticks per second from the first frame, looping at LoopStart.
    bool isPlaying() const;
    void play();
    void stop();

    void setOnionSkin(bool on, int animation, int frame, double opacity);

    void applySettings(const Config& config, const Config* engine);

    QPoint cursorPosition() const { return m_cursor; }
    void showLine(int line);

Q_SIGNALS:
    void currentChanged();
    void documentChanged();
    void statusChanged();
    void textModeChanged(bool on);
    void playingChanged(bool playing);
    void drawingChanged(int type);
    void selectionChanged();

private:
    void reparse();
    void refresh();
    SpriteFile* sprites() const;
    QImage spriteImage(int index) const;
    struct Placement
    {
        double x = 0;
        double y = 0;
        double xScale = 1;
        double yScale = 1;
        double angle = 0;
    };
    bool spriteLayer(const AirFrame& frame, const Placement& placement, double opacity, bool draggable,
                     GraphicsView::Image* out) const;
    void advance();
    Placement placement() const;
    void followCursor();

    QPointer<CodeFile> m_file;
    CodePage* m_code;
    Project* m_project;
    QStackedWidget* m_stack;
    QWidget* m_graphic;
    GraphicsView* m_view;
    TimelineWidget* m_timeline;
    QTimer* m_timer;
    AirDocument m_doc;
    int m_current = -1;
    int m_frame = -1;
    QHash<int, int> m_frames;
    AirEdits::BoxRef m_selected;
    QList<AirEdits::BoxRef> m_boxRefs;
    int m_drawing = 0;
    QPoint m_dragOffset;
    QPoint m_cursor;
    int m_tick = 0;
    bool m_onion = false;
    int m_onionAnimation = 0;
    int m_onionFrame = 0;
    int m_onionEffect = 2;
    double m_onionOpacity = 0.74;
    bool m_secondaryCross = true;
    bool m_showAttack = true;
    bool m_showCollision = true;
    QColor m_defaultAttack{255, 128, 0};
    QColor m_defaultCollision{0, 255, 255};
    bool m_settingsApplied = false;
    bool m_following = false;
    mutable QHash<int, QImage> m_spriteCache;
    QImage onionImage(const QImage& sprite) const;
    mutable qint64 m_onionSourceKey = -1;
    mutable int m_onionCachedEffect = -1;
    mutable QImage m_onionImage;
};

} // namespace OFS
