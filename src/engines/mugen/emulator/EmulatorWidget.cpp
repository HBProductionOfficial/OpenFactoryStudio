#include "engines/mugen/emulator/EmulatorWidget.h"

#include "core/GameTime.h"
#include "core/app/AppContext.h"
#include "core/project/Project.h"
#include "core/settings/Config.h"
#include "core/sprites/SpriteFile.h"
#include "core/widgets/RenderSurface.h"
#include "engines/mugen/MugenEngine.h"
#include "engines/mugen/MugenTypes.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QEventLoop>
#include <QGroupBox>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QSpinBox>
#include <QWheelEvent>

namespace OFS::Mugen {

using namespace Runtime;

namespace {

const char* const EngineKey = "emul_engine";

void fill(QComboBox* combo, const QStringList& items)
{
    if (combo && combo->count() == 0)
        combo->addItems(items);
}

} // namespace

EmulatorWidget::EmulatorWidget(Project& project, AppContext& context, QWidget* parent)
    : EngineWorkspace(parent)
    , m_project(project)
    , m_context(context)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    m_surface = Rendering::attachSurface(this, [this](QPainter& p) { paintScreen(p); });
    setAttribute(Qt::WA_OpaquePaintEvent);
    connect(&m_context.engineConfig(MugenEngine::Id), &Config::changed, this, [this] {
        m_keyCodes.reset();
        m_renderOptions.reset();
    });
    connect(&m_context.config(), &Config::changed, this, [this] { m_renderOptions.reset(); });
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(1000 / TicksPerSecond / 2);
    connect(&m_timer, &QTimer::timeout, this, &EmulatorWidget::advance);
    m_engineDir = EmulatorSession::engineFolderOf(project.filePath());
    if (m_engineDir.isEmpty())
        m_engineDir = m_context.engineConfig(MugenEngine::Id).value(QLatin1String(EngineKey));
    connect(&project, &Project::fileAdded, this, [this](ProjectFile* file) {
        if (auto* sprites = qobject_cast<SpriteFile*>(file))
            connect(sprites, &SpriteFile::documentChanged, this, [this] { m_renderer.clearCache(); });
    });
    for (ProjectFile* file : project.files()) {
        if (auto* sprites = qobject_cast<SpriteFile*>(file))
            connect(sprites, &SpriteFile::documentChanged, this, [this] { m_renderer.clearCache(); });
    }
}

EmulatorWidget::~EmulatorWidget()
{
    // End a break's event loop. The match finishes its tick on its own because it holds the session.
    m_alive.reset();
    if (m_breakLoop)
        m_breakLoop->quit();
}

