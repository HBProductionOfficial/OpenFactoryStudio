#include "shell/AnimationPage.h"

#include "core/code/CodeEditor.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/settings/Config.h"
#include "core/sprites/SpriteFile.h"
#include "core/widgets/TimelineWidget.h"
#include "shell/CodePage.h"

#include <QStackedWidget>
#include <QTextBlock>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

namespace OFS {

namespace {

QRect boxRect(const AirBox& box)
{
    QRect r;
    const AirBox b = box.normalized();
    r.setCoords(b.left, b.top, b.right, b.bottom);
    return r;
}

AirBox fromRect(const QRect& rect)
{
    const QRect r = rect.normalized();
    return AirBox{r.left(), r.top(), r.right(), r.bottom(), 0, -1};
}

QColor colorValue(const Config* config, const QString& key, const QColor& fallback)
{
    if (!config)
        return fallback;
    const QStringList parts = config->value(key).split(QLatin1Char(','));
    if (parts.size() != 3)
        return fallback;
    return QColor(parts[0].trimmed().toInt(), parts[1].trimmed().toInt(), parts[2].trimmed().toInt());
}

} // namespace

AnimationPage::AnimationPage(CodeFile* file, CodePage* code, Project* project, QWidget* parent)
    : QWidget(parent)
    , m_file(file)
    , m_code(code)
    , m_project(project)
    , m_stack(new QStackedWidget(this))
    , m_graphic(new QWidget(m_stack))
    , m_view(new GraphicsView(m_graphic))
    , m_timeline(new TimelineWidget(m_graphic))
    , m_timer(new QTimer(this))
{
    m_view->setObjectName(QStringLiteral("animationView"));
    auto* graphicLayout = new QVBoxLayout(m_graphic);
    graphicLayout->setContentsMargins(0, 0, 0, 0);
    graphicLayout->setSpacing(0);
    graphicLayout->addWidget(m_view, 1);
    graphicLayout->addWidget(m_timeline);
    m_stack->addWidget(m_graphic);
    m_code->setParent(m_stack);
    m_stack->addWidget(m_code);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_stack);
    m_view->setZoomIndex(4); // 3x
    m_timer->setTimerType(Qt::PreciseTimer);
    m_timer->setInterval(1000 / 60);
    connect(m_timer, &QTimer::timeout, this, &AnimationPage::advance);

