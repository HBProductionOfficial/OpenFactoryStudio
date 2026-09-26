#include "shell/BackgroundsEditor.h"

#include "core/app/AppContext.h"
#include "core/app/Messages.h"
#include "core/bg/BgEdits.h"
#include "core/editors/Organize.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/settings/Config.h"
#include "core/sprites/SpriteFile.h"
#include "shell/BackgroundsPage.h"
#include "shell/OrganizerBinding.h"
#include "shell/dialogs/BackgroundDialogs.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QScrollBar>
#include <QSpinBox>
#include <QStandardItemModel>

#include <cmath>

namespace OFS {

namespace {

template <typename T>
T* control(QWidget* panel, const char* name)
{
    return panel ? panel->findChild<T*>(QLatin1String(name)) : nullptr;
}

// All widgets with that name, because the X: and Y: labels repeat.
template <typename T>
QList<T*> controls(QWidget* panel, const char* name)
{
    return panel ? panel->findChildren<T*>(QLatin1String(name)) : QList<T*>();
}

void setValue(QSpinBox* box, int value)
{
    if (box)
        box->setValue(value);
}

void setValue(QDoubleSpinBox* box, double value)
{
    if (box)
        box->setValue(value);
}

void setEnabled(QWidget* widget, bool on)
{
    if (widget)
        widget->setEnabled(on);
}

double valueOf(const QDoubleSpinBox* box)
{
    return box ? box->value() : 0;
}

int valueOf(const QSpinBox* box)
{
    return box ? box->value() : 0;
}

// Element types in combo order, spelled as written to the file.
const char* const TypeNames[] = {"Null", "Normal", "Anim", "Parallax", "Dummy"};
const char* const ControllerTypeNames[] = {"Null",   "Enable", "Visible", "Anim", "PosSet",
                                           "PosAdd", "VelSet", "VelAdd",  "SinX", "SinY"};

} // namespace

BackgroundsEditor::BackgroundsEditor(AppContext& context, QObject* parent)
    : QObject(parent)
    , m_context(context)
{}

void BackgroundsEditor::bindPanel(QWidget* panel)
{
    m_panel = panel;
    m_selector = control<QScrollBar>(panel, "bgs_sel");
    m_selectorLabel = control<QLabel>(panel, "bgs_sel_lb");
    m_name = control<QLineEdit>(panel, "bgs_name");
    m_type = control<QComboBox>(panel, "bgs_type");
    m_spriteGroup = control<QSpinBox>(panel, "bgs_spr_grp");
    m_spriteNumber = control<QSpinBox>(panel, "bgs_spr_num");
    m_action = control<QSpinBox>(panel, "bgs_anim_no");
    m_mask = control<QCheckBox>(panel, "bgs_mask");
    m_startX = control<QDoubleSpinBox>(panel, "bgs_start_x");
    m_startY = control<QDoubleSpinBox>(panel, "bgs_start_y");
    m_deltaX = control<QDoubleSpinBox>(panel, "bgs_delta_x");
    m_deltaY = control<QDoubleSpinBox>(panel, "bgs_delta_y");
    m_velocityX = control<QDoubleSpinBox>(panel, "bgs_velocity_x");
    m_velocityY = control<QDoubleSpinBox>(panel, "bgs_velocity_y");
    m_id = control<QSpinBox>(panel, "bgs_id");
    m_layer = control<QSpinBox>(panel, "bgs_layer");
    m_link = control<QCheckBox>(panel, "bgs_pos_link");
    m_tileX = control<QSpinBox>(panel, "bgs_tile_x");
    m_tileY = control<QSpinBox>(panel, "bgs_tile_y");
    m_spacingX = control<QSpinBox>(panel, "bgs_tile_spacing_x");
    m_spacingY = control<QSpinBox>(panel, "bgs_tile_spacing_y");
    m_trans = control<QComboBox>(panel, "bgs_trans");
    m_source = control<QSpinBox>(panel, "bgs_transsrc");
    m_dest = control<QSpinBox>(panel, "bgs_transdst");
    m_parallaxMode = control<QComboBox>(panel, "bgs_parallax_mode");
    m_parallaxTop = control<QDoubleSpinBox>(panel, "bgs_parallax_top");
    m_parallaxBottom = control<QDoubleSpinBox>(panel, "bgs_parallax_bottom");
    m_scaleStartX = control<QDoubleSpinBox>(panel, "bgs_scale_start_x");
    m_scaleStartY = control<QDoubleSpinBox>(panel, "bgs_scale_start_y");
    m_scaleDeltaX = control<QDoubleSpinBox>(panel, "bgs_scale_delta_x");
    m_scaleDeltaY = control<QDoubleSpinBox>(panel, "bgs_scale_delta_y");
    m_yScaleStart = control<QDoubleSpinBox>(panel, "bgs_y_scale_start");
    m_yScaleDelta = control<QDoubleSpinBox>(panel, "bgs_y_scale_delta");
    m_windowDeltaX = control<QDoubleSpinBox>(panel, "bgs_window_delta_x");
    m_windowDeltaY = control<QDoubleSpinBox>(panel, "bgs_window_delta_y");
    m_windowMode = control<QComboBox>(panel, "bgs_window_mode");
    m_windowTop = control<QSpinBox>(panel, "bgs_window_top");
    m_windowLeft = control<QSpinBox>(panel, "bgs_window_left");
    m_windowBottom = control<QSpinBox>(panel, "bgs_window_bottom");
    m_windowRight = control<QSpinBox>(panel, "bgs_window_right");
    static const char* const sinX[] = {"bgs_sin_x_ampl", "bgs_sin_x_period", "bgs_sin_x_offset"};
    static const char* const sinY[] = {"bgs_sin_y_ampl", "bgs_sin_y_period", "bgs_sin_y_offset"};
    for (int k = 0; k < 3; ++k) {
        m_sinX[k] = control<QDoubleSpinBox>(panel, sinX[k]);
        m_sinY[k] = control<QDoubleSpinBox>(panel, sinY[k]);
    }

    for (QCheckBox* box : {m_mask.data(), m_link.data()}) {
        if (box)
            box->setStatusTip(box->text());
    }
    for (QDoubleSpinBox* box : panel ? panel->findChildren<QDoubleSpinBox*>() : QList<QDoubleSpinBox*>()) {
        if (box->objectName().startsWith(QLatin1String("bgs_")))
            box->setDecimals(4);
    }
    if (m_type) {
        m_type->clear();
        m_type->addItems({tr("Null"), tr("Normal"), tr("Animation"), tr("Parallax"), tr("Dummy")});
    }
    if (m_trans) {
        m_trans->clear();
        m_trans->addItems({tr("None"), tr("Default"), tr("Addition"), tr("Addition 50%"), tr("Subtraction")});
    }
    if (m_parallaxMode) {
        m_parallaxMode->clear();
        m_parallaxMode->addItems({tr("Width (centered)"), tr("Width (x axis aligned)"), tr("X scale (pre-projected)")});
    }

    if (m_selector)
        connect(m_selector, &QScrollBar::valueChanged, this, [this](int value) {
            if (!m_updating && m_page)
                m_page->setCurrent(value);
        });
    if (m_name)
        connect(m_name, &QLineEdit::textEdited, this, [this](const QString& text) {
            const BgGroup* g = m_page ? m_page->currentGroup() : nullptr;
            if (m_updating || !g)
                return;
            const QString prefix = g->prefix;
            editElement([&](QStringList& lines, int header) {
                BgEdits::setHeader(lines, header, prefix + QLatin1Char(' ') + text.trimmed());
            });
        });
    if (m_type)
        connect(m_type, &QComboBox::activated, this, &BackgroundsEditor::changeType);
    auto spriteChanged = [this] {
        if (m_updating)
            return;
        const QString value = QStringLiteral("%1, %2").arg(valueOf(m_spriteGroup)).arg(valueOf(m_spriteNumber));
        editElement([&](QStringList& lines, int header) {
            BgEdits::setValue(lines, header, QStringLiteral("spriteno"), value);
        });
    };
    for (QSpinBox* box : {m_spriteGroup.data(), m_spriteNumber.data()}) {
        if (box)
            connect(box, &QSpinBox::valueChanged, this, spriteChanged);
    }
    if (m_action)
        connect(m_action, &QSpinBox::valueChanged, this, [this](int value) {
            if (!m_updating)
                editElement([&](QStringList& lines, int header) {
                    BgEdits::setValue(lines, header, QStringLiteral("actionno"), QString::number(value));
                });
        });
    // Check boxes written as "key = 1", removed when off.
    auto flag = [this](QCheckBox* box, const char* key) {
        if (!box)
            return;
        connect(box, &QCheckBox::toggled, this, [this, key](bool on) {
            if (!m_updating)
                editElement([&](QStringList& lines, int header) {
                    BgEdits::setNumbers(lines, header, QLatin1String(key), {on ? 1.0 : 0.0}, {0});
                });
        });
    };
    flag(m_mask, "mask");
    flag(m_link, "positionlink");
    auto numbers = [this](QList<QAbstractSpinBox*> boxes, const char* key, QList<double> defaults) {
        auto write = [this, boxes, key, defaults] {
            if (m_updating)
                return;
            QList<double> values;
            for (QAbstractSpinBox* box : boxes) {
                if (auto* d = qobject_cast<QDoubleSpinBox*>(box))
                    values << d->value();
                else if (auto* i = qobject_cast<QSpinBox*>(box))
                    values << i->value();
            }
            editElement([&](QStringList& lines, int header) {
                BgEdits::setNumbers(lines, header, QLatin1String(key), values, defaults);
            });
        };
        for (QAbstractSpinBox* box : boxes) {
            if (auto* d = qobject_cast<QDoubleSpinBox*>(box))
                connect(d, &QDoubleSpinBox::valueChanged, this, write);
            else if (auto* i = qobject_cast<QSpinBox*>(box))
                connect(i, &QSpinBox::valueChanged, this, write);
        }
    };
    if (m_startX && m_startY)
        numbers({m_startX, m_startY}, "start", {0, 0});
    if (m_deltaX && m_deltaY)
        numbers({m_deltaX, m_deltaY}, "delta", {1, 1});
    if (m_velocityX && m_velocityY)
        numbers({m_velocityX, m_velocityY}, "velocity", {0, 0});
    if (m_id)
        numbers({m_id}, "id", {0});
    if (m_layer)
        numbers({m_layer}, "layerno", {0});
    if (m_tileX && m_tileY)
        numbers({m_tileX, m_tileY}, "tile", {0, 0});
    if (m_spacingX && m_spacingY)
        numbers({m_spacingX, m_spacingY}, "tilespacing", {0, 0});
    if (m_scaleStartX && m_scaleStartY)
        numbers({m_scaleStartX, m_scaleStartY}, "scalestart", {1, 1});
    if (m_scaleDeltaX && m_scaleDeltaY)
        numbers({m_scaleDeltaX, m_scaleDeltaY}, "scaledelta", {0, 0});
    if (m_yScaleStart)
        numbers({m_yScaleStart}, "yscalestart", {100});
    if (m_yScaleDelta)
        numbers({m_yScaleDelta}, "yscaledelta", {0});
    if (m_windowDeltaX && m_windowDeltaY)
        numbers({m_windowDeltaX, m_windowDeltaY}, "windowdelta", {0, 0});
    if (m_sinX[0] && m_sinX[1] && m_sinX[2])
        numbers({m_sinX[0], m_sinX[1], m_sinX[2]}, "sin.x", {0, 0, 0});
    if (m_sinY[0] && m_sinY[1] && m_sinY[2])
        numbers({m_sinY[0], m_sinY[1], m_sinY[2]}, "sin.y", {0, 0, 0});
    if (m_trans)
        connect(m_trans, &QComboBox::activated, this, &BackgroundsEditor::changeTransparency);
    // Source / Dest: "trans = AddAlpha" with "alpha = s, d".
    for (QSpinBox* box : {m_source.data(), m_dest.data()}) {
        if (!box)
            continue;
        connect(box, &QSpinBox::valueChanged, this, [this] {
            if (m_updating)
                return;
            const QString alpha = QStringLiteral("%1, %2").arg(valueOf(m_source)).arg(valueOf(m_dest));
            editElement([&](QStringList& lines, int header) {
                BgEdits::setValue(lines, header, QStringLiteral("trans"), QStringLiteral("AddAlpha"));
                BgEdits::setValue(lines, header, QStringLiteral("alpha"), alpha);
            });
        });
    }
    if (m_parallaxMode)
        connect(m_parallaxMode, &QComboBox::activated, this, &BackgroundsEditor::changeParallaxMode);
    for (QDoubleSpinBox* box : {m_parallaxTop.data(), m_parallaxBottom.data()}) {
        if (!box)
            continue;
        connect(box, &QDoubleSpinBox::valueChanged, this, [this] {
            if (m_updating || !m_parallaxMode)
                return;
            const bool xscale = m_parallaxMode->currentIndex() == 2;
            const QString value = BgEdits::numbers({valueOf(m_parallaxTop), valueOf(m_parallaxBottom)});
            editElement([&](QStringList& lines, int header) {
                BgEdits::setValue(lines, header, xscale ? QStringLiteral("xscale") : QStringLiteral("width"), value);
            });
        });
    }
    if (m_windowMode)
        connect(m_windowMode, &QComboBox::activated, this, &BackgroundsEditor::changeWindowMode);
    for (QSpinBox* box : {m_windowTop.data(), m_windowLeft.data(), m_windowBottom.data(), m_windowRight.data()}) {
        if (box)
            connect(box, &QSpinBox::valueChanged, this, [this] {
                if (!m_updating)
                    writeWindow();
            });
    }
    refresh();
}

void BackgroundsEditor::bindControllers(QWidget* panel)
{
    m_controllers = panel;
    m_defSelector = control<QScrollBar>(panel, "bgctrls_def_sel");
    m_defLabel = control<QLabel>(panel, "bgctrls_def_sel_lb");
    m_defName = control<QLineEdit>(panel, "bgctrls_def_name");
    m_defLoop = control<QSpinBox>(panel, "bgctrls_def_loop");
    m_ids.clear();
    for (int i = 1; i <= 10; ++i)
        m_ids << control<QSpinBox>(panel, QStringLiteral("bgctrls_def_id_%1").arg(i).toLatin1().constData());
    m_ctrlSelector = control<QScrollBar>(panel, "bgctrls_sel");
    m_ctrlLabel = control<QLabel>(panel, "bgctrls_sel_lb");
    m_ctrlName = control<QLineEdit>(panel, "bgctrls_name");
    m_ctrlType = control<QComboBox>(panel, "bgctrls_type");
    m_ctrlBool = control<QCheckBox>(panel, "bgctrls_bool");
    m_ctrlAnimLabel = control<QLabel>(panel, "bgctrls_anim_no_lb");
    m_ctrlAnim = control<QSpinBox>(panel, "bgctrls_anim_no");
    m_ctrlXOn = control<QCheckBox>(panel, "bgctrls_x_lb");
    m_ctrlYOn = control<QCheckBox>(panel, "bgctrls_y_lb");
    m_ctrlX = control<QDoubleSpinBox>(panel, "bgctrls_x");
    m_ctrlY = control<QDoubleSpinBox>(panel, "bgctrls_y");
    m_ctrlSine[0] = control<QDoubleSpinBox>(panel, "bgctrls_ampl");
    m_ctrlSine[1] = control<QDoubleSpinBox>(panel, "bgctrls_period");
    m_ctrlSine[2] = control<QDoubleSpinBox>(panel, "bgctrls_offset");
    m_ctrlSineLabels = {control<QLabel>(panel, "bgctrls_ampl_lb"), control<QLabel>(panel, "bgctrls_period_lb"),
                        control<QLabel>(panel, "bgctrls_offset_lb")};
    m_ctrlStart = control<QSpinBox>(panel, "bgctrls_start_time");
    m_ctrlEnd = control<QSpinBox>(panel, "bgctrls_end_time");
    m_ctrlLoop = control<QSpinBox>(panel, "bgctrls_loop_time");
    for (QDoubleSpinBox* box :
         {m_ctrlX.data(), m_ctrlY.data(), m_ctrlSine[0].data(), m_ctrlSine[1].data(), m_ctrlSine[2].data()}) {
        if (box)
            box->setDecimals(4);
    }
    if (m_ctrlType) {
        m_ctrlType->clear();
        m_ctrlType->addItems({tr("Null"), tr("Enable"), tr("Visible"), tr("Animation"), tr("Set position"),
                              tr("Add position"), tr("Set velocity"), tr("Add velocity"), tr("Sine (x-axis)"),
                              tr("Sine (y-axis)")});
    }

    if (m_defSelector)
        connect(m_defSelector, &QScrollBar::valueChanged, this, [this](int value) {
            if (!m_updating && m_page)
                m_page->setCurrentDefinition(value);
        });
    if (m_ctrlSelector)
        connect(m_ctrlSelector, &QScrollBar::valueChanged, this, [this](int value) {
            if (!m_updating && m_page)
                m_page->setCurrentController(value);
        });
    if (m_defName)
        connect(m_defName, &QLineEdit::textEdited, this, [this](const QString& text) {
            const BgGroup* g = m_page ? m_page->currentGroup() : nullptr;
            if (m_updating || !g)
                return;
            const QString header = g->prefix + QStringLiteral("CtrlDef ") + text.trimmed();
            editDefinition([&](QStringList& lines, int line) { BgEdits::setHeader(lines, line, header); });
        });
    if (m_ctrlName)
        connect(m_ctrlName, &QLineEdit::textEdited, this, [this](const QString& text) {
            const BgGroup* g = m_page ? m_page->currentGroup() : nullptr;
            if (m_updating || !g)
                return;
            const QString header = g->prefix + QStringLiteral("Ctrl ") + text.trimmed();
            editController([&](QStringList& lines, int line) { BgEdits::setHeader(lines, line, header); });
        });
    if (m_defLoop)
        connect(m_defLoop, &QSpinBox::valueChanged, this, [this](int value) {
            if (!m_updating)
                editDefinition([&](QStringList& lines, int line) {
                    BgEdits::setNumbers(lines, line, QStringLiteral("looptime"), {double(value)}, {-1});
                });
        });
    for (const QPointer<QSpinBox>& box : std::as_const(m_ids)) {
        if (box)
            connect(box, &QSpinBox::valueChanged, this, [this] {
                if (!m_updating)
                    writeIds();
            });
    }
    if (m_ctrlType)
        connect(m_ctrlType, &QComboBox::activated, this, &BackgroundsEditor::changeControllerType);
    if (m_ctrlBool)
        connect(m_ctrlBool, &QCheckBox::toggled, this, [this](bool on) {
            if (!m_updating)
                editController([&](QStringList& lines, int line) {
                    BgEdits::setNumbers(lines, line, QStringLiteral("value"), {on ? 1.0 : 0.0}, {0});
                });
        });
    if (m_ctrlAnim)
        connect(m_ctrlAnim, &QSpinBox::valueChanged, this, [this](int value) {
            if (!m_updating)
                editController([&](QStringList& lines, int line) {
                    BgEdits::setValue(lines, line, QStringLiteral("value"), QString::number(value));
                });
        });
    auto axis = [this](QCheckBox* on, QDoubleSpinBox* box, const char* key) {
        if (on)
            connect(on, &QCheckBox::toggled, this, [this, box, key](bool checked) {
                if (box)
                    box->setEnabled(checked);
                if (m_updating)
                    return;
                editController([&](QStringList& lines, int line) {
                    if (checked)
                        BgEdits::setValue(lines, line, QLatin1String(key), BgEdits::number(valueOf(box)));
                    else
                        BgEdits::removeKey(lines, line, QLatin1String(key));
                });
            });
        if (box)
            connect(box, &QDoubleSpinBox::valueChanged, this, [this, key](double value) {
                if (!m_updating)
                    editController([&](QStringList& lines, int line) {
                        BgEdits::setValue(lines, line, QLatin1String(key), BgEdits::number(value));
                    });
            });
    };
    axis(m_ctrlXOn, m_ctrlX, "x");
    axis(m_ctrlYOn, m_ctrlY, "y");
    for (const QPointer<QDoubleSpinBox>& box : m_ctrlSine) {
        if (box)
            connect(box, &QDoubleSpinBox::valueChanged, this, [this] {
                if (m_updating)
                    return;
                const QString value =
                    BgEdits::numbers({valueOf(m_ctrlSine[0]), valueOf(m_ctrlSine[1]), valueOf(m_ctrlSine[2])});
                editController([&](QStringList& lines, int line) {
                    BgEdits::setValue(lines, line, QStringLiteral("value"), value);
                });
            });
    }
    for (QSpinBox* box : {m_ctrlStart.data(), m_ctrlEnd.data(), m_ctrlLoop.data()}) {
        if (box)
            connect(box, &QSpinBox::valueChanged, this, [this] {
                if (!m_updating)
                    writeTime();
            });
    }
    refreshControllers();
}

void BackgroundsEditor::bindOrganizer(QWidget* panel)
{
    if (m_organizer)
        disconnect(m_organizer, nullptr, this, nullptr);
    m_organizer = control<QListView>(panel, "org_list");
    setOrganizerActive(m_organizerActive);
}

void BackgroundsEditor::setOrganizerActive(bool active)
{
    m_organizerActive = active;
    if (!m_organizer)
        return;
    disconnect(m_organizer, nullptr, this, nullptr);
    if (!active)
        OrganizerDrops::disable(m_organizer, this);
    if (!active)
        return;
    if (!m_organizerModel) {
        m_organizerModel = new QStandardItemModel(this);
        connect(m_organizerModel, &QStandardItemModel::itemChanged, this, [this] {
            if (m_updating || !m_page)
                return;
            QSet<int> hidden;
            for (int row = 0; row < m_organizerModel->rowCount(); ++row) {
                if (m_organizerModel->item(row)->checkState() != Qt::Checked)
                    hidden.insert(row);
            }
            m_page->setHiddenElements(hidden);
        });
    }
    m_organizer->setModel(m_organizerModel);
    m_organizer->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_organizer->setEditTriggers(QAbstractItemView::NoEditTriggers);
    OrganizerDrops::enable(m_organizer, this, [this](int target) { dropElements(target); });
    connect(m_organizer, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
        if (m_page)
            m_page->setCurrent(index.row());
    });
    refreshOrganizer();
}

