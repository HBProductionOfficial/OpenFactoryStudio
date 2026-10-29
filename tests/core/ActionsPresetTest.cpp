#include "core/actions/ActionRegistry.h"
#include "core/preset/ControlRegistry.h"
#include "core/preset/ControlTable.h"
#include "core/preset/PresetBuilder.h"
#include "core/preset/PresetEngine.h"

#include <QAction>
#include <QApplication>
#include <QGridLayout>
#include <QHelpEvent>
#include <QLabel>
#include <QSlider>
#include <QStandardItemModel>
#include <QTest>
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>
#include <QTreeView>

#include <memory>

using namespace OFS;

namespace {

// Native key text, so macOS shows the command symbol.
QString keyText(const char* keys)
{
    return QKeySequence(QString::fromLatin1(keys)).toString(QKeySequence::NativeText);
}

} // namespace

class ActionsPresetTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void editorScopesShareKeys()
    {
        ActionRegistry actions;
        QAction* sprAdd = actions.add("spr_add", "Add", "editor:sprites");
        QAction* animAdd = actions.add("anim_add", "Add", "editor:animations");
        QAction* open = actions.add("file_open", "Open...");
        actions.setKeys("spr_add", QKeySequence("Ctrl+A"));
        actions.setKeys("anim_add", QKeySequence("Ctrl+A"));
        actions.setKeys("file_open", QKeySequence("Ctrl+O"));

        QCOMPARE(open->shortcut(), QKeySequence("Ctrl+O"));
        QVERIFY(sprAdd->shortcut().isEmpty());
        QVERIFY(animAdd->shortcut().isEmpty());

        actions.setActiveEditorScope("editor:sprites");
        QCOMPARE(sprAdd->shortcut(), QKeySequence("Ctrl+A"));
        QVERIFY(animAdd->shortcut().isEmpty());

        actions.setActiveEditorScope("editor:animations");
        QVERIFY(sprAdd->shortcut().isEmpty());
        QCOMPARE(animAdd->shortcut(), QKeySequence("Ctrl+A"));

        QVERIFY(actions.conflicts().isEmpty());
        actions.add("edit_select_all", "Select all");
        actions.setKeys("edit_select_all", QKeySequence("Ctrl+A"));
        QCOMPARE(actions.conflicts().size(), 3);
    }

    void windowScopeKeepsShortcut()
    {
        ActionRegistry actions;
        QAction* pen = actions.add("spr_tool_pen", "Pencil", "window:imgedit", true);
        actions.setKeys("spr_tool_pen", QKeySequence("Alt+6"));
        QCOMPARE(pen->shortcut(), QKeySequence("Alt+6"));
        QCOMPARE(pen->shortcutContext(), Qt::WindowShortcut);
        QVERIFY(pen->isCheckable());
    }

    void addIsIdempotent()
    {
        ActionRegistry actions;
        QAction* a = actions.add("file_new", "New");
        QAction* b = actions.add("file_new", "Other text");
        QCOMPARE(a, b);
        QCOMPARE(actions.ids(), QStringList{"file_new"});
        QCOMPARE(a->objectName(), QStringLiteral("file_new"));
    }

    void buildsPanelFromPreset()
    {
        ActionRegistry actions;
        actions.add("spr_add", "Add", "editor:sprites");
        actions.add("spr_delete", "Delete", "editor:sprites");

        ControlRegistry controls(&actions);
        controls.registerFactory(
            "spr_sel", [](const Preset::Control&, QWidget* parent) { return new QSlider(Qt::Vertical, parent); });
        controls.registerFactory("spr_sel_lb", [](const Preset::Control&, QWidget* parent) {
            return new QLabel(QStringLiteral("0 / 0"), parent);
        });

        Preset::Model model;
        QVERIFY(Preset::PresetEngine::runString(QStringLiteral(R"(
            function apply()
                local g = OFS.gridLayout()
                g.spacing = 3
                g:addControl("spr_add", 0, 0):icon(20):flat()
                g:addControl("spr_delete", 0, 1)
                g:addControl("spr_sel", 1, 0, 1, -1):horizontal()
                g:addControl("spr_sel_lb", 2, 0, 1, 2):align(OFS.Align.Right)
                g:addControl("does_not_exist", 3, 0)
                g:addVerticalSpacer(4, 0)
                OFS.panel("sprites").layout = g
                local tb = OFS.toolBar("main")
                tb:addButton("spr_add")
                tb:addSeparator()
                tb:addButton("missing_action")
            end
        )"),
                                                QStringLiteral("test"), model));

        QWidget panel;
        PresetBuilder builder(controls, actions);
        builder.buildPanel(*model.panels["sprites"].layout, &panel, 16);

        auto* grid = qobject_cast<QGridLayout*>(panel.layout());
        QVERIFY(grid);
        QCOMPARE(grid->spacing(), 3);

        auto* add = panel.findChild<QToolButton*>("spr_add");
        QVERIFY(add);
        QCOMPARE(add->defaultAction(), actions.action("spr_add"));
        QCOMPARE(add->iconSize(), QSize(20, 20));
        QVERIFY(add->autoRaise());
        QCOMPARE(panel.findChild<QToolButton*>("spr_delete")->iconSize(), QSize(16, 16));

        auto* slider = panel.findChild<QSlider*>("spr_sel");
        QVERIFY(slider);
        QCOMPARE(slider->orientation(), Qt::Horizontal);
        int row, col, rowSpan, colSpan;
        grid->getItemPosition(grid->indexOf(slider), &row, &col, &rowSpan, &colSpan);
        QCOMPARE(row, 1);
        QCOMPARE(colSpan, 2);

        auto* label = panel.findChild<QLabel*>("spr_sel_lb");
        QVERIFY(label);
        QCOMPARE(label->alignment(), Qt::AlignRight);

        auto* placeholder = panel.findChild<QToolButton*>("does_not_exist");
        QVERIFY(placeholder);
        QVERIFY(!placeholder->isEnabled());
        QVERIFY(builder.warnings().contains(QStringLiteral("unknown control 'does_not_exist'")));

        QToolBar bar;
        builder.buildToolBar(model.toolBars["main"], &bar);
        QCOMPARE(bar.actions().size(), 2);
        QCOMPARE(bar.actions().first(), actions.action("spr_add"));
        QVERIFY(bar.actions().last()->isSeparator());
        QVERIFY(builder.warnings().last().contains(QStringLiteral("missing_action")));
    }

    void menuTextShowsShortcutInParentheses()
    {
        ActionRegistry actions;
        ActionRegistry::Spec spec;
        spec.id = "file_open";
        spec.text = "Open...";
        spec.description = "Open a project";
        QAction* open = actions.add(spec);
        QCOMPARE(open->text(), QStringLiteral("Open..."));
        QCOMPARE(open->statusTip(), QStringLiteral("Open a project"));

        actions.setKeys("file_open", QKeySequence("Ctrl+O"));
        QCOMPARE(open->text(), "Open...\t (" + keyText("Ctrl+O") + ')');
        // Buttons show the description without the shortcut, and so does the status bar.
        QCOMPARE(open->toolTip(), QStringLiteral("Open a project"));
        QCOMPARE(open->statusTip(), QStringLiteral("Open a project"));
        QCOMPARE(actions.caption("file_open"), QStringLiteral("Open..."));
        QCOMPARE(actions.description("file_open"), QStringLiteral("Open a project"));

        QAction* add = actions.add("spr_add", "Add", "editor:sprites");
        actions.setKeys("spr_add", QKeySequence("Ctrl+A"));
        QVERIFY(add->shortcut().isEmpty());
        QCOMPARE(add->text(), "Add\t (" + keyText("Ctrl+A") + ')');
        // Without a description they show the caption.
        QCOMPARE(add->toolTip(), QStringLiteral("Add"));
        QCOMPARE(add->statusTip(), QStringLiteral("Add"));
        QAction* save = actions.add("file_saveas", "Save &as...");
        QCOMPARE(save->toolTip(), QStringLiteral("Save as"));
    }

    void panelControlTips()
    {
        const ControlDef check{"c", ControlKind::Check, "OnionSkin", 0, 0, false, nullptr, "Enable OnionSkin"};
        std::unique_ptr<QWidget> box(createControlWidget(check, nullptr));
        QCOMPARE(box->toolTip(), QStringLiteral("OnionSkin"));
        QCOMPARE(box->statusTip(), QStringLiteral("Enable OnionSkin"));
        const ControlDef plainCheck{"p", ControlKind::Check, "Masked", 0, 0, false};
        std::unique_ptr<QWidget> plain(createControlWidget(plainCheck, nullptr));
        QCOMPARE(plain->toolTip(), QStringLiteral("Masked"));
        QCOMPARE(plain->statusTip(), QStringLiteral("Masked"));
        const ControlDef buttonDef{
            "b", ControlKind::Button, "Start emulator", 0, 0, false, "Start debugger on built-in emulator"};
        std::unique_ptr<QWidget> button(createControlWidget(buttonDef, nullptr));
        QCOMPARE(button->toolTip(), QStringLiteral("Start debugger on built-in emulator"));
        QCOMPARE(button->statusTip(), QStringLiteral("Start debugger on built-in emulator"));
        const ControlDef sliderDef{"s", ControlKind::Slider, "", 0, 100, false, "Volume"};
        std::unique_ptr<QWidget> slider(createControlWidget(sliderDef, nullptr));
        QCOMPARE(slider->toolTip(), QStringLiteral("Volume"));
        QVERIFY(slider->statusTip().isEmpty());

        // Tree items show their text as the tooltip.
        const ControlDef treeDef{"t", ControlKind::Tree, "", 0, 0, false};
        std::unique_ptr<QWidget> widget(createControlWidget(treeDef, nullptr));
        auto* tree = qobject_cast<QTreeView*>(widget.get());
        QVERIFY(tree);
        QStandardItemModel model;
        model.appendRow(new QStandardItem(QStringLiteral("810 - Kung Fu Throw - Throwing the opponent")));
        tree->setModel(&model);
        tree->resize(120, 80);
        tree->show();
        QVERIFY(QTest::qWaitForWindowExposed(tree));
        const QPoint at = tree->visualRect(model.index(0, 0)).center();
        QHelpEvent help(QEvent::ToolTip, at, tree->viewport()->mapToGlobal(at));
        QApplication::sendEvent(tree->viewport(), &help);
        QTRY_COMPARE(QToolTip::text(), QStringLiteral("810 - Kung Fu Throw - Throwing the opponent"));
        QToolTip::hideText();
    }

    void rebuildingReplacesContent()
    {
        ActionRegistry actions;
        actions.add("a", "A");
        ControlRegistry controls(&actions);
        PresetBuilder builder(controls, actions);
        Preset::Layout layout;
        layout.type = Preset::Layout::Type::Vertical;
        Preset::LayoutItem item;
        item.control = std::make_shared<Preset::Control>();
        item.control->name = "a";
        layout.items << item;

        QWidget panel;
        builder.buildPanel(layout, &panel);
        builder.buildPanel(layout, &panel);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(panel.findChildren<QToolButton*>().size(), 1);
    }
};

QTEST_MAIN(ActionsPresetTest)
#include "ActionsPresetTest.moc"