void EmulatorWidget::bindPanel(QWidget* panel)
{
    if (!panel)
        return;
    m_start = panel->findChild<QAbstractButton*>(QStringLiteral("emul_start"));
    m_engine = panel->findChild<QWidget*>(QStringLiteral("emul_engine"));
    m_mode = panel->findChild<QComboBox*>(QStringLiteral("emul_match_mode"));
    m_enemy = panel->findChild<QComboBox*>(QStringLiteral("emul_enemy"));
    m_rounds = panel->findChild<QSpinBox*>(QStringLiteral("emul_rounds"));
    m_time = panel->findChild<QSpinBox*>(QStringLiteral("emul_time"));
    m_debugInfo = panel->findChild<QCheckBox*>(QStringLiteral("emul_debug_info"));
    m_explodMode = panel->findChild<QComboBox*>(QStringLiteral("emul_explod_mode"));
    m_screenCombo = panel->findChild<QComboBox*>(QStringLiteral("emul_screen"));
    m_training = panel->findChild<QGroupBox*>(QStringLiteral("emul_training"));
    m_dummyCtrl = panel->findChild<QComboBox*>(QStringLiteral("emul_dummy_ctrl"));
    m_dummyMode = panel->findChild<QComboBox*>(QStringLiteral("emul_dummy_mode"));
    m_guardMode = panel->findChild<QComboBox*>(QStringLiteral("emul_guard_mode"));
    m_distance = panel->findChild<QComboBox*>(QStringLiteral("emul_distance"));
    m_buttonJam = panel->findChild<QComboBox*>(QStringLiteral("emul_button_jam"));
    m_aiLevel = panel->findChild<QSpinBox*>(QStringLiteral("emul_ai_level"));
    fill(m_mode, {tr("Versus"), tr("Training")});
    fill(m_enemy, {tr("Self"), tr("Random")});
    fill(m_explodMode,
         {tr("Engine's default"), tr("M.U.G.E.N. 1.0"), tr("M.U.G.E.N. 1.1"), tr("MUGENext"), tr("Player's version")});
    fill(m_dummyCtrl, {tr("Cooperative"), tr("AI"), tr("Manual")});
    fill(m_dummyMode, {tr("Stand"), tr("Crouch"), tr("Jump"), tr("W Jump")});
    fill(m_guardMode, {tr("None"), tr("Auto")});
    fill(m_distance, {tr("Any"), tr("Close"), tr("Medium"), tr("Far")});
    fill(m_buttonJam, {tr("None"), tr("A"), tr("B"), tr("C"), tr("X"), tr("Y"), tr("Z"), tr("Start")});
    if (m_aiLevel && m_aiLevel->value() == 0)
        m_aiLevel->setValue(4);
    auto applyTraining = [this] {
        if (m_session->isStarted())
            m_session->setTrainingOptions(trainingOptions());
    };
    for (QComboBox* combo :
         {m_dummyCtrl.data(), m_dummyMode.data(), m_guardMode.data(), m_distance.data(), m_buttonJam.data()}) {
        if (combo)
            connect(combo, &QComboBox::currentIndexChanged, this, applyTraining);
    }
    if (m_aiLevel)
        connect(m_aiLevel, &QSpinBox::valueChanged, this, applyTraining);
    fill(m_screenCombo, {tr("Menu"), tr("Versus"), tr("Continue"), tr("Victory"), tr("Win"), tr("Survival Results")});
    if (m_screenCombo) {
        m_screenCombo->setEnabled(m_project.typeId() == MugenType::Motif);
        connect(m_screenCombo, &QComboBox::currentIndexChanged, this, [this] {
            if (m_motifMode && isRunning())
                restart();
        });
    }
    if (auto* edit = qobject_cast<QLineEdit*>(m_engine.data())) {
        edit->setText(QDir::toNativeSeparators(m_engineDir));
        connect(edit, &QLineEdit::editingFinished, this, [this, edit] { setEngineDir(edit->text()); });
    }
    if (m_start) {
        m_start->disconnect(this);
        m_start->setEnabled(!isRunning());
        connect(m_start, &QAbstractButton::clicked, this, [this] {
            if (isRunning())
                stop();
            else if (!start() && !m_error.isEmpty())
                QMessageBox::warning(this, tr("Emulator"), m_error);
        });
    }
    if (m_mode)
        connect(m_mode, &QComboBox::currentIndexChanged, this, &EmulatorWidget::updateTraining);
    updateTraining();
}

TrainingOptions EmulatorWidget::trainingOptions() const
{
    TrainingOptions o;
    auto index = [](const QPointer<QComboBox>& combo) {
        return combo ? std::max(0, combo->currentIndex()) : 0;
    };
    o.control = TrainingOptions::Control(std::min(index(m_dummyCtrl), 2));
    o.mode = TrainingOptions::Mode(std::min(index(m_dummyMode), 3));
    o.guard = TrainingOptions::Guard(std::min(index(m_guardMode), 1));
    o.distance = TrainingOptions::Distance(std::min(index(m_distance), 3));
    o.buttonJam = std::min(index(m_buttonJam), 7);
    o.aiLevel = m_aiLevel ? std::clamp(m_aiLevel->value(), 1, 8) : 4;
    return o;
}

void EmulatorWidget::updateTraining()
{
    if (m_training)
        m_training->setEnabled(m_mode && m_mode->currentIndex() == 1);
}

QString EmulatorWidget::engineDir() const
{
    return m_engineDir;
}

void EmulatorWidget::setEngineDir(const QString& dir)
{
    m_engineDir = QDir::fromNativeSeparators(dir.trimmed());
    m_context.engineConfig(MugenEngine::Id).setValue(QLatin1String(EngineKey), m_engineDir);
    if (auto* edit = qobject_cast<QLineEdit*>(m_engine.data()))
        edit->setText(QDir::toNativeSeparators(m_engineDir));
}