void BackgroundsEditor::refreshOrganizer()
{
    if (!m_organizerActive || !m_organizer || !m_organizerModel)
        return;
    const bool updating = std::exchange(m_updating, true);
    const QList<int> selected = organizerSelection();
    m_organizerModel->clear();
    const BgGroup* g = m_page ? m_page->currentGroup() : nullptr;
    for (int i = 0; g && i < g->elements.size(); ++i) {
        auto* item = new QStandardItem(g->elements[i].section.name);
        item->setCheckable(true);
        item->setCheckState(m_page->hiddenElements().contains(i) ? Qt::Unchecked : Qt::Checked);
        m_organizerModel->appendRow(item);
    }
    for (int row : selected) {
        if (row < m_organizerModel->rowCount())
            m_organizer->selectionModel()->select(m_organizerModel->index(row, 0), QItemSelectionModel::Select);
    }
    m_updating = updating;
}

QList<int> BackgroundsEditor::organizerSelection() const
{
    QList<int> rows;
    if (m_organizer && m_organizer->selectionModel() && m_organizer->model() == m_organizerModel) {
        for (const QModelIndex& index : m_organizer->selectionModel()->selectedRows())
            rows << index.row();
    }
    std::sort(rows.begin(), rows.end());
    return rows;
}

void BackgroundsEditor::moveElements(const QString& id)
{
    const BgGroup* g = m_page ? m_page->currentGroup() : nullptr;
    if (!g || g->elements.isEmpty() || id == QLatin1String("org_auto"))
        return;
    QList<int> rows = organizerSelection();
    if (rows.isEmpty() && m_page->current() >= 0)
        rows << m_page->current();
    if (rows.isEmpty())
        return;
    const int count = int(g->elements.size());
    const int delta = id == QLatin1String("org_up")        ? -1
                      : id == QLatin1String("org_down")    ? 1
                      : id == QLatin1String("org_up_10")   ? -10
                      : id == QLatin1String("org_down_10") ? 10
                      : id == QLatin1String("org_top")     ? -count
                                                           : count;
    const int group = m_page->group();
    QList<int> moved = rows;
    m_page->edit([&](QStringList& lines) {
        for (int step = 0; step < std::abs(delta); ++step) {
            QList<int> next = moved;
            const bool up = delta < 0;
            bool changed = false;
            for (int k = 0; k < moved.size(); ++k) {
                const int i = up ? k : int(moved.size()) - 1 - k;
                const int row = next[i];
                const int target = row + (up ? -1 : 1);
                if (target < 0 || target >= count || next.contains(target))
                    continue;
                const BgDocument doc = BgDocument::parse(lines);
                const QList<BgElement>& elements = doc.groups()[group].elements;
                BgEdits::swapSections(lines, elements[row].section.headerLine, elements[target].section.headerLine);
                next[i] = target;
                changed = true;
            }
            moved = next;
            if (!changed)
                break;
        }
    });
    if (m_organizer && m_organizerModel) {
        m_organizer->selectionModel()->clearSelection();
        for (int row : std::as_const(moved))
            m_organizer->selectionModel()->select(m_organizerModel->index(row, 0), QItemSelectionModel::Select);
    }
}

