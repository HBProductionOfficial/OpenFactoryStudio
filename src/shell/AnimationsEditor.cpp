#include "shell/AnimationsEditor.h"

#include "core/actions/ActionRegistry.h"
#include "core/anims/AirEdits.h"
#include "core/anims/AirGif.h"
#include "core/app/AppContext.h"
#include "core/app/Messages.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/settings/Config.h"
#include "core/sprites/SpriteExport.h"
#include "core/sprites/SpriteFile.h"
#include "shell/AnimationPage.h"
#include "shell/FileDialogs.h"
#include "shell/dialogs/AnimationDialogs.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QScreen>
#include <QScrollBar>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTextDocument>
#include <QToolButton>

namespace OFS {

namespace {

template <typename T>
T* control(QWidget* panel, const char* name)
{
    return panel ? panel->findChild<T*>(QLatin1String(name)) : nullptr;
}

struct CopiedBox
{
    int type = 0;
    AirBox box;
};
CopiedBox& copiedBox()
{
    static CopiedBox box;
    return box;
}

const int InterpolationFlags[4] = {AirFrame::InterpolateBlend, AirFrame::InterpolateOffset, AirFrame::InterpolateScale,
                                   AirFrame::InterpolateAngle};

} // namespace

AnimationsEditor::AnimationsEditor(AppContext& context, QObject* parent)
    : QObject(parent)
    , m_context(context)
{}

AnimationsEditor::~AnimationsEditor()
{
    delete m_codeWindow;
}

void AnimationsEditor::bindPanel(QWidget* panel)
{
    m_selector = control<QScrollBar>(panel, "anim_sel");
    m_selectorLabel = control<QLabel>(panel, "anim_sel_lb");
    m_number = control<QSpinBox>(panel, "anim_number");
    m_name = control<QLineEdit>(panel, "anim_name");
    m_frameSelector = control<QScrollBar>(panel, "anim_frame_sel");
    m_frameLabel = control<QLabel>(panel, "anim_frame_sel_lb");
    m_group = control<QSpinBox>(panel, "anim_frame_grp");
    m_index = control<QSpinBox>(panel, "anim_frame_num");
    m_x = control<QSpinBox>(panel, "anim_frame_xaxis");
    m_y = control<QSpinBox>(panel, "anim_frame_yaxis");
    m_time = control<QSpinBox>(panel, "anim_frame_time");
    m_flip = control<QComboBox>(panel, "anim_frame_flip");
    m_transparency = control<QComboBox>(panel, "anim_frame_trans");
    m_source = control<QSpinBox>(panel, "anim_frame_transsrc");
    m_dest = control<QSpinBox>(panel, "anim_frame_transdst");
    m_loop = control<QCheckBox>(panel, "anim_frame_loop");
    m_xScale = control<QDoubleSpinBox>(panel, "anim_frame_xscale");
    m_yScale = control<QDoubleSpinBox>(panel, "anim_frame_yscale");
    m_angle = control<QDoubleSpinBox>(panel, "anim_frame_angle");
    const char* const interpolation[] = {"anim_frame_intp_blend", "anim_frame_intp_offset", "anim_frame_intp_scale",
                                         "anim_frame_intp_angle"};
    for (int i = 0; i < 4; ++i)
        m_interpolate[i] = control<QCheckBox>(panel, interpolation[i]);
    m_onion = control<QCheckBox>(panel, "anim_onion_skin");
    m_onionSelector = control<QScrollBar>(panel, "anim_onion_sel");
    m_onionFrameSelector = control<QScrollBar>(panel, "anim_onion_framesel");
    m_onionLabel = control<QLabel>(panel, "anim_onion_lb");

    if (m_selector)
        connect(m_selector, &QScrollBar::valueChanged, this, [this](int value) {
            if (!m_updating && m_page && !m_page->isPlaying())
                m_page->setCurrent(value);
        });
    if (m_frameSelector)
        connect(m_frameSelector, &QScrollBar::valueChanged, this, [this](int value) {
            if (!m_updating && m_page && !m_page->isPlaying())
                m_page->setCurrentFrame(value);
        });
    if (m_number)
        connect(m_number, &QSpinBox::valueChanged, this, [this](int value) {
            if (m_updating || !m_page || !m_page->animation() || m_page->animation()->number == value)
                return;
            const int a = m_page->current();
            m_page->edit([a, value](QStringList& lines) { AirEdits::setNumber(lines, a, value); });
        });
    if (m_name)
        connect(m_name, &QLineEdit::textEdited, this, [this](const QString& text) {
            if (m_updating || !m_page || !m_page->animation())
                return;
            const int a = m_page->current();
            m_page->edit([a, text](QStringList& lines) { AirEdits::setName(lines, a, text); });
        });
    auto spin = [this](QSpinBox* box, void (*set)(AirFrame&, int)) {
        if (!box)
            return;
        box->setKeyboardTracking(false);
        connect(box, &QSpinBox::valueChanged, this, [this, set](int value) {
            if (!m_updating)
                applyFrame([set, value](AirFrame& f) { set(f, value); });
        });
    };
    spin(m_group, [](AirFrame& f, int v) { f.group = v; });
    spin(m_index, [](AirFrame& f, int v) { f.number = v; });
    spin(m_x, [](AirFrame& f, int v) { f.x = v; });
    spin(m_y, [](AirFrame& f, int v) { f.y = v; });
    spin(m_time, [](AirFrame& f, int v) { f.time = v; });
    if (m_flip) {
        m_flip->clear();
        m_flip->addItems({tr("None"), tr("Horizontally"), tr("Vertically"), tr("Both")});
        connect(m_flip, &QComboBox::currentIndexChanged, this, [this](int index) {
            if (!m_updating && index >= 0)
                applyFrame([index](AirFrame& f) {
                    f.flipH = index == 1 || index == 3;
                    f.flipV = index == 2 || index == 3;
                });
        });
    }
    if (m_transparency) {
        m_transparency->clear();
        m_transparency->addItems({tr("None"), tr("Addition"), tr("Addition 50%"), tr("Subtraction")});
        connect(m_transparency, &QComboBox::currentIndexChanged, this, [this](int index) {
            if (m_updating || index < 0)
                return;
            // Addition 50% sets the destination to 128.
            if (index == 2 && m_dest) {
                const bool updating = std::exchange(m_updating, true);
                m_dest->setValue(128);
                m_updating = updating;
            }
            const int source = m_source ? m_source->value() : 256;
            const int dest = m_dest ? m_dest->value() : 256;
            applyFrame([index, source, dest](AirFrame& f) {
                f.transparency.mode = AirTransparency::Mode(index);
                f.transparency.source = source;
                f.transparency.dest = dest;
            });
        });
    }
    for (QSpinBox* box : {m_source.data(), m_dest.data()}) {
        if (!box)
            continue;
        box->setRange(0, 256);
        box->setValue(256);
        box->setKeyboardTracking(false);
        connect(box, &QSpinBox::valueChanged, this, [this] {
            // Only written to the file for Addition.
            if (m_updating || !m_page || !m_page->frame() ||
                m_page->frame()->transparency.mode == AirTransparency::Mode::None ||
                m_page->frame()->transparency.mode == AirTransparency::Mode::Subtraction)
                return;
            const int source = m_source ? m_source->value() : 256;
            const int dest = m_dest ? m_dest->value() : 256;
            applyFrame([source, dest](AirFrame& f) {
                f.transparency.source = source;
                f.transparency.dest = dest;
            });
        });
    }
    if (m_loop)
        connect(m_loop, &QCheckBox::toggled, this, [this](bool on) {
            if (m_updating || !m_page || !m_page->frame())
                return;
            const int a = m_page->current();
            const int f = m_page->currentFrame();
            m_page->edit([a, f, on](QStringList& lines) { AirEdits::setLoopStart(lines, a, f, on); });
        });
    for (QDoubleSpinBox* box : {m_xScale.data(), m_yScale.data(), m_angle.data()}) {
        if (!box)
            continue;
        box->setDecimals(4);
        box->setKeyboardTracking(false);
        box->setRange(0, box == m_angle ? 360 : 100);
        connect(box, &QDoubleSpinBox::valueChanged, this, [this, box](double value) {
            if (m_updating)
                return;
            applyFrame([this, box, value](AirFrame& f) {
                if (box == m_xScale)
                    f.xScale = value;
                else if (box == m_yScale)
                    f.yScale = value;
                else
                    f.angle = value;
            });
        });
    }
    for (int i = 0; i < 4; ++i) {
        if (!m_interpolate[i])
            continue;
        const int flag = InterpolationFlags[i];
        connect(m_interpolate[i], &QCheckBox::toggled, this, [this, flag](bool on) {
            if (m_updating || !m_page || !m_page->frame())
                return;
            const int a = m_page->current();
            const int f = m_page->currentFrame();
            m_page->edit([a, f, flag, on](QStringList& lines) { AirEdits::setInterpolation(lines, a, f, flag, on); });
        });
    }
    if (m_onion)
        connect(m_onion, &QCheckBox::toggled, this, [this] {
            // Starts at the first animation, same as Fighter Factory.
            m_onionUsed = true;
            refreshOnion();
        });
    for (QScrollBar* bar : {m_onionSelector.data(), m_onionFrameSelector.data()}) {
        if (bar)
            connect(bar, &QScrollBar::valueChanged, this, [this] {
                if (!m_updating)
                    refreshOnion();
            });
    }
    refresh();
}

void AnimationsEditor::setPage(AnimationPage* page)
{
    if (m_page == page)
        return;
    if (m_page)
        disconnect(m_page, nullptr, this, nullptr);
    m_page = page;
    if (m_page) {
        connect(m_page, &AnimationPage::currentChanged, this, &AnimationsEditor::refresh);
        for (auto signal : {&AnimationPage::textModeChanged, &AnimationPage::playingChanged})
            connect(m_page, signal, this, [this] {
                refresh();
                Q_EMIT stateChanged();
            });
        connect(m_page, &AnimationPage::drawingChanged, this, &AnimationsEditor::stateChanged);
        connect(m_page, &AnimationPage::selectionChanged, this, &AnimationsEditor::stateChanged);
    }
    refresh();
    refreshOnion();
    Q_EMIT stateChanged();
}

void AnimationsEditor::applyFrame(const std::function<void(AirFrame&)>& change)
{
    if (!m_page || !m_page->frame())
        return;
    AirFrame frame = *m_page->frame();
    change(frame);
    if (frame.text() == m_page->frame()->text())
        return;
    const int a = m_page->current();
    const int f = m_page->currentFrame();
    m_page->edit([a, f, frame](QStringList& lines) { AirEdits::setFrame(lines, a, f, frame); });
}

void AnimationsEditor::refresh()
{
    const bool updating = std::exchange(m_updating, true);
    const AirDocument* doc = m_page ? &m_page->document() : nullptr;
    const AirAnimation* animation = m_page ? m_page->animation() : nullptr;
    const AirFrame* frame = m_page ? m_page->frame() : nullptr;
    const int count = doc ? doc->count() : 0;
    const bool playing = m_page && m_page->isPlaying();
    if (m_selector) {
        m_selector->setRange(0, std::max(0, count - 1));
        m_selector->setValue(std::max(0, m_page ? m_page->current() : 0));
    }
    if (m_selectorLabel)
        m_selectorLabel->setText(count > 0 ? QStringLiteral("%1 / %2").arg(m_page->current() + 1).arg(count)
                                           : QString());
    if (m_number) {
        m_number->setEnabled(animation);
        m_number->setValue(animation ? animation->number : 0);
    }
    if (m_name && (!animation || m_name->text() != animation->name)) {
        m_name->setEnabled(animation);
        m_name->setText(animation ? animation->name : QString());
    }
    const int frames = animation ? int(animation->frames.size()) : 0;
    if (m_frameSelector) {
        m_frameSelector->setRange(0, std::max(0, frames - 1));
        m_frameSelector->setValue(std::max(0, m_page ? m_page->currentFrame() : 0));
    }
    if (m_frameLabel) {
        // "6 / 11 - 83 ticks" shows the end tick of the frame (-1 for an endless one).
        m_frameLabel->setText(frame ? tr("%1 / %2 - %3 ticks")
                                          .arg(m_page->currentFrame() + 1)
                                          .arg(frames)
                                          .arg(animation->endTick(m_page->currentFrame()))
                                    : QString());
    }
    const bool editable = frame && !playing;
    for (QWidget* w :
         std::initializer_list<QWidget*>{m_group, m_index, m_x, m_y, m_time, m_flip, m_transparency, m_loop}) {
        if (w)
            w->setEnabled(editable);
    }
    for (QWidget* w : std::initializer_list<QWidget*>{m_source, m_dest, m_xScale, m_yScale, m_angle}) {
        if (w)
            w->setEnabled(frame);
    }
    if (frame) {
        if (m_group)
            m_group->setValue(frame->group);
        if (m_index)
            m_index->setValue(frame->number);
        if (m_x)
            m_x->setValue(frame->x);
        if (m_y)
            m_y->setValue(frame->y);
        if (m_time)
            m_time->setValue(frame->time);
        if (m_flip)
            m_flip->setCurrentIndex(frame->flipH && frame->flipV ? 3 : frame->flipH ? 1 : frame->flipV ? 2 : 0);
        if (m_transparency)
            m_transparency->setCurrentIndex(int(frame->transparency.mode));
        if (frame->transparency.mode != AirTransparency::Mode::None) {
            if (m_source)
                m_source->setValue(frame->transparency.source);
            if (m_dest)
                m_dest->setValue(frame->transparency.dest);
        }
        if (m_loop)
            m_loop->setChecked(frame->loopStart);
        if (m_xScale)
            m_xScale->setValue(frame->xScale);
        if (m_yScale)
            m_yScale->setValue(frame->yScale);
        if (m_angle)
            m_angle->setValue(frame->angle);
        for (int i = 0; i < 4; ++i) {
            if (m_interpolate[i]) {
                m_interpolate[i]->setEnabled(true);
                m_interpolate[i]->setChecked(frame->interpolate & InterpolationFlags[i]);
            }
        }
    } else {
        for (QCheckBox* box : m_interpolate) {
            if (box)
                box->setEnabled(false);
        }
    }
    if (m_onion)
        m_onion->setEnabled(m_page && !m_page->textMode());
    if (m_onionSelector)
        m_onionSelector->setRange(0, std::max(0, count - 1));
    m_updating = updating;
    refreshOnion();
}

void AnimationsEditor::refreshOnion()
{
    if (!m_page)
        return;
    const AirDocument& doc = m_page->document();
    const bool updating = std::exchange(m_updating, true);
    const int animation = m_onionSelector ? m_onionSelector->value() : 0;
    const AirAnimation* onion = animation >= 0 && animation < doc.count() ? &doc.animations().at(animation) : nullptr;
    if (m_onionFrameSelector && m_onionUsed)
        m_onionFrameSelector->setRange(0, onion ? std::max(0, int(onion->frames.size()) - 1) : 0);
    const int frame = m_onionFrameSelector ? m_onionFrameSelector->value() : 0;
    m_updating = updating;
    const bool on = m_onion && m_onion->isChecked();
    if (m_onionLabel && m_onionUsed) {
        const AirFrame* f = onion && frame < onion->frames.size() ? &onion->frames.at(frame) : nullptr;
        m_onionLabel->setText(onion && f
                                  ? tr("Animation: %1, Frame: %2,%3").arg(onion->number).arg(f->group).arg(f->number)
                                  : tr("Animation: none, Frame: none"));
    }
    m_page->setOnionSkin(on, animation, frame,
                         m_context.config().value(QStringLiteral("onion_opacity"), QStringLiteral("0.74")).toDouble());
}

bool AnimationsEditor::perform(const QString& id)
{
    if (!m_page)
        return false;
    AnimationPage* page = m_page;
    const AirDocument& doc = page->document();
    const int a = page->current();
    const int f = page->currentFrame();
    const AirAnimation* animation = page->animation();
    if (id == QLatin1String("anim_prev_anim")) {
        page->setCurrent(a - 1);
    } else if (id == QLatin1String("anim_next_anim")) {
        page->setCurrent(std::min(a + 1, doc.count() - 1));
    } else if (id == QLatin1String("anim_prev_frame")) {
        page->setCurrentFrame(f - 1);
    } else if (id == QLatin1String("anim_next_frame")) {
        page->setCurrentFrame(f + 1);
    } else if (id.startsWith(QLatin1String("anim_pos_"))) {
        const QString direction = id.mid(9);
        const int dx = direction == QLatin1String("left") ? -1 : direction == QLatin1String("right") ? 1 : 0;
        const int dy = direction == QLatin1String("up") ? -1 : direction == QLatin1String("down") ? 1 : 0;
        applyFrame([dx, dy](AirFrame& frame) {
            frame.x += dx;
            frame.y += dy;
        });
    } else if (id == QLatin1String("anim_play")) {
        page->play();
    } else if (id == QLatin1String("anim_stop")) {
        page->stop();
    } else if (id == QLatin1String("anim_editcode")) {
        page->setTextMode(!page->textMode());
    } else if (id == QLatin1String("anim_frame_add")) {
        if (!animation)
            return true;
        int added = -1;
        page->edit([a, f, &added](QStringList& lines) {
            added = AirEdits::insertFrames(lines, a, f, {AirEdits::blankFrame()});
        });
        page->setCurrentFrame(added);
    } else if (id == QLatin1String("anim_frame_delete")) {
        if (!page->frame())
            return true;
        if (m_context.config().boolValue(QStringLiteral("confirm_delete"), false)) {
            QMessageBox box(QMessageBox::Question, tr("Delete frame"), tr("Do you really want to delete the frame?"),
                            QMessageBox::Yes | QMessageBox::No, page->window());
            box.setDefaultButton(QMessageBox::Yes);
            auto* dontAsk = new QCheckBox(tr("Don't show this again"), &box);
            box.setCheckBox(dontAsk);
            const bool yes = box.exec() == QMessageBox::Yes;
            if (dontAsk->isChecked())
                m_context.config().setValue(QStringLiteral("confirm_delete"), false);
            if (!yes)
                return true;
        }
        page->edit([a, f](QStringList& lines) { AirEdits::deleteFrame(lines, a, f); });
    } else if (id.startsWith(QLatin1String("anim_frameall_"))) {
        const QString field = id.mid(14);
        const AirEdits::Field which = field == QLatin1String("offset")  ? AirEdits::Field::Offset
                                      : field == QLatin1String("time")  ? AirEdits::Field::Time
                                      : field == QLatin1String("flip")  ? AirEdits::Field::Flip
                                      : field == QLatin1String("trans") ? AirEdits::Field::Transparency
                                                                        : AirEdits::Field::Scale;
        if (page->frame())
            page->edit([a, f, which](QStringList& lines) { AirEdits::applyToNext(lines, a, f, which); });
    } else if (!performBoxes(id) && !performDialog(id)) {
        return false;
    }
    return true;
}

bool AnimationsEditor::performBoxes(const QString& id)
{
    AnimationPage* page = m_page;
    const int a = page->current();
    const int f = page->currentFrame();
    const AirEdits::BoxRef selected = page->selectedBox();
    if (id == QLatin1String("anim_box_add_attack") || id == QLatin1String("anim_box_add_collision")) {
        const int type = id == QLatin1String("anim_box_add_attack") ? 1 : 2;
        page->setDrawingBoxes(page->drawingBoxes() == type || page->textMode() ? 0 : type);
    } else if (id == QLatin1String("anim_box_del")) {
        if (!selected.isValid())
            return true;
        page->setSelectedBox({});
        page->edit([a, f, selected](QStringList& lines) { AirEdits::deleteBoxes(lines, a, f, {selected}); });
    } else if (id == QLatin1String("anim_box_delall")) {
        page->setSelectedBox({});
        page->edit([a, f](QStringList& lines) { AirEdits::deleteFrameBoxes(lines, a, f); });
    } else if (id == QLatin1String("anim_box_delall_anim")) {
        page->setSelectedBox({});
        page->edit([a](QStringList& lines) { AirEdits::deleteAnimationBoxes(lines, a); });
    } else if (id == QLatin1String("anim_box_type")) {
        if (!selected.isValid())
            return true;
        AirEdits::BoxRef moved;
        page->edit(
            [a, f, selected, &moved](QStringList& lines) { moved = AirEdits::switchBoxType(lines, a, f, selected); });
        page->setSelectedBox(moved);
    } else if (id == QLatin1String("anim_box_default")) {
        if (selected.isValid())
            page->edit([a, f, selected](QStringList& lines) { AirEdits::switchBoxDefault(lines, a, f, selected); });
    } else if (id == QLatin1String("anim_box_prev")) {
        page->setSelectedBox({});
        page->edit([a, f](QStringList& lines) { AirEdits::copyPreviousBoxes(lines, a, f); });
    } else if (id == QLatin1String("anim_box_allinst")) {
        if (!page->frame())
            return true;
        if (!askQuestion(page->window(), m_context.config(), QStringLiteral("showmsg_cbox_replicate"),
                         tr("Confirmation"),
                         tr("Do you really want to replicate Contact boxes of current frame in all its instances?\n"
                            "All existing Contact boxes in these frames will be replaced.")))
            return true;
        page->edit([a, f](QStringList& lines) { AirEdits::replicateBoxes(lines, a, f); });
    } else {
        return false;
    }
    return true;
}

bool AnimationsEditor::performDialog(const QString& id)
{
    AnimationPage* page = m_page;
    const AirDocument& doc = page->document();
    const int a = page->current();
    QWidget* parent = page->window();
    if (id == QLatin1String("anim_add")) {
        AddAnimationDialog dialog(parent);
        if (dialog.exec() != QDialog::Accepted)
            return true;
        const int number = dialog.number();
        const QString name = dialog.name();
        const int time = dialog.time();
        int added = -1;
        page->edit([a, number, name, time, &added](QStringList& lines) {
            added = AirEdits::addAnimation(lines, a, number, name, {AirEdits::blankFrame(time)});
        });
        page->setCurrent(added);
    } else if (id == QLatin1String("anim_delete")) {
        if (!page->animation())
            return true;
        if (m_context.config().boolValue(QStringLiteral("confirm_delete"), false)) {
            QMessageBox box(QMessageBox::Question, tr("Delete"), tr("Do you really want to delete the animation?"),
                            QMessageBox::Yes | QMessageBox::No, parent);
            box.setDefaultButton(QMessageBox::Yes);
            auto* dontAsk = new QCheckBox(tr("Don't show this again"), &box);
            box.setCheckBox(dontAsk);
            const bool yes = box.exec() == QMessageBox::Yes;
            if (dontAsk->isChecked())
                m_context.config().setValue(QStringLiteral("confirm_delete"), false);
            if (!yes)
                return true;
        }
        page->edit([a](QStringList& lines) { AirEdits::deleteAnimation(lines, a); });
    } else if (id == QLatin1String("anim_duplicate")) {
        const AirAnimation* animation = page->animation();
        if (!animation)
            return true;
        int number = animation->number;
        if (askQuestion(parent, m_context.config(), QStringLiteral("showmsg_anim_clone"), tr("Duplicate"),
                        tr("Do you want to define a new identifier?"))) {
            for (;;) {
                bool ok = false;
                const QString text = QInputDialog::getText(parent, tr("Duplicate animation"), tr("Identifier:"),
                                                           QLineEdit::Normal, QString(), &ok);
                if (!ok)
                    return true;
                bool isNumber = false;
                number = text.trimmed().toInt(&isNumber);
                if (isNumber)
                    break;
                QMessageBox::warning(parent, tr("Duplicate animation"), tr("The identifier must be a number."));
            }
        }
        int added = -1;
        page->edit([a, number, &added](QStringList& lines) { added = AirEdits::duplicateAnimation(lines, a, number); });
        page->setCurrent(added);
    } else if (id == QLatin1String("anim_code")) {
        showCodeWindow();
    } else if (id == QLatin1String("anim_frame_align")) {
        AnimationAlignDialog dialog(doc, a, parent);
        connect(&dialog, &AnimationAlignDialog::applyRequested, this,
                [page](const QList<int>& animations, AirEdits::AlignOperation operation, int x, int y) {
                    if (animations.isEmpty() || (x == 0 && y == 0 && operation != AirEdits::AlignOperation::ScaleBoxes))
                        return;
                    page->edit([animations, operation, x, y](QStringList& lines) {
                        AirEdits::align(lines, animations, operation, x, y);
                    });
                });
        dialog.exec();
    } else if (id == QLatin1String("anim_optimize")) {
        int removed = 0;
        page->edit([&removed](QStringList& lines) { removed = AirEdits::optimize(lines); });
        QMessageBox::information(parent, tr("Optimize done!"),
                                 tr("Code clean, and %1 line(s) were removed in the optimization!").arg(removed));
    } else if (id == QLatin1String("anim_onion_find")) {
        bool ok = false;
        const QString text =
            QInputDialog::getText(parent, tr("Find animation"), QString(), QLineEdit::Normal, QString(), &ok);
        if (!ok)
            return true;
        bool isNumber = false;
        const int number = text.trimmed().toInt(&isNumber);
        const int found = isNumber ? doc.indexOf(number) : -1;
        if (found < 0) {
            QMessageBox::information(parent, tr("Find animation"), tr("No animation found!"));
            return true;
        }
        if (m_onionSelector)
            m_onionSelector->setValue(found);
        refreshOnion();
    } else if (id == QLatin1String("anim_save_gif")) {
        saveGif();
    } else if (id == QLatin1String("anim_import_proj")) {
        QMessageBox::warning(parent, tr("Warning"), tr("You can't import from the destination project!"));
    } else if (id == QLatin1String("thumb_save")) {
        const QList<int> sprites = explorerSprites(false);
        auto* project = qobject_cast<Project*>(page->file()->parent());
        const QStringList files = project ? project->editorFiles(QStringLiteral("sprites")) : QStringList();
        auto* spriteFile =
            project && !files.isEmpty() ? project->spriteFile(files.first(), QStringLiteral("sprites")) : nullptr;
        if (sprites.isEmpty() || !spriteFile)
            return true;
        const QString path = FileDialogs::getSaveFileName(
            parent, tr("Save sprites"), m_context.config().value(QStringLiteral("spr_path")),
            tr("Portable Network Graphic (*.png);;Graphics Interchange Format (*.gif);;Personal Computer eXchange "
               "(*.pcx);;Bitmap (*.bmp)"));
        if (path.isEmpty())
            return true;
        const QFileInfo info(path);
        const QString suffix = info.suffix().isEmpty() ? QStringLiteral("png") : info.suffix();
        const QString base = info.suffix().isEmpty() ? info.fileName() : info.completeBaseName();
        for (int index : sprites) {
            QString error;
            const QString out = info.dir().filePath(
                QStringLiteral("%1_%2.%3").arg(base).arg(spriteFile->document().sprites()[index].number()).arg(suffix));
            if (!writeImageFile(out, spriteFile->document().image(index), &error)) {
                QMessageBox::warning(parent, tr("Save sprites"), error);
                break;
            }
        }
    } else {
        return false;
    }
    return true;
}

void AnimationsEditor::showCodeWindow()
{
    AnimationPage* page = m_page;
    if (!page->animation())
        return;
    if (m_codeWindow) {
        m_codeWindow->raise();
        m_codeWindow->activateWindow();
        return;
    }
    const int a = page->current();
    const QStringList text = AirEdits::animationText(AirEdits::lines(page->file()->document()), a);
    m_codeWindow = new AnimationCodeWindow(text, page->window());
    m_codeWindow->setAttribute(Qt::WA_DeleteOnClose);
    Config& config = m_context.config();
    const QByteArray geometry =
        QByteArray::fromBase64(config.value(QStringLiteral("anim_editcode_geometry")).toLatin1());
    if (!geometry.isEmpty())
        m_codeWindow->restoreGeometry(geometry);
    if (QScreen* screen = page->window()->screen()) {
        const QRect available = screen->availableGeometry();
        QRect frame = m_codeWindow->frameGeometry();
        if (!available.contains(frame)) {
            frame.setSize(frame.size().boundedTo(available.size()));
            frame.moveLeft(std::clamp(frame.left(), available.left(), available.right() - frame.width()));
            frame.moveTop(std::clamp(frame.top(), available.top(), available.bottom() - frame.height()));
            m_codeWindow->move(frame.topLeft());
        }
    }
    connect(m_codeWindow, &QDialog::finished, this, [this, &config] {
        if (m_codeWindow)
            config.setValue(QStringLiteral("anim_editcode_geometry"),
                            QString::fromLatin1(m_codeWindow->saveGeometry().toBase64()));
    });
    connect(m_codeWindow, &AnimationCodeWindow::applyRequested, this, [this, a](const QStringList& lines) {
        if (m_page)
            m_page->edit([a, lines](QStringList& all) { AirEdits::replaceAnimation(all, a, lines); });
    });
    m_codeWindow->show();
}

void AnimationsEditor::saveGif()
{
    AnimationPage* page = m_page;
    const AirAnimation* animation = page->animation();
    auto* project = qobject_cast<Project*>(page->file()->parent());
    const QStringList files = project ? project->editorFiles(QStringLiteral("sprites")) : QStringList();
    SpriteFile* sprites =
        project && !files.isEmpty() ? project->spriteFile(files.first(), QStringLiteral("sprites")) : nullptr;
    if (!animation || !sprites)
        return;
    QWidget* parent = page->window();
    Config& config = m_context.config();
    const QString folder = config.value(QStringLiteral("gif_path"), project->directory());
    QString path = FileDialogs::getSaveFileName(parent, tr("Save as animated GIF..."), folder,
                                                tr("Graphics Interchange Format (*.gif)"));
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".gif");
    GifOptionsDialog options(parent);
    if (options.exec() != QDialog::Accepted)
        return;
    config.setValue(QStringLiteral("gif_path"), QFileInfo(path).absolutePath());
    QString error;
    const QByteArray gif = animationGif(*animation, sprites->document(), options.scale(), options.velocity(), &error);
    QFile file(path);
    if (gif.isEmpty() || !file.open(QIODevice::WriteOnly) || file.write(gif) != gif.size())
        QMessageBox::warning(parent, tr("Save as animated GIF..."), gif.isEmpty() ? error : file.errorString());
}

