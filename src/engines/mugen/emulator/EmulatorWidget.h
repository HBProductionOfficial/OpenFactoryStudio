#pragma once

#include "core/audio/AudioMixer.h"
#include "core/engine/EngineWorkspace.h"
#include "engines/mugen/emulator/EmulatorDebugger.h"
#include "engines/mugen/emulator/EmulatorRenderer.h"
#include "engines/mugen/emulator/EmulatorSession.h"

#include <QElapsedTimer>
#include <QImage>
#include <QPointer>
#include <QSet>
#include <QTimer>

#include <memory>

class QAbstractButton;
class QEventLoop;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLineEdit;
class QSpinBox;

namespace OFS {
class AppContext;
class Project;
} // namespace OFS

namespace OFS::Mugen {

// The Emulator editor's workspace: the match drawn on the game's screen and played with the keyboard.
class EmulatorWidget : public EngineWorkspace
{
    Q_OBJECT

public:
    EmulatorWidget(Project& project, AppContext& context, QWidget* parent = nullptr);
    ~EmulatorWidget() override;

    void bindPanel(QWidget* panel) override;
    QStringList statusSections() const override;
    bool perform(const QString& actionId) override;
    void applySettings() override;
    // 0 fits the game's screen to the workspace, else whole times its size.
    int zoom() const { return m_zoom; }
    void setZoom(int zoom);
    EmulatorRenderer::Options renderOptions() const;

    // Debugging: every match is a debug session.
    Debug::SessionState debugState() const override { return m_debugState; }
    void setBreakpoints(Debug::BreakpointList* breakpoints) override;
    QList<Debug::Variable> debugVariables() const override;
    std::optional<Debug::Variable> debugVariable(const QString& path) const override;
    QList<Debug::StackFrame> debugStack() const override;
    const EmulatorDebugger& debugger() const { return m_debugger; }

    bool isRunning() const { return m_debugState != Debug::SessionState::None; }
    bool start();
    void stop();
    void restart();
    QString lastError() const { return m_error; }
    void step();
    const EmulatorSession& session() const { return *m_session; }
    const QImage& frame() const { return m_frame; }
    // Made when a match starts (null before, silent without a sound card).
    AudioMixer* mixer() const { return m_mixer.get(); }
    QString engineDir() const;
    void setEngineDir(const QString& dir);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    void advance();
    // Returns false when the workspace went away during the tick (a break's event loop).
    bool tick();
    // The debugger may stop at a line of the match, in an event loop until it goes on.
    void onLine(const Runtime::Character& character, const Runtime::Controller& controller, int line);
    void interrupted();
    void resumeRun(EmulatorDebugger::Step step);
    void startTimer();
    void finishStop();
    bool handleRequests();
    void setDebugState(Debug::SessionState state);
    void updateTraining();
    TrainingOptions trainingOptions() const;
    Runtime::InputState inputOf(int player) const;
    QRect screenRect() const;

    Project& m_project;
    AppContext& m_context;
    std::shared_ptr<EmulatorSession> m_session = std::make_shared<EmulatorSession>();
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
    EmulatorDebugger m_debugger;
    Debug::SessionState m_debugState = Debug::SessionState::None;
    QPointer<Debug::BreakpointList> m_breakpoints;
    QEventLoop* m_breakLoop = nullptr;
    bool m_stopRequested = false;
    bool m_restartRequested = false;
    int m_refresh = 0;
    EmulatorRenderer m_renderer;
    std::unique_ptr<AudioMixer> m_mixer;
    QImage m_frame;
    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_ticksRun = 0;
    float m_slowBudget = 0;
    QSet<int> m_keys;
    QPoint m_mouse;
    QString m_error;
    QString m_engineDir;
    int m_zoom = 0;

    QPointer<QAbstractButton> m_start;
    QPointer<QWidget> m_engine;
    QPointer<QComboBox> m_mode;
    QPointer<QComboBox> m_enemy;
    QPointer<QSpinBox> m_rounds;
    QPointer<QSpinBox> m_time;
    QPointer<QCheckBox> m_debugInfo;
    QPointer<QComboBox> m_explodMode;
    QPointer<QGroupBox> m_training;
    QPointer<QComboBox> m_dummyCtrl;
    QPointer<QComboBox> m_dummyMode;
    QPointer<QComboBox> m_guardMode;
    QPointer<QComboBox> m_distance;
    QPointer<QComboBox> m_buttonJam;
    QPointer<QSpinBox> m_aiLevel;
};

} // namespace OFS::Mugen
