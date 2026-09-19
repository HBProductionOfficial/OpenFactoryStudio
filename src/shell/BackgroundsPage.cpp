#include "shell/BackgroundsPage.h"

#include "core/GameTime.h"
#include "core/anims/AirEdits.h"
#include "core/bg/BgEdits.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/settings/Config.h"
#include "core/sprites/SpriteFile.h"
#include "core/widgets/GraphicsView.h"
#include "core/widgets/TimelineWidget.h"

#include <QKeyEvent>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>
#include <limits>
#include <utility>

namespace OFS {

BackgroundsPage::BackgroundsPage(CodeFile* file, Project* project, QWidget* parent)
    : QWidget(parent)
    , m_file(file)
    , m_project(project)
    , m_view(new GraphicsView(this))
    , m_timeline(new TimelineWidget(this))
    , m_timer(new QTimer(this))
{
    m_view->setObjectName(QStringLiteral("bgView"));
    m_view->setZoomLevels(GraphicsView::largeZoomRange());
    m_view->setZoomCenter(GraphicsView::ZoomCenter::Axis);
    m_view->setBackground(GraphicsView::Background::Color);
    m_view->installEventFilter(this);
    m_timeline->setTickMode(true);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_view, 1);
    layout->addWidget(m_timeline);

    m_timer->setInterval(TickIntervalMs);
    connect(m_timer, &QTimer::timeout, this, [this] {
        ++m_tick;
        refresh();
        Q_EMIT tickChanged();
    });
    connect(m_timeline, &TimelineWidget::tickClicked, this, &BackgroundsPage::setTick);
    connect(m_view, &GraphicsView::imagePressed, this, [this](int image) {
        if (image >= 0 && image < m_imageElements.size())
            setCurrent(m_imageElements.at(image));
    });
    connect(m_view, &GraphicsView::imageDragged, this, [this](int dx, int dy) {
        m_dragOffset += QPoint(dx, dy);
        refresh();
    });
    connect(m_view, &GraphicsView::dragFinished, this, [this] {
        const QPoint offset = std::exchange(m_dragOffset, QPoint());
        const BgElement* e = element();
        if (offset.isNull() || !e)
            return;
        const int header = e->section.headerLine;
        const QPointF start = e->start + QPointF(offset);
        edit([&](QStringList& lines) {
            BgEdits::setNumbers(lines, header, QStringLiteral("start"), {start.x(), start.y()}, {0, 0});
        });
    });
    connect(m_view, &GraphicsView::cursorMoved, this, [this](const QPoint& units) {
        m_cursor = units;
        Q_EMIT statusChanged();
    });
    if (m_file)
        connect(m_file->document(), &QTextDocument::contentsChanged, this, &BackgroundsPage::reparse);
    reparse();
}

BackgroundsPage::~BackgroundsPage()
{
    if (m_file)
        disconnect(m_file->document(), nullptr, this, nullptr);
    if (m_spriteFile)
        disconnect(m_spriteFile, nullptr, this, nullptr);
    m_view->removeEventFilter(this);
}

SpriteFile* BackgroundsPage::sprites() const
{
    if (!m_project)
        return nullptr;
    const QStringList files = m_project->editorFiles(QStringLiteral("sprites"));
    if (files.isEmpty() || files.first().isEmpty())
        return nullptr;
    if (auto* open = qobject_cast<SpriteFile*>(m_project->file(files.first())))
        return open;
    return m_project->spriteFile(files.first(), QStringLiteral("sprites"));
}

bool BackgroundsPage::isStage() const
{
    return m_project && m_project->typeId() == QLatin1String("stage");
}

QString BackgroundsPage::value(const QString& section, const QString& key) const
{
    const BgSection* s = m_doc.section(section);
    return s ? s->value(key).value_or(QString()) : QString();
}

QList<double> BackgroundsPage::numbers(const QString& section, const QString& key, int count, double fallback) const
{
    const BgSection* s = m_doc.section(section);
    return s ? s->numbers(key, count, fallback) : QList<double>(count, fallback);
}