void AnimationsEditor::copyBox()
{
    if (!m_page || !m_page->frame() || !m_page->selectedBox().isValid())
        return;
    const AirEdits::BoxRef ref = m_page->selectedBox();
    const AirFrame* f = m_page->frame();
    if (ref.block >= f->blocks.size() || ref.box >= f->blocks.at(ref.block).boxes.size())
        return;
    copiedBox() = CopiedBox{f->blocks.at(ref.block).type, f->blocks.at(ref.block).boxes.at(ref.box)};
}

void AnimationsEditor::cutBox()
{
    if (!m_page || !m_page->selectedBox().isValid())
        return;
    copyBox();
    perform(QStringLiteral("anim_box_del"));
}

void AnimationsEditor::pasteBox()
{
    if (!m_page || !m_page->frame() || copiedBox().type == 0)
        return;
    const CopiedBox copied = copiedBox();
    const int a = m_page->current();
    const int f = m_page->currentFrame();
    AirEdits::BoxRef added;
    m_page->edit(
        [a, f, copied, &added](QStringList& lines) { added = AirEdits::addBox(lines, a, f, copied.type, copied.box); });
    m_page->setSelectedBox(added);
}

void AnimationsEditor::bindExplorer(QWidget* panel)
{
    m_explorerGroups = control<QComboBox>(panel, "thumb_grp_list");
    m_explorerView = control<QListView>(panel, "thumb_view");
    if (m_explorerView && !m_explorerModel)
        m_explorerModel = new QStandardItemModel(this);
    if (m_explorerGroups)
        connect(m_explorerGroups, &QComboBox::currentIndexChanged, this, [this] {
            if (!m_updating)
                refreshExplorer();
        });
    const struct
    {
        const char* name;
        bool all;
        bool newAnimation;
    } buttons[] = {{"thumb_action1", false, false},
                   {"thumb_action2", true, false},
                   {"thumb_action3", false, true},
                   {"thumb_action4", true, true}};
    for (const auto& b : buttons) {
        auto* button = control<QToolButton>(panel, b.name);
        if (!button)
            continue;
        connect(button, &QToolButton::clicked, this, [this, all = b.all, newAnimation = b.newAnimation] {
            if (!m_page || !m_explorerActive)
                return;
            newAnimation ? newAnimationFromExplorer(all) : addExplorerFrames(all);
        });
    }
}