void BackgroundsEditor::dropElements(int target)
{
    const BgGroup* g = m_page ? m_page->currentGroup() : nullptr;
    const QList<int> rows = organizerSelection();
    if (!g || rows.isEmpty())
        return;
    const int count = int(g->elements.size());
    QList<int> moved;
    const QList<int> order = Organize::dropOrder(count, rows, target, &moved);
    if (moved == rows)
        return;
    const int group = m_page->group();
    m_page->edit([&](QStringList& lines) {
        const BgDocument doc = BgDocument::parse(lines);
        QList<int> headers;
        for (const BgElement& element : doc.groups()[group].elements)
            headers << element.section.headerLine;
        const QList<Organize::Block> blocks = Organize::sectionBlocks(lines, headers);
        if (blocks.size() == order.size())
            Organize::reorderBlocks(lines, blocks, order);
    });
    if (m_organizer && m_organizerModel) {
        m_organizer->selectionModel()->clearSelection();
        for (int row : std::as_const(moved))
            m_organizer->selectionModel()->select(m_organizerModel->index(row, 0), QItemSelectionModel::Select);
    }
}

void BackgroundsEditor::setPage(BackgroundsPage* page)
{
    if (m_page == page)
        return;
    if (m_page)
        disconnect(m_page, nullptr, this, nullptr);
    m_page = page;
    if (m_page) {
        auto update = [this] {
            refresh();
            refreshControllers();
            refreshOrganizer();
            Q_EMIT stateChanged();
        };
        connect(m_page, &BackgroundsPage::currentChanged, this, update);
        connect(m_page, &BackgroundsPage::documentChanged, this, update);
        connect(m_page, &BackgroundsPage::modeChanged, this, update);
    }
    refresh();
    refreshControllers();
    refreshOrganizer();
    Q_EMIT stateChanged();
}