bool EmulatorWidget::start()
{
    if (m_breakLoop) {
        m_restartRequested = true;
        m_breakLoop->quit();
        return true;
    }
    m_error.clear();
    if (!m_mixer) {
        m_mixer = std::make_unique<AudioMixer>(qEnvironmentVariable("OFS_AUDIO_BACKEND") == QLatin1String("null")
                                                   ? AudioEngine::Backend::Null
                                                   : AudioEngine::Backend::System);
    }
    if (m_project.typeId() == MugenType::Storyboard) {
        if (!m_storyboard.start(m_project, m_project.filePath(), m_engineDir, &m_error))
            return false;
        m_storyMode = true;
        m_mixer->stopAll();
        m_frame = m_storyboard.render();
        m_stopRequested = m_restartRequested = false;
        startTimer();
        setFocus();
        if (m_start)
            m_start->setEnabled(false);
        update();
        setDebugState(Debug::SessionState::Running);
        Q_EMIT statusChanged();
        return true;
    }
    m_storyMode = false;
    if (m_project.typeId() == MugenType::Motif) {
        const int screen = m_screenCombo ? std::clamp(m_screenCombo->currentIndex(), 0, 5) : 0;
        if (!m_motif.start(m_project, m_project.filePath(), m_engineDir, MotifScreen::Screen(screen), &m_error))
            return false;
        m_motifMode = true;
        m_mixer->stopAll();
        m_frame = m_motif.render();
        m_stopRequested = m_restartRequested = false;
        startTimer();
        setFocus();
        if (m_start)
            m_start->setEnabled(false);
        update();
        setDebugState(Debug::SessionState::Running);
        Q_EMIT statusChanged();
        return true;
    }
    m_motifMode = false;
    if (!EmulatorSession::canPlay(m_project.typeId())) {
        m_error = tr("The emulator runs character, stage, fight screen, storyboard and motif projects.");
        return false;
    }
    EmulatorSession::Options options;
    options.engineDir = m_engineDir;
    options.training = m_mode && m_mode->currentIndex() == 1;
    options.trainingOptions = trainingOptions();
    options.explodMode = m_explodMode ? std::max(0, m_explodMode->currentIndex()) : 0;
    options.randomEnemy = m_enemy && m_enemy->currentIndex() == 1;
    options.rounds = m_rounds ? m_rounds->value() : 0;
    options.time = m_time ? m_time->value() : 0;
    options.seed = std::uint32_t(QDateTime::currentMSecsSinceEpoch() & 0x7fffffff) | 1u;
    if (!m_session->start(m_project, options, &m_error))
        return false;
    m_renderer.clearCache();
    m_mixer->stopAll();
    m_frame = m_renderer.render(*m_session, renderOptions());
    m_debugger.setBreakpoints(m_breakpoints);
    m_debugger.reset();
    m_stopRequested = m_restartRequested = false;
    std::weak_ptr<bool> alive = m_alive;
    m_session->match().setLineObserver([this, alive](const Character& c, const Controller& controller, int line) {
        if (!alive.expired())
            onLine(c, controller, line);
    });
    m_slowBudget = 0;
    startTimer();
    setFocus();
    if (m_start)
        m_start->setEnabled(false); // same as Fighter Factory: Stop is on the Debug panel
    update();
    setDebugState(Debug::SessionState::Running);
    Q_EMIT statusChanged();
    return true;
}

void EmulatorWidget::startTimer()
{
    m_ticksRun = 0;
    m_clock.start();
    m_timer.start();
    setFocus();
}

void EmulatorWidget::stop()
{
    // In a break, the match ends its tick first (without stopping again).
    if (m_breakLoop) {
        m_stopRequested = true;
        m_debugState = Debug::SessionState::None;
        m_breakLoop->quit();
        return;
    }
    finishStop();
}

void EmulatorWidget::restart()
{
    if (m_breakLoop) {
        m_restartRequested = true;
        m_debugState = Debug::SessionState::None;
        m_breakLoop->quit();
        return;
    }
    finishStop();
    if (!start() && !m_error.isEmpty())
        QMessageBox::warning(this, tr("Emulator"), m_error);
}

void EmulatorWidget::finishStop()
{
    m_timer.stop();
    m_stopRequested = false;
    if (m_mixer)
        m_mixer->stopAll();
    if (m_start)
        m_start->setEnabled(true);
    setDebugState(Debug::SessionState::None);
    Q_EMIT debugLineChanged(QString(), 0);
    Q_EMIT statusChanged();
}