QSizeF BackgroundsPage::screenSize() const
{
    // localcoord lives in [StageInfo] for stages, [Info] for motifs and [SceneDef] for storyboards.
    for (const char* section : {"StageInfo", "Info", "SceneDef"}) {
        const BgSection* s = m_doc.section(QLatin1String(section));
        if (s && s->value(QStringLiteral("localcoord"))) {
            const QList<double> size = s->numbers(QStringLiteral("localcoord"), 2, 0);
            if (size[0] > 0 && size[1] > 0)
                return QSizeF(size[0], size[1]);
        }
    }
    return QSizeF(320, 240);
}

QStringList BackgroundsPage::lines() const
{
    return m_file ? AirEdits::lines(m_file->document()) : QStringList();
}

bool BackgroundsPage::edit(const std::function<void(QStringList&)>& change)
{
    if (!m_file)
        return false;
    QStringList text = lines();
    change(text);
    return AirEdits::apply(m_file->document(), text);
}

void BackgroundsPage::reparse()
{
    const QStringList text = lines();
    m_doc = BgDocument::parse(text);
    m_air = AirDocument::parse(text);
    if (m_group >= m_doc.groups().size())
        m_group = int(m_doc.groups().size()) - 1;
    if (m_group < 0 && !m_doc.groups().isEmpty())
        m_group = 0;
    SpriteFile* spriteFile = sprites();
    if (spriteFile != m_spriteFile) {
        if (m_spriteFile)
            disconnect(m_spriteFile, nullptr, this, nullptr);
        m_spriteFile = spriteFile;
        if (m_spriteFile)
            connect(m_spriteFile, &SpriteFile::documentChanged, this, [this] {
                if (m_renderer)
                    m_renderer->clearCache();
                refresh();
            });
    }
    m_renderer = std::make_unique<BgRenderer>(m_spriteFile ? &m_spriteFile->document() : nullptr, &m_air);
    clampSelection();
    const BgGroup* g = currentGroup();
    m_simulator = g ? std::make_unique<BgSimulator>(*g) : nullptr;
    refresh();
    Q_EMIT documentChanged();
}

void BackgroundsPage::clampSelection()
{
    const BgGroup* g = currentGroup();
    const int elements = g ? int(g->elements.size()) : 0;
    m_current = elements > 0 ? std::clamp(m_current < 0 ? 0 : m_current, 0, elements - 1) : -1;
    const int definitions = g ? int(g->controllerDefs.size()) : 0;
    m_definition = definitions > 0 ? std::clamp(m_definition < 0 ? 0 : m_definition, 0, definitions - 1) : -1;
    const int controllers = m_definition >= 0 ? int(g->controllerDefs[m_definition].controllers.size()) : 0;
    m_controller = controllers > 0 ? std::clamp(m_controller < 0 ? 0 : m_controller, 0, controllers - 1) : -1;
}

const BgGroup* BackgroundsPage::currentGroup() const
{
    return m_group >= 0 && m_group < m_doc.groups().size() ? &m_doc.groups()[m_group] : nullptr;
}

const BgElement* BackgroundsPage::element() const
{
    const BgGroup* g = currentGroup();
    return g && m_current >= 0 && m_current < g->elements.size() ? &g->elements[m_current] : nullptr;
}

const BgControllerDef* BackgroundsPage::definition() const
{
    const BgGroup* g = currentGroup();
    return g && m_definition >= 0 && m_definition < g->controllerDefs.size() ? &g->controllerDefs[m_definition]
                                                                             : nullptr;
}

const BgController* BackgroundsPage::controller() const
{
    const BgControllerDef* d = definition();
    return d && m_controller >= 0 && m_controller < d->controllers.size() ? &d->controllers[m_controller] : nullptr;
}

void BackgroundsPage::setGroup(int group)
{
    if (group == m_group || group < 0 || group >= m_doc.groups().size())
        return;
    m_group = group;
    m_current = m_definition = m_controller = -1;
    m_hidden.clear();
    reparse();
    Q_EMIT currentChanged();
}

void BackgroundsPage::setCurrent(int element)
{
    const BgGroup* g = currentGroup();
    if (!g || g->elements.isEmpty())
        return;
    element = std::clamp(element, 0, int(g->elements.size()) - 1);
    if (element == m_current)
        return;
    m_current = element;
    refresh();
    Q_EMIT currentChanged();
}

