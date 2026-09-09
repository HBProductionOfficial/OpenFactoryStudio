#include "engines/mugen/emulator/EmulatorWidget.h"

#include "core/app/AppContext.h"
#include "core/project/Project.h"
#include "core/settings/Config.h"
#include "core/sprites/SpriteFile.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QGroupBox>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QSpinBox>

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

EmulatorWidget::~EmulatorWidget() = default;

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
    m_training = panel->findChild<QGroupBox*>(QStringLiteral("emul_training"));
    fill(m_mode, {tr("Versus"), tr("Training")});
    fill(m_enemy, {tr("Self"), tr("Random")});
    fill(panel->findChild<QComboBox*>(QStringLiteral("emul_explod_mode")),
         {tr("Engine's default"), tr("M.U.G.E.N. 1.0"), tr("M.U.G.E.N. 1.1"), tr("MUGENext"), tr("Player's version")});
    fill(panel->findChild<QComboBox*>(QStringLiteral("emul_dummy_ctrl")), {tr("Cooperative"), tr("AI"), tr("Manual")});
    fill(panel->findChild<QComboBox*>(QStringLiteral("emul_dummy_mode")),
         {tr("Stand"), tr("Crouch"), tr("Jump"), tr("W Jump")});
    fill(panel->findChild<QComboBox*>(QStringLiteral("emul_guard_mode")), {tr("None"), tr("Auto")});
    fill(panel->findChild<QComboBox*>(QStringLiteral("emul_distance")),
         {tr("Any"), tr("Close"), tr("Medium"), tr("Far")});
    fill(panel->findChild<QComboBox*>(QStringLiteral("emul_button_jam")),
         {tr("None"), tr("A"), tr("B"), tr("C"), tr("X"), tr("Y"), tr("Z"), tr("Start")});
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
    m_error.clear();
    if (m_project.typeId() != QLatin1String("char")) {
        m_error = tr("The emulator runs character projects.");
        return false;
    }
    EmulatorSession::Options options;
    options.engineDir = m_engineDir;
    options.training = m_mode && m_mode->currentIndex() == 1;
    options.randomEnemy = m_enemy && m_enemy->currentIndex() == 1;
    options.rounds = m_rounds ? m_rounds->value() : 0;
    options.time = m_time ? m_time->value() : 0;
    options.seed = std::uint32_t(QDateTime::currentMSecsSinceEpoch() & 0x7fffffff) | 1u;
    if (!m_session.start(m_project, options, &m_error))
        return false;
    m_renderer.clearCache();
    if (!m_mixer) {
        m_mixer = std::make_unique<AudioMixer>(qEnvironmentVariable("OFS_AUDIO_BACKEND") == QLatin1String("null")
                                                   ? AudioEngine::Backend::Null
                                                   : AudioEngine::Backend::System);
    }
    m_mixer->stopAll();
    m_frame = m_renderer.render(m_session, m_debugInfo && m_debugInfo->isChecked());
    m_ticksRun = 0;
    m_slowBudget = 0;
    m_clock.start();
    m_timer.start();
    setFocus();
    if (m_start)
        m_start->setToolTip(tr("Stop emulator"));
    update();
    Q_EMIT statusChanged();
    return true;
}

void EmulatorWidget::stop()
{
    m_timer.stop();
    if (m_mixer)
        m_mixer->stopAll();
    if (m_start)
        m_start->setToolTip(tr("Start emulator"));
    Q_EMIT statusChanged();
}

void EmulatorWidget::advance()
{
    // 60 ticks a second of real time, catching up after a slow frame (a few at most).
    const qint64 due = m_clock.elapsed() * TicksPerSecond / 1000;
    int ran = 0;
    bool ticked = false;
    while (m_ticksRun < due && ran < 4) {
        m_slowBudget += m_session.fightScreen().gameSpeed(m_session.match());
        for (; m_slowBudget >= 1; m_slowBudget -= 1) {
            tick();
            ticked = true;
        }
        ++m_ticksRun;
        ++ran;
    }
    m_ticksRun = std::max(m_ticksRun, due - 4);
    if (ticked) {
        m_frame = m_renderer.render(m_session, m_debugInfo && m_debugInfo->isChecked());
        update();
    }
}

void EmulatorWidget::tick()
{
    m_session.tick(inputOf(1), inputOf(2));
    if (m_mixer)
        m_session.playSounds(*m_mixer);
}

void EmulatorWidget::step()
{
    if (!m_session.isStarted())
        return;
    tick();
    m_frame = m_renderer.render(m_session, m_debugInfo && m_debugInfo->isChecked());
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
    const QSize fitted = game.scaled(size(), Qt::KeepAspectRatio);
    return QRect(QPoint((width() - fitted.width()) / 2, (height() - fitted.height()) / 2), fitted);
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
        if (!start() && !m_error.isEmpty())
            QMessageBox::warning(this, tr("Emulator"), m_error);
        return true;
    }
    if (actionId == QLatin1String("debug_stop") && isRunning()) {
        stop();
        return true;
    }
    return false;
}

} // namespace OFS::Mugen