    connect(m_file->document(), &QTextDocument::contentsChanged, this, &AnimationPage::reparse);
    connect(m_code->editor(), &CodeEditor::cursorPositionChanged, this, &AnimationPage::followCursor);
    if (SpriteFile* spriteFile = sprites()) {
        connect(spriteFile, &SpriteFile::documentChanged, this, [this] {
            m_spriteCache.clear();
            refresh();
        });
    }
    connect(m_timeline, &TimelineWidget::frameClicked, this, [this](int f) {
        if (!isPlaying())
            setCurrentFrame(f);
    });
    connect(m_view, &GraphicsView::cursorMoved, this, [this](const QPoint& units) {
        m_cursor = units;
        Q_EMIT statusChanged();
    });
    connect(m_view, &GraphicsView::guidesChanged, this, &AnimationPage::statusChanged);
    connect(m_view, &GraphicsView::imageDragged, this, [this](int dx, int dy) {
        m_dragOffset += QPoint(dx, dy);
        refresh();
    });
    connect(m_view, &GraphicsView::dragFinished, this, [this] {
        const QPoint offset = std::exchange(m_dragOffset, QPoint());
        const AirFrame* f = frame();
        if (offset.isNull() || !f)
            return;
        AirFrame moved = *f;
        moved.x += offset.x();
        moved.y += offset.y();
        const int a = m_current;
        const int n = m_frame;
        edit([a, n, moved](QStringList& lines) { AirEdits::setFrame(lines, a, n, moved); });
    });
    connect(m_view, &GraphicsView::boxPressed, this, [this](int index) {
        setSelectedBox(index >= 0 && index < m_boxRefs.size() ? m_boxRefs.at(index) : AirEdits::BoxRef{});
    });
    connect(m_view, &GraphicsView::boxEdited, this, [this](int index, const QRect& rect) {
        if (index < 0 || index >= m_boxRefs.size() || !m_boxRefs.at(index).isValid())
            return;
        const AirEdits::BoxRef ref = m_boxRefs.at(index);
        const int a = m_current;
        const int n = m_frame;
        edit([a, n, ref, rect](QStringList& lines) { AirEdits::setBox(lines, a, n, ref, fromRect(rect)); });
    });
    connect(m_view, &GraphicsView::boxDrawn, this, [this](const QRect& rect) {
        if (!m_drawing || !frame())
            return;
        AirEdits::BoxRef added;
        const int a = m_current;
        const int n = m_frame;
        const int type = m_drawing;
        edit([&added, a, n, type, rect](QStringList& lines) {
            added = AirEdits::addBox(lines, a, n, type, fromRect(rect));
        });
        setSelectedBox(added);
    });
    connect(m_view, &GraphicsView::rightClicked, this, [this] { setDrawingBoxes(0); });
    connect(m_view, &GraphicsView::boxNudged, this, [this](int dx, int dy) {
        const AirFrame* f = frame();
        if (!f || !m_selected.isValid() || m_selected.block >= f->blocks.size() ||
            m_selected.box >= f->blocks.at(m_selected.block).boxes.size())
            return;
        AirBox b = f->blocks.at(m_selected.block).boxes.at(m_selected.box).normalized();
        b.left += dx;
        b.right += dx;
        b.top += dy;
        b.bottom += dy;
        const AirEdits::BoxRef ref = m_selected;
        const int a = m_current;
        const int n = m_frame;
        edit([a, n, ref, b](QStringList& lines) { AirEdits::setBox(lines, a, n, ref, b); });
    });
    connect(m_view, &GraphicsView::boxDeleteRequested, this, [this] {
        if (!m_selected.isValid())
            return;
        const AirEdits::BoxRef ref = m_selected;
        const int a = m_current;
        const int n = m_frame;
        setSelectedBox({});
        edit([a, n, ref](QStringList& lines) { AirEdits::deleteBoxes(lines, a, n, {ref}); });
    });
    reparse();
}

AnimationPage::~AnimationPage()
{
    // Child widgets are deleted after this destructor, so cut their signals to the page first.
    m_timer->stop();
    disconnect(m_code->editor(), nullptr, this, nullptr);
    disconnect(m_view, nullptr, this, nullptr);
    disconnect(m_timeline, nullptr, this, nullptr);
    if (m_file)
        disconnect(m_file->document(), nullptr, this, nullptr);
}

SpriteFile* AnimationPage::sprites() const
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

QImage AnimationPage::spriteImage(int index) const
{
    const auto cached = m_spriteCache.constFind(index);
    if (cached != m_spriteCache.cend())
        return *cached;
    QImage image;
    if (SpriteFile* s = sprites()) {
        const Formats::Image rgba = s->document().imageRgba(index);
        if (!rgba.pixelBuffer().empty())
            image = QImage(rgba.pixels().data(), rgba.width(), rgba.height(), rgba.width() * 4, QImage::Format_RGBA8888)
                        .copy();
    }
    m_spriteCache.insert(index, image);
    return image;
}

const AirAnimation* AnimationPage::animation() const
{
    return m_current >= 0 && m_current < m_doc.count() ? &m_doc.animations().at(m_current) : nullptr;
}

const AirFrame* AnimationPage::frame() const
{
    const AirAnimation* a = animation();
    return a && m_frame >= 0 && m_frame < a->frames.size() ? &a->frames.at(m_frame) : nullptr;
}

void AnimationPage::reparse()
{
    m_doc = AirDocument::parse(m_file->document()->toPlainText());
    if (m_doc.count() == 0) {
        m_current = -1;
        m_frame = -1;
    } else {
        m_current = std::clamp(m_current, 0, m_doc.count() - 1);
        const int frames = int(m_doc.animations().at(m_current).frames.size());
        m_frame = frames > 0 ? std::clamp(m_frame, 0, frames - 1) : -1;
    }
    const AirFrame* f = frame();
    if (m_selected.isValid() &&
        (!f || m_selected.block >= f->blocks.size() || m_selected.box >= f->blocks.at(m_selected.block).boxes.size()))
        m_selected = {};
    if (const AirAnimation* a = animation(); a && m_frame >= 0)
        m_frames.insert(a->number, m_frame);
    refresh();
    Q_EMIT documentChanged();
    Q_EMIT currentChanged();
}