void BackgroundsPage::setCurrentDefinition(int definition)
{
    const BgGroup* g = currentGroup();
    if (!g || g->controllerDefs.isEmpty())
        return;
    definition = std::clamp(definition, 0, int(g->controllerDefs.size()) - 1);
    if (definition == m_definition)
        return;
    m_definition = definition;
    m_controller = -1;
    clampSelection();
    Q_EMIT currentChanged();
}

void BackgroundsPage::setCurrentController(int controller)
{
    const BgControllerDef* d = definition();
    if (!d || d->controllers.isEmpty())
        return;
    controller = std::clamp(controller, 0, int(d->controllers.size()) - 1);
    if (controller == m_controller)
        return;
    m_controller = controller;
    Q_EMIT currentChanged();
}

void BackgroundsPage::setTick(int tick)
{
    tick = std::max(0, tick);
    if (tick == m_tick)
        return;
    m_tick = tick;
    refresh();
    Q_EMIT tickChanged();
}

void BackgroundsPage::play()
{
    if (m_mode == Mode::Playing)
        return;
    if (m_mode == Mode::Stopped && isStage()) {
        m_camera = QPointF(value(QStringLiteral("Camera"), QStringLiteral("startx")).toDouble(),
                           value(QStringLiteral("Camera"), QStringLiteral("starty")).toDouble());
        moveCamera(0, 0);
    }
    m_mode = Mode::Playing;
    m_timer->start();
    refresh();
    Q_EMIT modeChanged();
    m_view->setFocus();
}

void BackgroundsPage::pause()
{
    if (m_mode != Mode::Playing)
        return;
    m_timer->stop();
    m_mode = Mode::Paused;
    refresh();
    Q_EMIT modeChanged();
}

void BackgroundsPage::stop()
{
    m_timer->stop();
    const bool changed = m_mode != Mode::Stopped || m_tick != 0;
    m_mode = Mode::Stopped;
    m_tick = 0;
    m_camera = QPointF();
    refresh();
    if (changed) {
        Q_EMIT modeChanged();
        Q_EMIT tickChanged();
    }
}

void BackgroundsPage::moveCamera(double dx, double dy)
{
    m_camera += QPointF(dx, dy);
    if (isStage()) {
        // Within [boundleft, boundright] x [boundhigh, boundlow].
        const BgSection* camera = m_doc.section(QStringLiteral("Camera"));
        auto bound = [camera](const char* key, double fallback) {
            return camera && camera->value(QLatin1String(key)) ? camera->numbers(QLatin1String(key), 1)[0] : fallback;
        };
        const double inf = std::numeric_limits<double>::infinity();
        m_camera.setX(std::clamp(m_camera.x(), bound("boundleft", -inf), bound("boundright", inf)));
        m_camera.setY(std::clamp(m_camera.y(), bound("boundhigh", -inf), bound("boundlow", inf)));
    }
    refresh();
}

void BackgroundsPage::setHiddenElements(const QSet<int>& hidden)
{
    m_hidden = hidden;
    refresh();
}

