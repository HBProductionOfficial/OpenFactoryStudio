#include "engines/mugen/emulator/EmulatorWidget.h"

#include "core/app/AppContext.h"
#include "core/project/Project.h"
#include "core/settings/Config.h"
#include "core/sprites/SpriteFile.h"

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

constexpr int TicksPerSecond = 60;

const char* const EngineKey = "emul_engine";

const QHash<QString, QString>& defaultKeys()
{
    static const QHash<QString, QString> keys = {
        {QStringLiteral("p1_left"), QStringLiteral("A")},      {QStringLiteral("p1_right"), QStringLiteral("D")},
        {QStringLiteral("p1_up"), QStringLiteral("W")},        {QStringLiteral("p1_down"), QStringLiteral("S")},
        {QStringLiteral("p1_a"), QStringLiteral("F")},         {QStringLiteral("p1_b"), QStringLiteral("G")},
        {QStringLiteral("p1_c"), QStringLiteral("H")},         {QStringLiteral("p1_x"), QStringLiteral("R")},
        {QStringLiteral("p1_y"), QStringLiteral("T")},         {QStringLiteral("p1_z"), QStringLiteral("Y")},
        {QStringLiteral("p1_start"), QStringLiteral("U")},     {QStringLiteral("p2_left"), QStringLiteral("Left")},
        {QStringLiteral("p2_right"), QStringLiteral("Right")}, {QStringLiteral("p2_up"), QStringLiteral("Up")},
        {QStringLiteral("p2_down"), QStringLiteral("Down")},   {QStringLiteral("p2_a"), QStringLiteral("4")},
        {QStringLiteral("p2_b"), QStringLiteral("5")},         {QStringLiteral("p2_c"), QStringLiteral("6")},
        {QStringLiteral("p2_x"), QStringLiteral("7")},         {QStringLiteral("p2_y"), QStringLiteral("8")},
        {QStringLiteral("p2_z"), QStringLiteral("9")},         {QStringLiteral("p2_start"), QStringLiteral("0")},
    };
    return keys;
}

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
    setAttribute(Qt::WA_OpaquePaintEvent);
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(1000 / TicksPerSecond / 2);
    connect(&m_timer, &QTimer::timeout, this, &EmulatorWidget::advance);
    m_engineDir = EmulatorSession::engineFolderOf(project.filePath());
    if (m_engineDir.isEmpty())
        m_engineDir = m_context.engineConfig(QStringLiteral("mugen")).value(QLatin1String(EngineKey));
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
    fill(panel->findChild<QComboBox*>(QStringLiteral("emul_screen")),
         {tr("Menu"), tr("Versus"), tr("Continue"), tr("Victory"), tr("Win"), tr("Survival Results")});
    if (auto* edit = qobject_cast<QLineEdit*>(m_engine.data())) {
        edit->setText(QDir::toNativeSeparators(m_engineDir));
        connect(edit, &QLineEdit::editingFinished, this, [this, edit] { setEngineDir(edit->text()); });
    }
    if (m_start) {
        m_start->disconnect(this);
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
    m_context.engineConfig(QStringLiteral("mugen")).setValue(QLatin1String(EngineKey), m_engineDir);
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
    if (m_project.typeId() != QLatin1String("char")) {
        m_error = tr("The emulator runs character projects.");
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
    if (!m_mixer) {
        m_mixer = std::make_unique<AudioMixer>(qEnvironmentVariable("OFS_AUDIO_BACKEND") == QLatin1String("null")
                                                   ? AudioEngine::Backend::Null
                                                   : AudioEngine::Backend::System);
    }
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
        m_start->setToolTip(tr("Stop emulator"));
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
        m_start->setToolTip(tr("Start emulator"));
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

void EmulatorWidget::step()
{
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

InputState EmulatorWidget::inputOf(int player) const
{
    const Config& config = m_context.engineConfig(QStringLiteral("mugen"));
    auto held = [&](const char* button) {
        const QString key = QStringLiteral("p%1_%2").arg(player).arg(QLatin1String(button));
        const QKeySequence sequence = QKeySequence::fromString(config.value(key, defaultKeys().value(key)));
        return !sequence.isEmpty() && m_keys.contains(sequence[0].key());
    };
    InputState in;
    in.up = held("up");
    in.down = held("down");
    in.left = held("left");
    in.right = held("right");
    in.a = held("a");
    in.b = held("b");
    in.c = held("c");
    in.x = held("x");
    in.y = held("y");
    in.z = held("z");
    in.s = held("start");
    return in;
}

QRect EmulatorWidget::screenRect() const
{
    const QSize game = EmulatorRenderer::Screen;
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
    if (event->angleDelta().y() > 0)
        setZoom(current + 1);
    else if (event->angleDelta().y() < 0)
        setZoom(current > 1 ? current - 1 : 0);
    event->accept();
}

EmulatorRenderer::Options EmulatorWidget::renderOptions() const
{
    EmulatorRenderer::Options options;
    options.debugInfo = m_debugInfo && m_debugInfo->isChecked();
    const Config& config = m_context.config();
    options.attackBoxes = config.boolValue(QStringLiteral("show_attack_box"), true);
    options.collisionBoxes = config.boolValue(QStringLiteral("show_collision_box"), true);
    const Config& engine = m_context.engineConfig(QStringLiteral("mugen"));
    auto color = [&engine](const char* key, const QColor& fallback) {
        const QStringList parts = engine.value(QLatin1String(key)).split(QLatin1Char(','));
        if (parts.size() != 3)
            return fallback;
        return QColor(parts[0].trimmed().toInt(), parts[1].trimmed().toInt(), parts[2].trimmed().toInt());
    };
    options.attackColor = color("clsn_default_attack", options.attackColor);
    options.collisionColor = color("clsn_default_collision", options.collisionColor);
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
    QPainter p(this);
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
        m_mouse = QPoint((event->position().x() - screen.x()) * EmulatorRenderer::Screen.width() / screen.width(),
                         (event->position().y() - screen.y()) * EmulatorRenderer::Screen.height() / screen.height());
    Q_EMIT statusChanged();
}

QStringList EmulatorWidget::statusSections() const
{
    return {tr("Position: %1, %2").arg(m_mouse.x()).arg(m_mouse.y()), QString(),
            tr("Renderer: %1").arg(QStringLiteral("OpenGL"))};
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