void AnimationPage::setCurrent(int index)
{
    if (m_doc.count() == 0)
        return;
    index = std::clamp(index, 0, m_doc.count() - 1);
    if (index == m_current && m_frame >= 0)
        return;
    m_current = index;
    const AirAnimation& a = m_doc.animations().at(index);
    m_frame = a.frames.isEmpty() ? -1 : std::clamp(m_frames.value(a.number, 0), 0, int(a.frames.size()) - 1);
    m_selected = {};
    m_tick = 0;
    refresh();
    Q_EMIT currentChanged();
}

void AnimationPage::setCurrentFrame(int index)
{
    const AirAnimation* a = animation();
    if (!a || a->frames.isEmpty())
        return;
    index = std::clamp(index, 0, int(a->frames.size()) - 1);
    if (index == m_frame)
        return;
    m_frame = index;
    m_frames.insert(a->number, index);
    m_selected = {};
    m_tick = 0;
    refresh();
    Q_EMIT currentChanged();
}

bool AnimationPage::edit(const std::function<void(QStringList&)>& change)
{
    QStringList lines = AirEdits::lines(m_file->document());
    change(lines);
    return AirEdits::apply(m_file->document(), lines);
}

bool AnimationPage::textMode() const
{
    return m_stack->currentWidget() == m_code;
}

void AnimationPage::setTextMode(bool on)
{
    if (on == textMode())
        return;
    stop();
    setDrawingBoxes(0);
    m_selected = {};
    if (on) {
        m_stack->setCurrentWidget(m_code);
        const AirFrame* f = frame();
        const AirAnimation* a = animation();
        const int line = f ? f->line : a ? a->headerLine : 0;
        m_following = true;
        m_code->editor()->goToLine(line + 1);
        m_following = false;
        m_code->editor()->setFocus();
    } else {
        m_stack->setCurrentWidget(m_graphic);
        refresh();
    }
    Q_EMIT textModeChanged(on);
    Q_EMIT statusChanged();
}

void AnimationPage::followCursor()
{
    // In text mode the cursor line picks the current frame, or the animation when it is on a header.
    if (!textMode() || m_following)
        return;
    const int line = m_code->editor()->textCursor().blockNumber();
    int a = -1;
    int f = -1;
    if (!m_doc.locate(line, &a, &f))
        return;
    if (f >= 0) {
        if (a != m_current) {
            m_frames.insert(m_doc.animations().at(a).number, f);
            setCurrent(a);
        }
        setCurrentFrame(f);
    } else if (line == m_doc.animations().at(a).headerLine) {
        setCurrent(a);
    }
    Q_EMIT statusChanged();
}

void AnimationPage::showLine(int line)
{
    if (textMode()) {
        m_code->editor()->goToLine(line);
        return;
    }
    int a = -1;
    int f = -1;
    if (!m_doc.locate(line - 1, &a, &f))
        return;
    setCurrent(a);
    if (f >= 0)
        setCurrentFrame(f);
}

void AnimationPage::setSelectedBox(const AirEdits::BoxRef& box)
{
    if (box == m_selected)
        return;
    m_selected = box;
    refresh();
    Q_EMIT selectionChanged();
}

void AnimationPage::setDrawingBoxes(int type)
{
    if (type == m_drawing)
        return;
    m_drawing = type;
    m_view->setDrawingBoxes(type != 0, type == 1 ? Qt::red : Qt::blue);
    Q_EMIT drawingChanged(type);
}

bool AnimationPage::isPlaying() const
{
    return m_timer->isActive();
}

void AnimationPage::play()
{
    const AirAnimation* a = animation();
    if (isPlaying() || !a || a->frames.isEmpty())
        return;
    setDrawingBoxes(0);
    m_selected = {};
    m_frame = 0;
    m_tick = 0;
    m_timer->start();
    refresh();
    Q_EMIT playingChanged(true);
    Q_EMIT currentChanged();
}