QStringList BackgroundsEditor::groupNames() const
{
    QStringList names;
    if (m_page) {
        for (const BgGroup& g : m_page->document().groups())
            names << g.prefix;
    }
    return names;
}

int BackgroundsEditor::group() const
{
    return m_page ? m_page->group() : -1;
}

void BackgroundsEditor::setGroup(int group)
{
    if (m_page)
        m_page->setGroup(group);
}

bool BackgroundsEditor::version11() const
{
    auto* project = m_page && m_page->file() ? qobject_cast<Project*>(m_page->file()->parent()) : nullptr;
    return project && (project->versionId() == QLatin1String("1.1") || project->versionId() == QLatin1String("ext"));
}

double BackgroundsEditor::spriteWidth() const
{
    const BgElement* e = m_page ? m_page->element() : nullptr;
    auto* project = m_page && m_page->file() ? qobject_cast<Project*>(m_page->file()->parent()) : nullptr;
    if (!e || !project)
        return 0;
    SpriteFile* sprites = project->mainSpriteFile();
    if (!sprites)
        return 0;
    const int index = sprites->document().findSprite(e->spriteGroup, e->spriteNumber);
    return index >= 0 ? sprites->document().sprites()[index].width() : 0;
}

void BackgroundsEditor::editElement(const std::function<void(QStringList&, int)>& change)
{
    const BgElement* e = m_page ? m_page->element() : nullptr;
    if (!e)
        return;
    const int header = e->section.headerLine;
    m_page->edit([&](QStringList& lines) { change(lines, header); });
}

