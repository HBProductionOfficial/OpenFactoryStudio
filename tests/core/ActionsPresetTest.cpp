#include "core/actions/ActionRegistry.h"
#include "core/preset/ControlRegistry.h"
#include "core/preset/PresetBuilder.h"
#include "core/preset/PresetEngine.h"

#include <QAction>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QTest>
#include <QToolBar>
#include <QToolButton>

using namespace OFS;

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
        QCOMPARE(open->text(), QStringLiteral("Open...\t (Ctrl+O)"));
        QCOMPARE(open->toolTip(), QStringLiteral("Open  (Ctrl+O)"));
        QCOMPARE(actions.caption("file_open"), QStringLiteral("Open..."));
        QCOMPARE(actions.description("file_open"), QStringLiteral("Open a project"));

        QAction* add = actions.add("spr_add", "Add", "editor:sprites");
        actions.setKeys("spr_add", QKeySequence("Ctrl+A"));
        QVERIFY(add->shortcut().isEmpty());
        QCOMPARE(add->text(), QStringLiteral("Add\t (Ctrl+A)"));
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