void AnimationsEditor::setExplorerActive(bool active)
{
    m_explorerActive = active;
    refreshExplorer();
}

void AnimationsEditor::refreshExplorer()
{
    if (!m_explorerView || !m_explorerModel || !m_page || !m_explorerActive)
        return;
    auto* project = qobject_cast<Project*>(m_page->file()->parent());
    const QStringList files = project ? project->editorFiles(QStringLiteral("sprites")) : QStringList();
    SpriteFile* sprites =
        project && !files.isEmpty() ? project->spriteFile(files.first(), QStringLiteral("sprites")) : nullptr;
    m_explorerView->setModel(m_explorerModel);
    m_explorerView->setViewMode(QListView::IconMode);
    m_explorerView->setMovement(QListView::Static);
    m_explorerView->setResizeMode(QListView::Adjust);
    m_explorerView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_explorerView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    const bool updating = std::exchange(m_updating, true);
    QList<int> groups;
    if (sprites) {
        for (const SpriteEntry& entry : sprites->document().sprites()) {
            if (!groups.contains(entry.group()))
                groups << entry.group();
        }
    }
    std::sort(groups.begin(), groups.end());
    int group = groups.isEmpty() ? -1 : groups.first();
    if (m_explorerGroups) {
        if (m_explorerGroups->currentIndex() >= 0 && groups.contains(m_explorerGroups->currentText().toInt()))
            group = m_explorerGroups->currentText().toInt();
        m_explorerGroups->clear();
        for (int g : std::as_const(groups))
            m_explorerGroups->addItem(QString::number(g));
        m_explorerGroups->setCurrentIndex(int(groups.indexOf(group)));
    }
    const int size = std::max(16, m_context.config().intValue(QStringLiteral("thumb_size"), 64));
    m_explorerView->setIconSize(QSize(size, size));
    m_explorerModel->clear();
    if (sprites) {
        const SpriteDocument& doc = sprites->document();
        for (int i = 0; i < doc.spriteCount(); ++i) {
            if (doc.sprites().at(i).group() != group)
                continue;
            const Formats::Image rgba = doc.imageRgba(i);
            QPixmap thumbnail;
            if (!rgba.pixelBuffer().empty())
                thumbnail = QPixmap::fromImage(
                    QImage(rgba.pixels().data(), rgba.width(), rgba.height(), rgba.width() * 4, QImage::Format_RGBA8888)
                        .scaled(size, size, Qt::KeepAspectRatio, Qt::FastTransformation));
            auto* item = new QStandardItem(QIcon(thumbnail), QString::number(doc.sprites().at(i).number()));
            item->setData(i, Qt::UserRole);
            m_explorerModel->appendRow(item);
        }
    }
    m_updating = updating;
}