void BackgroundsEditor::editDefinition(const std::function<void(QStringList&, int)>& change)
{
    const BgControllerDef* d = m_page ? m_page->definition() : nullptr;
    if (!d)
        return;
    const int header = d->section.headerLine;
    m_page->edit([&](QStringList& lines) { change(lines, header); });
}

void BackgroundsEditor::editController(const std::function<void(QStringList&, int)>& change)
{
    const BgController* c = m_page ? m_page->controller() : nullptr;
    if (!c)
        return;
    const int header = c->section.headerLine;
    m_page->edit([&](QStringList& lines) { change(lines, header); });
}

void BackgroundsEditor::refresh()
{
    const bool updating = std::exchange(m_updating, true);
    const BgGroup* g = m_page ? m_page->currentGroup() : nullptr;
    const BgElement* e = m_page ? m_page->element() : nullptr;
    const int count = g ? int(g->elements.size()) : 0;
    if (m_selector) {
        m_selector->setRange(0, std::max(0, count - 1));
        m_selector->setValue(std::max(0, m_page ? m_page->current() : 0));
    }
    if (m_selectorLabel)
        m_selectorLabel->setText(QStringLiteral("%1 / %2").arg(e ? m_page->current() + 1 : 0).arg(count));
    if (e) {
        const BgSection& s = e->section;
        if (m_name && m_name->text() != s.name)
            m_name->setText(s.name);
        if (m_type) {
            static const BgElement::Type order[] = {BgElement::Type::Null, BgElement::Type::Normal,
                                                    BgElement::Type::Animation, BgElement::Type::Parallax,
                                                    BgElement::Type::Dummy};
            m_type->setCurrentIndex(int(std::find(std::begin(order), std::end(order), e->type) - std::begin(order)));
        }
        const bool hasSprite = s.value(QStringLiteral("spriteno")).has_value();
        setValue(m_spriteGroup, hasSprite ? e->spriteGroup : -1);
        setValue(m_spriteNumber, hasSprite ? e->spriteNumber : 0);
        setValue(m_action, e->action);
        if (m_mask)
            m_mask->setChecked(e->type != BgElement::Type::Animation && e->mask);
        setValue(m_startX, e->start.x());
        setValue(m_startY, e->start.y());
        setValue(m_deltaX, e->delta.x());
        setValue(m_deltaY, e->delta.y());
        setValue(m_velocityX, e->velocity.x());
        setValue(m_velocityY, e->velocity.y());
        setValue(m_id, e->id);
        setValue(m_layer, e->layer);
        if (m_link)
            m_link->setChecked(e->positionLink);
        setValue(m_tileX, e->tileX);
        setValue(m_tileY, e->tileY);
        setValue(m_spacingX, e->tileSpacingX);
        setValue(m_spacingY, e->tileSpacingY);
        // Transparency: no key means Default, and addalpha shows as Addition with its alphas.
        const QString trans = s.value(QStringLiteral("trans")).value_or(QString()).toLower();
        int transIndex = 1;
        int source = 256;
        int dest = 256;
        if (trans == QLatin1String("none")) {
            transIndex = 0;
        } else if (trans == QLatin1String("add")) {
            transIndex = 2;
        } else if (trans == QLatin1String("addalpha")) {
            transIndex = 2;
            source = e->alphaSource;
            dest = e->alphaDest;
        } else if (trans == QLatin1String("add1")) {
            transIndex = 3;
            dest = 128;
        } else if (trans == QLatin1String("sub")) {
            transIndex = 4;
        }
        if (m_trans)
            m_trans->setCurrentIndex(transIndex);
        setValue(m_source, source);
        setValue(m_dest, dest);
        if (m_parallaxMode)
            m_parallaxMode->setCurrentIndex(e->hasXScale && !e->hasWidth           ? 2
                                            : m_alignedParallax.contains(s.header) ? 1
                                                                                   : 0);
        setValue(m_parallaxTop, e->hasXScale && !e->hasWidth ? e->xScaleTop : e->widthTop);
        setValue(m_parallaxBottom, e->hasXScale && !e->hasWidth ? e->xScaleBottom : e->widthBottom);
        setValue(m_yScaleStart, e->yScaleStart);
        setValue(m_yScaleDelta, e->yScaleDelta);
        setValue(m_scaleStartX, e->scaleStart.x());
        setValue(m_scaleStartY, e->scaleStart.y());
        setValue(m_scaleDeltaX, e->scaleDelta.x());
        setValue(m_scaleDeltaY, e->scaleDelta.y());
        setValue(m_windowDeltaX, e->windowDelta.x());
        setValue(m_windowDeltaY, e->windowDelta.y());
        // Window modes: Full screen has no window, Top left uses window and Top center uses maskwindow (1.1).
        if (m_windowMode) {
            m_windowMode->clear();
            m_windowMode->addItems({tr("Full screen (disabled)"), tr("Top left (normal)")});
            if (version11())
                m_windowMode->addItem(tr("Top center (mask)"));
            m_windowMode->setCurrentIndex(!e->hasWindow ? 0 : e->maskWindow ? 2 : 1);
        }
        setValue(m_windowLeft, e->hasWindow ? e->window.left() : 0);
        setValue(m_windowTop, e->hasWindow ? e->window.top() : 0);
        setValue(m_windowRight, e->hasWindow ? e->window.right() : -1);
        setValue(m_windowBottom, e->hasWindow ? e->window.bottom() : -1);
        for (int k = 0; k < 3; ++k) {
            setValue(m_sinX[k], e->sinX[k]);
            setValue(m_sinY[k], e->sinY[k]);
        }
    } else if (m_name) {
        m_name->clear();
    }
    m_updating = updating;
    updateEnabled();
}

