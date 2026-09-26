#pragma once

#include "core/anims/AirDocument.h"
#include "core/bg/BgDocument.h"
#include "core/bg/BgRenderer.h"
#include "core/bg/BgSimulator.h"

#include <QPointer>
#include <QSet>
#include <QWidget>

#include <functional>
#include <memory>

class QTimer;

namespace OFS {

class CodeFile;
class Config;
class GraphicsView;
class Project;
class SpriteFile;
class TimelineWidget;

// Backgrounds editor workspace for one stage, motif or storyboard. The origin is the screen's top centre. Play runs the
// group at 60 ticks per second and the arrow keys move the camera.
class BackgroundsPage : public QWidget
{
    Q_OBJECT

public:
    enum class Mode
    {
        Stopped,
        Playing,
        Paused,
    };

    BackgroundsPage(CodeFile* file, Project* project, QWidget* parent = nullptr);
    ~BackgroundsPage() override;

    CodeFile* file() const { return m_file; }
    GraphicsView* view() const { return m_view; }
    TimelineWidget* timeline() const { return m_timeline; }
    const BgDocument& document() const { return m_doc; }
    bool isStage() const;

    // Index in document().groups(), -1 without groups.
    int group() const { return m_group; }
    void setGroup(int group);
    const BgGroup* currentGroup() const;
    // Current element, controller definition and controller, -1 for none.
    int current() const { return m_current; }
    void setCurrent(int element);
    const BgElement* element() const;
    int currentDefinition() const { return m_definition; }
    void setCurrentDefinition(int definition);
    const BgControllerDef* definition() const;
    int currentController() const { return m_controller; }
    void setCurrentController(int controller);
    const BgController* controller() const;

    QStringList lines() const;
    bool edit(const std::function<void(QStringList&)>& change);

    int tick() const { return m_tick; }
    void setTick(int tick);
    Mode mode() const { return m_mode; }
    void play();
    void pause();
    void stop();
    QPointF camera() const { return m_camera; }
    void moveCamera(double dx, double dy);

    void setHiddenElements(const QSet<int>& hidden);
    const QSet<int>& hiddenElements() const { return m_hidden; }

    QSizeF screenSize() const;

    void applySettings(const Config& config);
    QPoint cursorPosition() const { return m_cursor; }

Q_SIGNALS:
    void currentChanged();
    void documentChanged();
    void modeChanged();
    void tickChanged();
    void statusChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void reparse();
    void refresh();
    void clampSelection();
    SpriteFile* sprites() const;
    QString value(const QString& section, const QString& key) const;
    QList<double> numbers(const QString& section, const QString& key, int count, double fallback) const;

    QPointer<CodeFile> m_file;
    Project* m_project;
    GraphicsView* m_view;
    TimelineWidget* m_timeline;
    QTimer* m_timer;
    BgDocument m_doc;
    AirDocument m_air;
    std::unique_ptr<BgSimulator> m_simulator;
    std::unique_ptr<BgRenderer> m_renderer;
    QPointer<SpriteFile> m_spriteFile;
    int m_group = -1;
    int m_current = -1;
    int m_definition = -1;
    int m_controller = -1;
    int m_tick = 0;
    Mode m_mode = Mode::Stopped;
    QPointF m_camera;
    QSet<int> m_hidden;
    QList<int> m_imageElements;
    QPoint m_dragOffset;
    QPoint m_cursor;
    bool m_settingsApplied = false;
    bool m_secondaryCross = true;
    bool m_selectionShown = false;
};

} // namespace OFS