QList<int> AnimationsEditor::explorerSprites(bool all) const
{
    QList<int> sprites;
    if (!m_explorerModel || !m_explorerView || !m_explorerView->selectionModel())
        return sprites;
    for (int row = 0; row < m_explorerModel->rowCount(); ++row) {
        const QModelIndex index = m_explorerModel->index(row, 0);
        if (all || m_explorerView->selectionModel()->isSelected(index))
            sprites << index.data(Qt::UserRole).toInt();
    }
    return sprites;
}

void AnimationsEditor::addExplorerFrames(bool all)
{
    auto* project = qobject_cast<Project*>(m_page->file()->parent());
    const QStringList files = project ? project->editorFiles(QStringLiteral("sprites")) : QStringList();
    SpriteFile* spriteFile =
        project && !files.isEmpty() ? project->spriteFile(files.first(), QStringLiteral("sprites")) : nullptr;
    const QList<int> sprites = explorerSprites(all);
    if (!spriteFile || sprites.isEmpty() || !m_page->animation())
        return;
    QList<AirFrame> frames;
    for (int index : sprites) {
        AirFrame frame = AirEdits::blankFrame(10);
        frame.group = spriteFile->document().sprites().at(index).group();
        frame.number = spriteFile->document().sprites().at(index).number();
        frames << frame;
    }
    const int a = m_page->current();
    const int f = m_page->currentFrame();
    int last = -1;
    m_page->edit([a, f, frames, &last](QStringList& lines) { last = AirEdits::insertFrames(lines, a, f, frames); });
    m_page->setCurrentFrame(last);
    m_explorerView->clearSelection();
}