void BackgroundsEditor::updateEnabled()
{
    const BgElement* e = m_page ? m_page->element() : nullptr;
    const bool editable = e && m_page->mode() != BackgroundsPage::Mode::Playing;
    if (m_panel) {
        for (QWidget* w : m_panel->findChildren<QWidget*>()) {
            if (!w->objectName().startsWith(QLatin1String("bgs_")) || w->objectName() == QLatin1String("bgs_sel"))
                continue;
            if (qobject_cast<QAbstractSpinBox*>(w) || qobject_cast<QComboBox*>(w) || qobject_cast<QLineEdit*>(w) ||
                qobject_cast<QCheckBox*>(w))
                w->setEnabled(editable);
        }
    }
    setEnabled(m_selector, e != nullptr);
    if (!editable)
        return;
    const bool normal = e->type == BgElement::Type::Normal;
    const bool anim = e->type == BgElement::Type::Animation;
    const bool parallax = e->type == BgElement::Type::Parallax;
    const bool v11 = version11();
    setEnabled(m_spriteGroup, normal || parallax);
    setEnabled(m_spriteNumber, normal || parallax);
    setEnabled(m_action, anim || (parallax && v11));
    setEnabled(m_mask, normal || parallax);
    for (QWidget* w :
         std::initializer_list<QWidget*>{m_parallaxMode, m_parallaxTop, m_parallaxBottom, m_yScaleStart, m_yScaleDelta})
        setEnabled(w, parallax);
    for (QWidget* w : std::initializer_list<QWidget*>{m_scaleStartX, m_scaleStartY, m_scaleDeltaX, m_scaleDeltaY})
        setEnabled(w, v11);
    const bool bounds = m_windowMode && m_windowMode->currentIndex() > 0;
    for (QWidget* w : std::initializer_list<QWidget*>{m_windowTop, m_windowLeft, m_windowBottom, m_windowRight})
        setEnabled(w, bounds);
}

void BackgroundsEditor::changeType(int type)
{
    if (m_updating || type < 0 || type > 4)
        return;
    const double width = spriteWidth();
    editElement([&](QStringList& lines, int header) {
        BgEdits::setValue(lines, header, QStringLiteral("type"), QLatin1String(TypeNames[type]));
        const BgSection section = [&] {
            const BgDocument edited = BgDocument::parse(lines); // (kept alive for the loop)
            for (const BgSection& s : edited.sections()) {
                if (s.headerLine == header)
                    return s;
            }
            return BgSection();
        }();
        if (type == 2 && !section.value(QStringLiteral("actionno")))
            BgEdits::setValue(lines, header, QStringLiteral("actionno"), QStringLiteral("-1"));
        if (type == 3 && !section.value(QStringLiteral("width")) && !section.value(QStringLiteral("xscale")))
            BgEdits::setValue(lines, header, QStringLiteral("width"), BgEdits::numbers({width, width}));
    });
}

void BackgroundsEditor::changeTransparency(int index)
{
    if (m_updating)
        return;
    static const char* const values[] = {"None", nullptr, "Add", "Add1", "Sub"};
    editElement([&](QStringList& lines, int header) {
        BgEdits::removeKey(lines, header, QStringLiteral("alpha"));
        if (index == 1 || index < 0 || index > 4)
            BgEdits::removeKey(lines, header, QStringLiteral("trans"));
        else
            BgEdits::setValue(lines, header, QStringLiteral("trans"), QLatin1String(values[index]));
    });
}

void BackgroundsEditor::changeParallaxMode(int mode)
{
    const BgElement* e = m_page ? m_page->element() : nullptr;
    if (m_updating || !e)
        return;
    // Both width modes write "width". The values carry over, converted through the sprite's width.
    if (mode == 1)
        m_alignedParallax.insert(e->section.header);
    else
        m_alignedParallax.remove(e->section.header);
    const double width = spriteWidth();
    const bool toScale = mode == 2;
    QList<double> values;
    if (toScale && !e->hasXScale) {
        values = width > 0 ? QList<double>{e->widthTop / width, e->widthBottom / width} : QList<double>{1, 1};
    } else if (!toScale && !e->hasWidth) {
        values = width > 0 && e->hasXScale ? QList<double>{e->xScaleTop * width, e->xScaleBottom * width}
                                           : QList<double>{width, width};
    }
    if (values.isEmpty()) {
        refresh();
        return;
    }
    editElement([&](QStringList& lines, int header) {
        BgEdits::removeKey(lines, header, toScale ? QStringLiteral("width") : QStringLiteral("xscale"));
        BgEdits::setValue(lines, header, toScale ? QStringLiteral("xscale") : QStringLiteral("width"),
                          BgEdits::numbers(values));
    });
}

void BackgroundsEditor::changeWindowMode(int mode)
{
    const BgElement* e = m_page ? m_page->element() : nullptr;
    if (m_updating || !e)
        return;
    const QSizeF screen = m_page->screenSize();
    const int half = int(screen.width() / 2);
    // "window" counts from the screen's top-left with x2, y2 included. "maskwindow" counts from the top centre with x2,
    // y2 left out.
    int left = 0;
    int top = 0;
    int right = int(screen.width()) - 1;
    int bottom = int(screen.height()) - 1;
    if (e->hasWindow) {
        const QRect r = e->window;
        left = e->maskWindow ? r.left() + half : r.left();
        top = r.top();
        right = e->maskWindow ? r.right() - 1 + half : r.right();
        bottom = e->maskWindow ? r.bottom() - 1 : r.bottom();
    }
    editElement([&](QStringList& lines, int header) {
        BgEdits::removeKey(lines, header, QStringLiteral("window"));
        BgEdits::removeKey(lines, header, QStringLiteral("maskwindow"));
        if (mode == 1)
            BgEdits::setValue(lines, header, QStringLiteral("window"),
                              BgEdits::numbers({double(left), double(top), double(right), double(bottom)}));
        else if (mode == 2)
            BgEdits::setValue(
                lines, header, QStringLiteral("maskwindow"),
                BgEdits::numbers({double(left - half), double(top), double(right + 1 - half), double(bottom + 1)}));
    });
}