void AnimationPage::stop()
{
    if (!isPlaying())
        return;
    m_timer->stop();
    m_tick = 0;
    if (const AirAnimation* a = animation(); a && m_frame >= 0)
        m_frames.insert(a->number, m_frame);
    refresh();
    Q_EMIT playingChanged(false);
}

void AnimationPage::advance()
{
    const AirAnimation* a = animation();
    if (!a || a->frames.isEmpty()) {
        stop();
        return;
    }
    const AirFrame& f = a->frames.at(std::clamp(m_frame, 0, int(a->frames.size()) - 1));
    if (f.time < 0)
        return; // held forever
    ++m_tick;
    if (m_tick >= f.time) {
        m_tick = 0;
        m_frame = m_frame + 1 < a->frames.size() ? m_frame + 1 : a->loopStartFrame();
        Q_EMIT currentChanged();
    }
    refresh();
}

AnimationPage::Placement AnimationPage::placement() const
{
    const AirFrame* f = frame();
    if (!f)
        return {};
    Placement p{double(f->x + m_dragOffset.x()), double(f->y + m_dragOffset.y()), f->xScale, f->yScale, f->angle};
    const AirAnimation* a = animation();
    if (isPlaying() && a && m_frame + 1 < a->frames.size() && f->time > 0) {
        const AirFrame& next = a->frames.at(m_frame + 1);
        const double t = double(m_tick) / f->time;
        if (next.interpolate & AirFrame::InterpolateOffset) {
            p.x += (next.x - f->x) * t;
            p.y += (next.y - f->y) * t;
        }
        if (next.interpolate & AirFrame::InterpolateScale) {
            p.xScale += (next.xScale - f->xScale) * t;
            p.yScale += (next.yScale - f->yScale) * t;
        }
        if (next.interpolate & AirFrame::InterpolateAngle)
            p.angle += (next.angle - f->angle) * t;
    }
    return p;
}

bool AnimationPage::spriteLayer(const AirFrame& f, const Placement& placement, double opacity, bool draggable,
                                GraphicsView::Image* out) const
{
    SpriteFile* s = sprites();
    if (!s || f.group < 0)
        return false;
    const int index = s->document().findSprite(f.group, f.number);
    if (index < 0)
        return false;
    const auto& header = s->document().sprites().at(index).header;
    // Flip, scale and rotate (counterclockwise) around the sprite axis at the frame offset.
    QTransform transform;
    transform.translate(placement.x, placement.y);
    transform.rotate(-placement.angle);
    transform.scale(placement.xScale * (f.flipH ? -1 : 1), placement.yScale * (f.flipV ? -1 : 1));
    *out = GraphicsView::Image{
        spriteImage(index), QPoint(-header.axisX, -header.axisY), opacity, draggable, transform, {}, {}};
    return true;
}

void AnimationPage::refresh()
{
    const AirAnimation* a = animation();
    const AirFrame* f = frame();
    QList<GraphicsView::Image> images;
    GraphicsView::Image layer;
    if (m_onion && m_onionAnimation >= 0 && m_onionAnimation < m_doc.count()) {
        const AirAnimation& onion = m_doc.animations().at(m_onionAnimation);
        if (!onion.frames.isEmpty()) {
            const AirFrame& of = onion.frames.at(std::clamp(m_onionFrame, 0, int(onion.frames.size()) - 1));
            if (spriteLayer(of, Placement{double(of.x), double(of.y), of.xScale, of.yScale, of.angle}, m_onionOpacity,
                            false, &layer))
                images << layer;
        }
    }
    if (f && spriteLayer(*f, placement(), 1.0, !isPlaying(), &layer))
        images << layer;
    m_view->setImages(images);

    // The frame's own boxes are editable. Without them it shows the default boxes it inherits.
    QList<GraphicsView::Box> boxes;
    m_boxRefs.clear();
    if (f && a) {
        for (int type : {1, 2}) {
            if ((type == 1 && !m_showAttack) || (type == 2 && !m_showCollision))
                continue;
            const QColor regular = type == 1 ? QColor(255, 0, 0) : QColor(0, 0, 255);
            const QColor fallback = type == 1 ? m_defaultAttack : m_defaultCollision;
            if (f->hasBoxes(type)) {
                for (int b = 0; b < f->blocks.size(); ++b) {
                    const AirBoxBlock& block = f->blocks.at(b);
                    if (block.type != type)
                        continue;
                    for (int i = 0; i < block.boxes.size(); ++i) {
                        const AirEdits::BoxRef ref{b, i};
                        boxes << GraphicsView::Box{boxRect(block.boxes.at(i)), block.isDefault ? fallback : regular,
                                                   ref == m_selected, !isPlaying()};
                        m_boxRefs << ref;
                    }
                }
            } else {
                for (const AirBox& box : a->boxes(m_frame, type)) {
                    boxes << GraphicsView::Box{boxRect(box), fallback, false, false};
                    m_boxRefs << AirEdits::BoxRef{};
                }
            }
        }
    }
    m_view->setBoxes(boxes);
    m_view->setSecondaryCross(m_secondaryCross && f,
                              f ? QPoint(qRound(placement().x), qRound(placement().y)) : QPoint());

    QList<TimelineWidget::Frame> blocks;
    int start = 0;
    if (a) {
        for (int i = 0; i < a->frames.size(); ++i) {
            blocks << TimelineWidget::Frame{a->frames.at(i).time, a->frames.at(i).loopStart};
            if (i < m_frame)
                start += std::max(0, a->frames.at(i).time);
        }
    }
    m_timeline->setAnimation(a ? a->caption() : QString(), blocks);
    m_timeline->setPlayhead(start + (isPlaying() ? m_tick : 0));
}

