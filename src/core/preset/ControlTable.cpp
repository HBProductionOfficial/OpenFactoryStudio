#include "core/preset/ControlTable.h"

#include "core/preset/ControlRegistry.h"
#include "core/widgets/PaletteWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QTreeView>

namespace OFS {

namespace {

QString caption(const ControlDef& def)
{
    if (!def.caption || !*def.caption)
        return {};
    return QCoreApplication::translate("Controls", def.caption);
}

} // namespace

QWidget* createControlWidget(const ControlDef& def, QWidget* parent)
{
    const bool hasRange = def.minimum != 0 || def.maximum != 0;
    QWidget* w = nullptr;
    switch (def.kind) {
    case ControlKind::Button: {
        auto* b = new QToolButton(parent);
        b->setText(caption(def));
        b->setToolTip(caption(def));
        w = b;
        break;
    }
    case ControlKind::Label:
        w = new QLabel(caption(def), parent);
        break;
    case ControlKind::Int: {
        auto* s = new QSpinBox(parent);
        s->setRange(hasRange ? def.minimum : -32768, hasRange ? def.maximum : 32767);
        w = s;
        break;
    }
    case ControlKind::Float: {
        auto* s = new QDoubleSpinBox(parent);
        s->setDecimals(3);
        s->setRange(hasRange ? def.minimum : -100000.0, hasRange ? def.maximum : 100000.0);
        w = s;
        break;
    }
    case ControlKind::Selector: {
        auto* s = new QScrollBar(Qt::Horizontal, parent);
        s->setRange(0, hasRange ? def.maximum : 0);
        // Same as Fighter Factory: a click or Tab gives it the keyboard. The arrows step the items
        // and PgUp/PgDn step by 10. A scroll bar takes no focus by default.
        s->setFocusPolicy(Qt::StrongFocus);
        // Right/Up and PgUp step forward, as on a slider (Qt inverts a scroll bar's keys).
        s->setInvertedControls(false);
        w = s;
        break;
    }
    case ControlKind::Slider: {
        auto* s = new QSlider(Qt::Vertical, parent);
        if (hasRange)
            s->setRange(def.minimum, def.maximum);
        w = s;
        break;
    }
    case ControlKind::Check: {
        auto* c = new QCheckBox(caption(def), parent);
        c->setChecked(def.checked);
        w = c;
        break;
    }
    case ControlKind::Combo:
        w = new QComboBox(parent);
        break;
    case ControlKind::LineEdit:
        w = new QLineEdit(parent);
        break;
    case ControlKind::TextEdit:
        w = new QPlainTextEdit(parent);
        break;
    case ControlKind::Group:
        w = new QGroupBox(caption(def), parent);
        break;
    case ControlKind::Tree: {
        auto* t = new QTreeView(parent);
        t->setHeaderHidden(true);
        w = t;
        break;
    }
    case ControlKind::List:
        w = new QListView(parent);
        break;
    case ControlKind::Palette:
        w = new PaletteWidget(parent);
        break;
    case ControlKind::Widget:
        w = new QWidget(parent);
        break;
    }
    w->setObjectName(QString::fromLatin1(def.id));
    return w;
}

void registerControlTable(ControlRegistry& registry, const ControlDef* defs, int count)
{
    for (int i = 0; i < count; ++i) {
        const ControlDef def = defs[i];
        registry.registerFactory(QString::fromLatin1(def.id), [def](const Preset::Control&, QWidget* parent) {
            return createControlWidget(def, parent);
        });
    }
}

} // namespace OFS