void BackgroundsEditor::writeWindow()
{
    if (!m_windowMode || m_windowMode->currentIndex() <= 0)
        return;
    const QString key = m_windowMode->currentIndex() == 2 ? QStringLiteral("maskwindow") : QStringLiteral("window");
    const QString value = BgEdits::numbers({double(valueOf(m_windowLeft)), double(valueOf(m_windowTop)),
                                            double(valueOf(m_windowRight)), double(valueOf(m_windowBottom))});
    editElement([&](QStringList& lines, int header) { BgEdits::setValue(lines, header, key, value); });
}

void BackgroundsEditor::refreshControllers()
{
    const bool updating = std::exchange(m_updating, true);
    const BgGroup* g = m_page ? m_page->currentGroup() : nullptr;
    const BgControllerDef* d = m_page ? m_page->definition() : nullptr;
    const BgController* c = m_page ? m_page->controller() : nullptr;
    const int defs = g ? int(g->controllerDefs.size()) : 0;
    if (m_defSelector) {
        m_defSelector->setRange(0, std::max(0, defs - 1));
        m_defSelector->setValue(std::max(0, m_page ? m_page->currentDefinition() : 0));
    }
    if (m_defLabel)
        m_defLabel->setText(QStringLiteral("%1 / %2").arg(d ? m_page->currentDefinition() + 1 : 0).arg(defs));
    if (m_defName && (!d || m_defName->text() != d->section.name))
        m_defName->setText(d ? d->section.name : QString());
    setValue(m_defLoop, d ? d->loopTime : -1);
    // IDs: -1 marks an unused slot, and the slots after the first unused one are disabled.
    const QList<int> ids = d && d->ids ? *d->ids : QList<int>();
    bool open = d != nullptr;
    for (int i = 0; i < m_ids.size(); ++i) {
        const int id = i < ids.size() ? ids[i] : -1;
        setValue(m_ids[i], id);
        setEnabled(m_ids[i], open && m_page->mode() != BackgroundsPage::Mode::Playing);
        open = open && id != -1;
    }
    const int ctrls = d ? int(d->controllers.size()) : 0;
    if (m_ctrlSelector) {
        m_ctrlSelector->setRange(0, std::max(0, ctrls - 1));
        m_ctrlSelector->setValue(std::max(0, m_page ? m_page->currentController() : 0));
    }
    if (m_ctrlLabel)
        m_ctrlLabel->setText(d ? QStringLiteral("%1 / %2").arg(c ? m_page->currentController() + 1 : 0).arg(ctrls)
                               : QString());
    if (m_ctrlName && (!c || m_ctrlName->text() != c->section.name))
        m_ctrlName->setText(c ? c->section.name : QString());
    // The combo's order differs from the model's: Null, Enable, Visible, Anim, PosSet, PosAdd,
    // VelSet, VelAdd, SinX, SinY.
    static const BgController::Type order[] = {
        BgController::Type::Null,   BgController::Type::Enable, BgController::Type::Visible, BgController::Type::Anim,
        BgController::Type::PosSet, BgController::Type::PosAdd, BgController::Type::VelSet,  BgController::Type::VelAdd,
        BgController::Type::SinX,   BgController::Type::SinY};
    const int comboIndex = c ? int(std::find(std::begin(order), std::end(order), c->type) - std::begin(order)) : 0;
    if (m_ctrlType)
        m_ctrlType->setCurrentIndex(comboIndex);
    const double value = c ? c->value.value(0) : 0;
    if (m_ctrlBool)
        m_ctrlBool->setChecked(c && value != 0);
    setValue(m_ctrlAnim, int(value));
    if (m_ctrlXOn)
        m_ctrlXOn->setChecked(c && c->x);
    if (m_ctrlYOn)
        m_ctrlYOn->setChecked(c && c->y);
    setValue(m_ctrlX, c && c->x ? *c->x : 0);
    setValue(m_ctrlY, c && c->y ? *c->y : 0);
    for (int k = 0; k < 3; ++k)
        setValue(m_ctrlSine[k], c ? c->value.value(k) : 0);
    const bool timed = c && c->section.value(QStringLiteral("time"));
    setValue(m_ctrlStart, timed ? c->startTime : -1);
    setValue(m_ctrlEnd, timed ? c->endTime : -1);
    setValue(m_ctrlLoop, timed ? c->loopTime.value_or(-1) : -1);
    m_updating = updating;

    const BgController::Type t = c ? c->type : BgController::Type::Null;
    const bool flags = t == BgController::Type::Enable || t == BgController::Type::Visible;
    const bool anim = t == BgController::Type::Anim;
    const bool xy = t == BgController::Type::PosSet || t == BgController::Type::PosAdd ||
                    t == BgController::Type::VelSet || t == BgController::Type::VelAdd;
    const bool sine = t == BgController::Type::SinX || t == BgController::Type::SinY;
    auto show = [](QWidget* w, bool visible) {
        if (w)
            w->setVisible(visible);
    };
    show(m_ctrlBool, c && flags);
    show(m_ctrlAnimLabel, c && anim);
    show(m_ctrlAnim, c && anim);
    for (QWidget* w : std::initializer_list<QWidget*>{m_ctrlXOn, m_ctrlYOn, m_ctrlX, m_ctrlY})
        show(w, c && xy);
    for (int k = 0; k < 3; ++k) {
        show(m_ctrlSine[k], c && sine);
        show(m_ctrlSineLabels.value(k), c && sine);
    }
    const bool playing = m_page && m_page->mode() == BackgroundsPage::Mode::Playing;
    for (QWidget* w : std::initializer_list<QWidget*>{m_defName, m_defLoop})
        setEnabled(w, d && !playing);
    setEnabled(m_defSelector, defs > 0);
    setEnabled(m_ctrlSelector, ctrls > 0);
    for (QWidget* w : std::initializer_list<QWidget*>{m_ctrlName, m_ctrlType, m_ctrlBool, m_ctrlAnim, m_ctrlXOn,
                                                      m_ctrlYOn, m_ctrlStart, m_ctrlEnd, m_ctrlLoop, m_ctrlSine[0],
                                                      m_ctrlSine[1], m_ctrlSine[2]})
        setEnabled(w, c && !playing);
    setEnabled(m_ctrlX, c && c->x && !playing);
    setEnabled(m_ctrlY, c && c->y && !playing);
}

void BackgroundsEditor::writeIds()
{
    QList<double> ids;
    for (const QPointer<QSpinBox>& box : std::as_const(m_ids)) {
        if (!box || box->value() == -1)
            break;
        ids << box->value();
    }
    editDefinition([&](QStringList& lines, int line) {
        if (ids.isEmpty())
            BgEdits::removeKey(lines, line, QStringLiteral("ctrlid"));
        else
            BgEdits::setValue(lines, line, QStringLiteral("ctrlID"), BgEdits::numbers(ids));
    });
    refreshControllers();
}