void AnimationsEditor::newAnimationFromExplorer(bool all)
{
    auto* project = qobject_cast<Project*>(m_page->file()->parent());
    const QStringList files = project ? project->editorFiles(QStringLiteral("sprites")) : QStringList();
    SpriteFile* spriteFile =
        project && !files.isEmpty() ? project->spriteFile(files.first(), QStringLiteral("sprites")) : nullptr;
    const QList<int> sprites = explorerSprites(all);
    if (!spriteFile || sprites.isEmpty())
        return;
    AddAnimationDialog dialog(m_page->window());
    if (dialog.exec() != QDialog::Accepted)
        return;
    QList<AirFrame> frames;
    for (int index : sprites) {
        AirFrame frame = AirEdits::blankFrame(dialog.time());
        frame.group = spriteFile->document().sprites().at(index).group();
        frame.number = spriteFile->document().sprites().at(index).number();
        frames << frame;
    }
    const int a = m_page->current();
    const int number = dialog.number();
    const QString name = dialog.name();
    int added = -1;
    m_page->edit([a, number, name, frames, &added](QStringList& lines) {
        added = AirEdits::addAnimation(lines, a, number, name, frames);
    });
    m_page->setCurrent(added);
    m_explorerView->clearSelection();
}

} // namespace OFS
