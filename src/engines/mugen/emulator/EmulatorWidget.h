#pragma once

#include "core/audio/AudioMixer.h"
#include "core/engine/EngineWorkspace.h"
#include "engines/mugen/emulator/EmulatorRenderer.h"
#include "engines/mugen/emulator/EmulatorSession.h"

#include <QElapsedTimer>
#include <QImage>
#include <QPointer>
#include <QSet>
#include <QTimer>

#include <memory>

class QAbstractButton;
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

    bool isRunning() const { return m_timer.isActive(); }
    bool start();
    void stop();
    QString lastError() const { return m_error; }
    void step();
    const EmulatorSession& session() const { return m_session; }
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
    void tick();
    void updateTraining();
    TrainingOptions trainingOptions() const;
    Runtime::InputState inputOf(int player) const;
    QRect screenRect() const;

    Project& m_project;
    AppContext& m_context;
    EmulatorSession m_session;
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