void BackgroundsEditor::writeTime()
{
    // "time = start", ", end" when it differs, ", loop" when there is one.
    const int start = valueOf(m_ctrlStart);
    const int end = valueOf(m_ctrlEnd);
    const int loop = valueOf(m_ctrlLoop);
    QString value = QString::number(start);
    if (end != start || loop != -1)
        value += QStringLiteral(", ") + QString::number(end);
    if (loop != -1)
        value += QStringLiteral(", ") + QString::number(loop);
    editController(
        [&](QStringList& lines, int line) { BgEdits::setValue(lines, line, QStringLiteral("time"), value); });
}

void BackgroundsEditor::changeControllerType(int type)
{
    if (m_updating || type < 0 || type > 9)
        return;
    editController([&](QStringList& lines, int line) {
        BgEdits::setValue(lines, line, QStringLiteral("type"), QLatin1String(ControllerTypeNames[type]));
        const bool xy = type >= 4 && type <= 7;
        if (!xy) {
            BgEdits::removeKey(lines, line, QStringLiteral("x"));
            BgEdits::removeKey(lines, line, QStringLiteral("y"));
        }
        if (xy || type == 0)
            BgEdits::removeKey(lines, line, QStringLiteral("value"));
        if (type == 1 || type == 2)
            BgEdits::setValue(lines, line, QStringLiteral("value"), QStringLiteral("1"));
        if (type == 3)
            BgEdits::setValue(lines, line, QStringLiteral("value"), QStringLiteral("0"));
        if (type == 8 || type == 9)
            BgEdits::setValue(lines, line, QStringLiteral("value"), QStringLiteral("0, 0, 0"));
    });
}

bool BackgroundsEditor::confirm(const QString& title, const QString& text)
{
    return confirmDelete(m_page ? m_page->window() : nullptr, m_context.config(), title, text);
}

const QStringList& BackgroundsEditor::actionIds()
{
    static const QStringList ids = {QStringLiteral("bgs_add"),         QStringLiteral("bgs_delete"),
                                    QStringLiteral("bgs_duplicate"),   QStringLiteral("bgs_play"),
                                    QStringLiteral("bgs_pause"),       QStringLiteral("bgs_stop"),
                                    QStringLiteral("bgctrls_def_add"), QStringLiteral("bgctrls_def_delete"),
                                    QStringLiteral("bgctrls_add"),     QStringLiteral("bgctrls_delete")};
    return ids;
}

bool BackgroundsEditor::perform(const QString& id)
{
    if (m_page && id.startsWith(QLatin1String("org_"))) {
        moveElements(id);
        return true;
    }
    if (!m_page || !(id.startsWith(QLatin1String("bgs_")) || id.startsWith(QLatin1String("bgctrls_"))))
        return false;
    BackgroundsPage* page = m_page;
    const BgGroup* g = page->currentGroup();
    const BgElement* e = page->element();
    if (id == QLatin1String("bgs_play")) {
        page->play();
    } else if (id == QLatin1String("bgs_pause")) {
        page->pause();
    } else if (id == QLatin1String("bgs_stop")) {
        page->stop();
    } else if (id == QLatin1String("bgs_add")) {
        if (!g)
            return true;
        AddBackgroundDialog dialog(page->window());
        if (dialog.exec() != QDialog::Accepted)
            return true;
        QStringList text{QLatin1Char('[') + g->prefix + QLatin1Char(' ') + dialog.name() + QLatin1Char(']')};
        static const char* const types[] = {"Normal", "Anim", "Parallax", "Dummy"};
        text << QStringLiteral("type = ") + QLatin1String(types[std::clamp(dialog.type(), 0, 3)]);
        if (dialog.type() == 1)
            text << QStringLiteral("actionno = -1");
        if (dialog.type() == 2)
            text << QStringLiteral("width = 0, 0");
        const int after = e ? e->section.headerLine : g->def.headerLine;
        const int position = e ? page->current() + 1 : 0;
        page->edit(
            [&](QStringList& lines) { BgEdits::insertSection(lines, BgEdits::block(lines, after).second, text); });
        page->setCurrent(position);
    } else if (id == QLatin1String("bgs_delete")) {
        if (!e || !confirm(tr("Remove"), tr("Do you really want to delete the background?")))
            return true;
        const int header = e->section.headerLine;
        page->edit([&](QStringList& lines) { BgEdits::removeSection(lines, header); });
    } else if (id == QLatin1String("bgs_duplicate")) {
        if (!e)
            return true;
        const int header = e->section.headerLine;
        const int position = page->current() + 1;
        page->edit([&](QStringList& lines) { BgEdits::duplicate(lines, header); });
        page->setCurrent(position);
    } else if (id == QLatin1String("bgctrls_def_add")) {
        if (!g)
            return true;
        int after = g->def.headerLine;
        if (!g->controllerDefs.isEmpty()) {
            const BgControllerDef& last = g->controllerDefs.last();
            after = last.controllers.isEmpty() ? last.section.headerLine : last.controllers.last().section.headerLine;
        } else if (!g->elements.isEmpty()) {
            after = g->elements.last().section.headerLine;
        }
        const int position = int(g->controllerDefs.size());
        const QString header = QLatin1Char('[') + g->prefix + QStringLiteral("CtrlDef ]");
        page->edit(
            [&](QStringList& lines) { BgEdits::insertSection(lines, BgEdits::block(lines, after).second, {header}); });
        page->setCurrentDefinition(position);
    } else if (id == QLatin1String("bgctrls_def_delete")) {
        const BgControllerDef* d = page->definition();
        if (!d || !confirm(tr("Remove definition"), tr("Do you really want to delete the controller definition?")))
            return true;
        const int first = d->section.headerLine;
        const int last = d->controllers.isEmpty() ? first : d->controllers.last().section.headerLine;
        page->edit([&](QStringList& lines) {
            const int start = BgEdits::block(lines, first).first;
            const int end = BgEdits::block(lines, last).second;
            BgEdits::removeLines(lines, start, end);
        });
    } else if (id == QLatin1String("bgctrls_add")) {
        const BgControllerDef* d = page->definition();
        if (!d || !g)
            return true;
        const BgController* c = page->controller();
        const int after =
            c ? c->section.headerLine
              : (d->controllers.isEmpty() ? d->section.headerLine : d->controllers.last().section.headerLine);
        const int position = c ? page->currentController() + 1 : int(d->controllers.size());
        const QStringList text{QLatin1Char('[') + g->prefix + QStringLiteral("Ctrl ]"), QStringLiteral("type = Null")};
        page->edit(
            [&](QStringList& lines) { BgEdits::insertSection(lines, BgEdits::block(lines, after).second, text); });
        page->setCurrentController(position);
    } else if (id == QLatin1String("bgctrls_delete")) {
        const BgController* c = page->controller();
        if (!c || !confirm(tr("Remove"), tr("Do you really want to delete the controller?")))
            return true;
        const int header = c->section.headerLine;
        page->edit([&](QStringList& lines) { BgEdits::removeSection(lines, header); });
    } else {
        return false;
    }
    Q_EMIT stateChanged();
    return true;
}

} // namespace OFS