bool BackgroundsPage::eventFilter(QObject* watched, QEvent* event)
{
    // While playing (or paused) the arrow keys move the camera: 1 unit, Shift 2, Alt 10.
    if (watched == m_view && event->type() == QEvent::KeyPress && m_mode != Mode::Stopped) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->modifiers() & Qt::ControlModifier)
            return false;
        const double step = (key->modifiers() & Qt::AltModifier) ? 10 : (key->modifiers() & Qt::ShiftModifier) ? 2 : 1;
        switch (key->key()) {
        case Qt::Key_Left:
            moveCamera(-step, 0);
            return true;
        case Qt::Key_Right:
            moveCamera(step, 0);
            return true;
        case Qt::Key_Up:
            moveCamera(0, -step);
            return true;
        case Qt::Key_Down:
            moveCamera(0, step);
            return true;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void BackgroundsPage::applySettings(const Config& config)
{
    m_view->setRulersVisible(config.boolValue(QStringLiteral("show_rulers"), true));
    m_view->setMainCrossVisible(config.boolValue(QStringLiteral("show_cross1"), true) && isStage());
    m_view->setGuidesVisible(config.boolValue(QStringLiteral("show_guides"), false));
    m_view->setImagesOverGuides(config.boolValue(QStringLiteral("sprite_over_guides"), false));
    m_view->applyGraphicsSettings(config);
    if (!m_settingsApplied) {
        // bg_zoom_level counts the levels from 1x.
        m_settingsApplied = true;
        const int one = int(GraphicsView::largeZoomRange().indexOf(1.0));
        m_view->setZoomIndex(one + config.intValue(QStringLiteral("bg_zoom_level"), 2));
        if (config.contains(QStringLiteral("bg_x")))
            m_view->setOrigin(
                QPoint(config.intValue(QStringLiteral("bg_x"), 452), config.intValue(QStringLiteral("bg_y"), 51)));
    }
    m_secondaryCross = config.boolValue(QStringLiteral("show_cross2"), true);
    refresh();
}

void BackgroundsPage::refresh()
{
    const QSizeF screen = screenSize();
    const double halfWidth = screen.width() / 2;
    const QRectF screenRect(-halfWidth, 0, screen.width(), screen.height());
    const bool editing = m_mode == Mode::Stopped;
    QList<GraphicsView::Image> images;
    QList<GraphicsView::Frame> frames;
    m_imageElements.clear();
    frames << GraphicsView::Frame{screenRect, editing ? QColor(255, 0, 0) : QColor(0, 255, 0), false, false};
    QRectF selection;
    if (m_simulator && m_renderer) {
        m_simulator->runTo(m_tick);
        BgView view;
        view.screen = screen;
        view.camera = editing ? QPointF() : m_camera;
        view.windows = !editing;
        const QList<BgDrawItem> items = m_renderer->render(*m_simulator, view);
        for (const BgDrawItem& item : items) {
            if (m_hidden.contains(item.element))
                continue;
            QPointF topLeft = item.topLeft - QPointF(halfWidth, 0);
            if (item.element == m_current)
                topLeft += QPointF(m_dragOffset);
            QRectF clip;
            if (!editing) {
                clip = screenRect;
                if (item.clip)
                    clip &= item.clip->translated(-halfWidth, 0);
                if (clip.isEmpty())
                    continue;
            }
            const QPoint at(int(std::floor(topLeft.x())), int(std::floor(topLeft.y())));
            images << GraphicsView::Image{item.image, at, item.opacity, m_mode != Mode::Playing, {}, clip, item.mode};
            m_imageElements << item.element;
            if (item.element == m_current && selection.isNull())
                selection = QRectF(at, QSizeF(item.image.size()));
        }
    }
    if (const BgElement* e = element(); e && m_mode != Mode::Playing) {
        if (!selection.isNull())
            frames << GraphicsView::Frame{selection, Qt::white, true, true};
        QRectF window = screenRect;
        if (e->hasWindow) {
            const QRect r = e->window.normalized();
            const double left = r.left() - (e->maskWindow ? 0 : halfWidth);
            window = QRectF(left, r.top(), e->maskWindow ? r.width() - 1 : r.width(),
                            e->maskWindow ? r.height() - 1 : r.height());
        }
        frames << GraphicsView::Frame{window, QColor(0, 255, 0), false, false};
    }
    m_view->setImages(images);
    m_view->setFrames(frames);
    m_view->setBackgroundColor(editing ? QColor(255, 0, 255) : QColor(0, 0, 0));
    if (isStage()) {
        const double zoffset = value(QStringLiteral("StageInfo"), QStringLiteral("zoffset")).toDouble();
        auto start = [this, zoffset](const char* x, const char* y, double fallbackX) {
            const BgSection* info = m_doc.section(QStringLiteral("PlayerInfo"));
            const double px = info && info->value(QLatin1String(x)) ? info->numbers(QLatin1String(x), 1)[0] : fallbackX;
            const double py = info && info->value(QLatin1String(y)) ? info->numbers(QLatin1String(y), 1)[0] : 0;
            return QPoint(int(std::lround(px)), int(std::lround(zoffset + py)));
        };
        m_view->setMainCrossPosition(start("p1startx", "p1starty", -70));
        m_view->setSecondaryCross(m_secondaryCross, start("p2startx", "p2starty", 70));
    } else {
        m_view->setSecondaryCross(false, QPoint());
    }
    m_timeline->setPlayhead(m_tick);
    Q_EMIT statusChanged();
}

} // namespace OFS