bool EmulatorWidget::handleRequests()
{
    if (m_restartRequested) {
        m_restartRequested = false;
        finishStop();
        if (!start() && !m_error.isEmpty())
            QMessageBox::warning(this, tr("Emulator"), m_error);
        return true;
    }
    if (m_stopRequested) {
        finishStop();
        return true;
    }
    return false;
}

void EmulatorWidget::setDebugState(Debug::SessionState state)
{
    if (m_debugState == state)
        return;
    m_debugState = state;
    Q_EMIT debugStateChanged();
}

void EmulatorWidget::onLine(const Character& character, const Controller& controller, int line)
{
    if (m_debugState != Debug::SessionState::Running || m_breakLoop)
        return;
    if (!m_debugger.atLine(*m_session, character, controller, line))
        return;
    interrupted();
    // The match waits here until Continue, a step, Stop or Restart.
    QEventLoop loop;
    m_breakLoop = &loop;
    std::weak_ptr<bool> alive = m_alive;
    loop.exec();
    if (alive.expired())
        return;
    m_breakLoop = nullptr;
}

void EmulatorWidget::interrupted()
{
    m_timer.stop();
    setDebugState(Debug::SessionState::Interrupted);
    const std::optional<EmulatorDebugger::Location>& at = m_debugger.location();
    Q_EMIT debugLineChanged(at ? at->file : QString(), at ? at->line : 0);
    Q_EMIT debugVariablesChanged();
    m_frame = m_renderer.render(*m_session, renderOptions());
    update();
}

void EmulatorWidget::resumeRun(EmulatorDebugger::Step step)
{
    if (m_debugState != Debug::SessionState::Interrupted)
        return;
    m_debugger.resume(step);
    setDebugState(Debug::SessionState::Running);
    Q_EMIT debugLineChanged(QString(), 0);
    if (m_breakLoop)
        m_breakLoop->quit();
    else
        startTimer();
}

void EmulatorWidget::setBreakpoints(Debug::BreakpointList* breakpoints)
{
    m_breakpoints = breakpoints;
    m_debugger.setBreakpoints(breakpoints);
}

QList<Debug::Variable> EmulatorWidget::debugVariables() const
{
    if (m_motifMode) {
        Debug::Variable screen{QStringLiteral("screen"), tr("Screen"), QString(), QString(), {}};
        screen.children << Debug::Variable{QStringLiteral("screen.time"),
                                           QStringLiteral("time"),
                                           QString::number(m_motif.time()),
                                           QStringLiteral("int"),
                                           {}};
        screen.children << Debug::Variable{QStringLiteral("screen.menu"),
                                           QStringLiteral("menu"),
                                           QString::number(m_motif.menuIndex()),
                                           QStringLiteral("int"),
                                           {}};
        return {screen};
    }
    if (m_storyMode) {
        Debug::Variable story{QStringLiteral("story"), tr("Storyboard"), QString(), QString(), {}};
        const auto& scenes = m_storyboard.scenes();
        const int index = m_storyboard.sceneIndex();
        story.children << Debug::Variable{QStringLiteral("story.scene"),
                                          QStringLiteral("scene"),
                                          index < scenes.size() ? scenes[index].name : QString(),
                                          QStringLiteral("string"),
                                          {}};
        story.children << Debug::Variable{QStringLiteral("story.time"),
                                          QStringLiteral("time"),
                                          QString::number(m_storyboard.sceneTime()),
                                          QStringLiteral("int"),
                                          {}};
        return {story};
    }
    return EmulatorDebugger::variables(*m_session);
}

std::optional<Debug::Variable> EmulatorWidget::debugVariable(const QString& path) const
{
    return EmulatorDebugger::variable(*m_session, path);
}

QList<Debug::StackFrame> EmulatorWidget::debugStack() const
{
    return m_debugger.stack(*m_session);
}

