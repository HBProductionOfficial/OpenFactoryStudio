#include "core/preset/PresetEngine.h"

#include <QTest>

using namespace OFS::Preset;

class PresetEngineTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void toolbars()
    {
        Model model;
        QVERIFY(PresetEngine::runString(QStringLiteral(R"(
            function apply()
                local tb = FF.toolBar("main")
                tb.iconSize = 32
                tb:addButton("editor_def")
                tb:addSeparator()
                tb:addButton("editor_spr")
            end
        )"),
                                        QStringLiteral("test"), model));
        QVERIFY(model.errors.isEmpty());
        QCOMPARE(model.toolBarOrder, QStringList{"main"});
        QCOMPARE(model.toolBars["main"].iconSize, 32);
        QCOMPARE(model.toolBars["main"].actions, (QStringList{"editor_def", "", "editor_spr"}));
    }

    void gridPanelWithChaining()
    {
        Model model;
        QVERIFY(PresetEngine::runString(QStringLiteral(R"(
            function apply()
                local grid = OFS.gridLayout()
                grid.margin = 4
                grid.spacing = 2
                grid:addControl("spr_add", 0, 0):icon(24):flat()
                grid:addControl("spr_sel", 1, 0, 1, -1):horizontal()
                grid:addControl("spr_sel_lb", 2, 0):align(FF.Align.HCenter, FF.VCenter):size(80, 20)
                grid:addVerticalSpacer(3, 0)
                local row = OFS.horizontalLayout()
                row:addControl("spr_prev")
                row:addSpacing(6)
                row:addControl("spr_next")
                grid:addLayout(row, 4, 0, 1, 2)
                FF.panel("sprites").layout = grid
                FF.extraPanel("palettes").layout = OFS.verticalLayout()
            end
        )"),
                                        QStringLiteral("test"), model));
        QVERIFY2(model.errors.isEmpty(), qPrintable(model.errors.join('\n')));
        QCOMPARE(model.panelOrder, QStringList{"sprites"});
        QCOMPARE(model.extraPanelOrder, QStringList{"palettes"});

        const auto& layout = model.panels["sprites"].layout;
        QVERIFY(layout);
        QCOMPARE(layout->type, Layout::Type::Grid);
        QCOMPARE(layout->margin, 4);
        QCOMPARE(layout->spacing, 2);
        QCOMPARE(layout->items.size(), 5);

        const LayoutItem& add = layout->items[0];
        QCOMPARE(add.control->name, QStringLiteral("spr_add"));
        QCOMPARE(add.control->iconSize, 24);
        QVERIFY(add.control->flat);

        const LayoutItem& slider = layout->items[1];
        QCOMPARE(slider.row, 1);
        QCOMPARE(slider.columnSpan, -1);
        QCOMPARE(slider.control->orientation, std::optional<Qt::Orientation>(Qt::Horizontal));

        const LayoutItem& label = layout->items[2];
        QCOMPARE(label.control->alignment, Qt::AlignHCenter | Qt::AlignVCenter);
        QCOMPARE(label.control->size, QSize(80, 20));

        QCOMPARE(layout->items[3].kind, LayoutItem::Kind::VerticalSpacer);
        const LayoutItem& nested = layout->items[4];
        QCOMPARE(nested.kind, LayoutItem::Kind::Layout);
        QCOMPARE(nested.columnSpan, 2);
        QCOMPARE(nested.layout->type, Layout::Type::Horizontal);
        QCOMPARE(nested.layout->items.size(), 3);
        QCOMPARE(nested.layout->items[1].spacing, 6);

        QCOMPARE(model.referencedNames(), (QStringList{"spr_add", "spr_sel", "spr_sel_lb", "spr_prev", "spr_next"}));
    }

    void themeGlobals()
    {
        Model model;
        QVERIFY(PresetEngine::runString(
            QStringLiteral("iconset = \"Flat-Dark\"\nstylesheet = \"Dark\"\ncolorset = \"Monokai\"\n"),
            QStringLiteral("global"), model));
        QCOMPARE(model.iconset, QStringLiteral("Flat-Dark"));
        QCOMPARE(model.stylesheet, QStringLiteral("Dark"));
        QCOMPARE(model.colorset, QStringLiteral("Monokai"));
    }

    void scriptsMergeIntoOneModel()
    {
        Model model;
        QVERIFY(
            PresetEngine::runString(QStringLiteral("function apply() FF.toolBar('start'):addButton('file_new') end"),
                                    QStringLiteral("a"), model));
        QVERIFY(
            PresetEngine::runString(QStringLiteral("function apply() FF.toolBar('main'):addButton('file_open') end"),
                                    QStringLiteral("b"), model));
        QCOMPARE(model.toolBarOrder, (QStringList{"start", "main"}));
    }

    void errorsAreReported()
    {
        Model model;
        QVERIFY(!PresetEngine::runString(QStringLiteral("function apply() error('boom') end"),
                                         QStringLiteral("bad.lua"), model));
        QCOMPARE(model.errors.size(), 1);
        QVERIFY(model.errors[0].contains(QStringLiteral("boom")));

        Model syntax;
        QVERIFY(!PresetEngine::runString(QStringLiteral("this is not lua"), QStringLiteral("syntax.lua"), syntax));
        QCOMPARE(syntax.errors.size(), 1);
    }

    void noFileSystemAccess()
    {
        Model model;
        QVERIFY(!PresetEngine::runString(QStringLiteral("function apply() io.open('x') end"), QStringLiteral("io.lua"),
                                         model));
    }
};

QTEST_GUILESS_MAIN(PresetEngineTest)
#include "PresetEngineTest.moc"