void AnimationPage::setOnionSkin(bool on, int animation, int frame, double opacity)
{
    m_onion = on;
    m_onionAnimation = animation;
    m_onionFrame = frame;
    m_onionOpacity = opacity;
    refresh();
}

void AnimationPage::applySettings(const Config& config, const Config* engine)
{
    m_view->setZoomLevels(config.intValue(QStringLiteral("zoom_mode"), 1) == 0 ? GraphicsView::smallZoomRange()
                                                                               : GraphicsView::largeZoomRange());
    m_view->setZoomCenter(config.intValue(QStringLiteral("zoom_center"), 0) == 1 ? GraphicsView::ZoomCenter::Workspace
                                                                                 : GraphicsView::ZoomCenter::Axis);
    const QPoint offset(config.intValue(QStringLiteral("background_x"), -426),
                        config.intValue(QStringLiteral("background_y"), -416));
    switch (config.intValue(QStringLiteral("background_type"), 1)) {
    case 0:
        m_view->setBackground(GraphicsView::Background::Color);
        break;
    case 2:
        m_view->setBackgroundPicture(QImage(config.value(QStringLiteral("background_file"))), offset);
        m_view->setBackground(GraphicsView::Background::Picture);
        break;
    default:
        m_view->setBackgroundPicture(GraphicsView::defaultBackground(), offset);
        m_view->setBackground(GraphicsView::Background::Picture);
        break;
    }
    m_view->setRulersVisible(config.boolValue(QStringLiteral("show_rulers"), true));
    m_view->setMainCrossVisible(config.boolValue(QStringLiteral("show_cross1"), true));
    m_view->setBoxesOverCrosses(config.boolValue(QStringLiteral("contact_over_cross"), false));
    m_view->setExtraCrossVisible(config.boolValue(QStringLiteral("show_cross3"), false));
    m_view->setGuidesVisible(config.boolValue(QStringLiteral("show_guides"), false));
    m_view->setImagesOverGuides(config.boolValue(QStringLiteral("sprite_over_guides"), false));
    m_secondaryCross = config.boolValue(QStringLiteral("show_cross2"), true);
    m_showAttack = config.boolValue(QStringLiteral("show_attack_box"), true);
    m_showCollision = config.boolValue(QStringLiteral("show_collision_box"), true);
    m_defaultAttack = colorValue(engine, QStringLiteral("clsn_default_attack"), QColor(255, 128, 0));
    m_defaultCollision = colorValue(engine, QStringLiteral("clsn_default_collision"), QColor(0, 255, 255));
    m_onionOpacity = config.value(QStringLiteral("onion_opacity"), QStringLiteral("0.74")).toDouble();
    if (!m_settingsApplied) {
        m_settingsApplied = true;
        m_view->setZoomIndex(4); // 3x
    }
    refresh();
}

} // namespace OFS