void EmulatorWidget::advance()
{
    // 60 ticks a second of real time, catching up after a slow frame (a few at most).
    const qint64 due = m_clock.elapsed() * TicksPerSecond / 1000;
    int ran = 0;
    bool ticked = false;
    if (m_storyMode || m_motifMode) {
        while (m_ticksRun < due && ran < 4 && m_debugState == Debug::SessionState::Running) {
            if (m_motifMode)
                motifTick();
            else
                storyTick();
            ticked = true;
            ++m_ticksRun;
            ++ran;
        }
        m_ticksRun = std::max(m_ticksRun, due - 4);
        if (ticked)
            update();
        return;
    }
    while (m_ticksRun < due && ran < 4 && m_debugState == Debug::SessionState::Running) {
        m_slowBudget += m_session->fightScreen().gameSpeed(m_session->match());
        for (; m_slowBudget >= 1; m_slowBudget -= 1) {
            if (m_debugger.beforeFrame(*m_session)) {
                m_slowBudget = 0;
                interrupted();
                return;
            }
            if (!tick())
                return;
            if (handleRequests())
                return;
            ticked = true;
            if (m_debugState != Debug::SessionState::Running)
                break;
        }
        ++m_ticksRun;
        ++ran;
    }
    m_ticksRun = std::max(m_ticksRun, due - 4);
    if (ticked) {
        m_frame = m_renderer.render(*m_session, renderOptions());
        update();
        if (++m_refresh % 6 == 0)
            Q_EMIT debugVariablesChanged();
    }
    if (m_debugState == Debug::SessionState::Running && !m_timer.isActive())
        startTimer();
}

bool EmulatorWidget::tick()
{
    // The session and the workspace may part during a break, so keep the session until the tick ends.
    const std::shared_ptr<EmulatorSession> session = m_session;
    const std::weak_ptr<bool> alive = m_alive;
    session->tick(inputOf(1), inputOf(2));
    if (alive.expired())
        return false;
    m_debugger.afterFrame();
    if (m_mixer)
        session->playSounds(*m_mixer);
    return true;
}

void EmulatorWidget::storyTick()
{
    m_storyboard.tick();
    if (m_mixer)
        m_storyboard.playSounds(*m_mixer);
    m_frame = m_storyboard.render();
    if (m_storyboard.isFinished())
        finishStop();
}

void EmulatorWidget::motifTick()
{
    m_motif.tick(inputOf(1));
    if (m_mixer)
        m_motif.playSounds(*m_mixer);
    m_frame = m_motif.render();
}

void EmulatorWidget::step()
{
    if (m_motifMode) {
        if (m_debugState != Debug::SessionState::None)
            motifTick();
        return;
    }
    if (m_storyMode) {
        if (m_debugState != Debug::SessionState::None)
            storyTick();
        return;
    }
    if (!m_session->isStarted() || m_debugState == Debug::SessionState::None)
        return;
    if (m_debugger.beforeFrame(*m_session)) {
        interrupted();
        return;
    }
    if (!tick() || handleRequests())
        return;
    m_frame = m_renderer.render(*m_session, renderOptions());
}

namespace {

constexpr const char* kButtons[] = {"up", "down", "left", "right", "a", "b", "c", "x", "y", "z", "start"};

} // namespace

InputState EmulatorWidget::inputOf(int player) const
{
    if (!m_keyCodes) {
        const Config& config = m_context.engineConfig(MugenEngine::Id);
        KeyCodes codes{};
        for (int p = 0; p < 2; ++p) {
            for (std::size_t b = 0; b < std::size(kButtons); ++b) {
                const QString key = QStringLiteral("p%1_%2").arg(p + 1).arg(QLatin1String(kButtons[b]));
                const QKeySequence sequence = QKeySequence::fromString(config.value(key));
                codes[std::size_t(p)][b] = sequence.isEmpty() ? 0 : int(sequence[0].key());
            }
        }
        m_keyCodes = codes;
    }
    const auto& codes = (*m_keyCodes)[std::size_t(std::clamp(player, 1, 2) - 1)];
    auto held = [&](std::size_t button) {
        return codes[button] != 0 && m_keys.contains(codes[button]);
    };
    InputState in;
    in.up = held(0);
    in.down = held(1);
    in.left = held(2);
    in.right = held(3);
    in.a = held(4);
    in.b = held(5);
    in.c = held(6);
    in.x = held(7);
    in.y = held(8);
    in.z = held(9);
    in.s = held(10);
    return in;
}

QRect EmulatorWidget::screenRect() const
{
    const QSize game = m_frame.isNull() ? EmulatorRenderer::Screen : m_frame.size();
    const QSize fitted = m_zoom > 0 ? game * m_zoom : game.scaled(size(), Qt::KeepAspectRatio);
    return QRect(QPoint((width() - fitted.width()) / 2, (height() - fitted.height()) / 2), fitted);
}

void EmulatorWidget::setZoom(int zoom)
{
    m_zoom = std::clamp(zoom, 0, 8);
    update();
}

void EmulatorWidget::wheelEvent(QWheelEvent* event)
{
    const int fitted = std::max(1, screenRect().width() / EmulatorRenderer::Screen.width());
    const int current = m_zoom > 0 ? m_zoom : fitted;
    const int steps = m_wheel.take(event); // parts of notches add up (touchpads)
    if (steps > 0)
        setZoom(current + steps);
    else if (steps < 0)
        setZoom(current + steps >= 1 ? current + steps : 0);
    event->accept();
}

EmulatorRenderer::Options EmulatorWidget::renderOptions() const
{
    if (!m_renderOptions)
        m_renderOptions = readRenderOptions();
    EmulatorRenderer::Options options = *m_renderOptions;
    options.debugInfo = m_debugInfo && m_debugInfo->isChecked();
    return options;
}

EmulatorRenderer::Options EmulatorWidget::readRenderOptions() const
{
    EmulatorRenderer::Options options;
    const Config& config = m_context.config();
    options.attackBoxes = config.boolValue(QStringLiteral("show_attack_box"), true);
    options.collisionBoxes = config.boolValue(QStringLiteral("show_collision_box"), true);
    const Config& engine = m_context.engineConfig(MugenEngine::Id);
    options.attackColor = engine.colorValue(QStringLiteral("clsn_default_attack"), options.attackColor);
    options.collisionColor = engine.colorValue(QStringLiteral("clsn_default_collision"), options.collisionColor);
    return options;
}

void EmulatorWidget::applySettings()
{
    if (m_session->isStarted()) {
        m_frame = m_renderer.render(*m_session, renderOptions());
        update();
    }
}

void EmulatorWidget::paintEvent(QPaintEvent*)
{
    if (m_surface) {
        Rendering::update(m_surface);
        return;
    }
    QPainter p(this);
    paintScreen(p);
}

void EmulatorWidget::paintScreen(QPainter& p)
{
    p.fillRect(rect(), Qt::black);
    if (!m_frame.isNull())
        p.drawImage(screenRect(), m_frame);
}

void EmulatorWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat())
        return;
    m_keys.insert(event->key());
}

void EmulatorWidget::keyReleaseEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat())
        return;
    m_keys.remove(event->key());
}

void EmulatorWidget::focusOutEvent(QFocusEvent* event)
{
    // Keys released elsewhere would stay held.
    m_keys.clear();
    EngineWorkspace::focusOutEvent(event);
}

void EmulatorWidget::mouseMoveEvent(QMouseEvent* event)
{
    const QRect screen = screenRect();
    if (!screen.isEmpty())
        m_mouse =
            QPoint(int((event->position().x() - screen.x()) * EmulatorRenderer::Screen.width() / screen.width()),
                   int((event->position().y() - screen.y()) * EmulatorRenderer::Screen.height() / screen.height()));
    Q_EMIT statusChanged();
}

QStringList EmulatorWidget::statusSections() const
{
    return {tr("Position: %1, %2").arg(m_mouse.x()).arg(m_mouse.y()), QString(),
            tr("Renderer: %1").arg(Rendering::backendName())};
}

bool EmulatorWidget::perform(const QString& actionId)
{
    if (actionId == QLatin1String("debug_start_emul")) {
        if (!isRunning() && !start() && !m_error.isEmpty())
            QMessageBox::warning(this, tr("Emulator"), m_error);
        return true;
    }
    if (!isRunning())
        return false;
    if (actionId == QLatin1String("debug_stop")) {
        stop();
    } else if (actionId == QLatin1String("debug_restart")) {
        restart();
    } else if (actionId == QLatin1String("debug_interrupt")) {
        if (m_debugState == Debug::SessionState::Running)
            m_debugger.interrupt();
    } else if (actionId == QLatin1String("debug_continue") || actionId == QLatin1String("debug_start")) {
        resumeRun(EmulatorDebugger::Step::None);
    } else if (actionId == QLatin1String("debug_step_into")) {
        resumeRun(EmulatorDebugger::Step::Into);
    } else if (actionId == QLatin1String("debug_step_over")) {
        resumeRun(EmulatorDebugger::Step::Over);
    } else if (actionId == QLatin1String("debug_step_out")) {
        resumeRun(EmulatorDebugger::Step::Out);
    } else if (actionId == QLatin1String("debug_step_frame")) {
        resumeRun(EmulatorDebugger::Step::Frame);
    } else {
        return false;
    }
    return true;
}

} // namespace OFS::Mugen
