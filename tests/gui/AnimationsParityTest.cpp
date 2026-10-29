// Tests for the Animations editor.

#include "GuiTestSupport.h"
#include "core/actions/ActionRegistry.h"
#include "core/code/CodeEditor.h"
#include "core/sprites/SpriteDocument.h"
#include "core/sprites/SpriteFile.h"
#include "core/theme/IconSet.h"
#include "core/theme/ThemeManager.h"
#include "core/widgets/GraphicsView.h"
#include "core/widgets/PaletteWidget.h"
#include "core/widgets/TimelineWidget.h"
#include "shell/AdvancedPaletteEditor.h"
#include "shell/AnimationPage.h"
#include "shell/AnimationsEditor.h"
#include "shell/CodePage.h"
#include "shell/FileDialogs.h"
#include "shell/MainWindow.h"
#include "shell/ProjectView.h"
#include "shell/SpritePage.h"
#include "shell/dialogs/OptionsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEnterEvent>
#include <QFileInfo>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QScopeGuard>
#include <QScreen>
#include <QScrollBar>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextDocument>
#include <QToolButton>

using namespace OFS;
using namespace OFS::GuiTest;

namespace {

// 4x4 sprites, axis 2,4: 0,0 (colour 3), 0,1 (4), 5,0 (8) and 6,0 with a transparent left half.
QByteArray spriteFile()
{
    SpriteDocument sprites;
    sprites.palettes() << SpriteDocument::makePalette(1, 1, Formats::grayscalePalette(256));
    const int ids[][2] = {{0, 0}, {0, 1}, {5, 0}, {6, 0}};
    for (const auto& id : ids) {
        SpriteEntry entry;
        entry.header.group = std::uint16_t(id[0]);
        entry.header.number = std::uint16_t(id[1]);
        entry.header.axisX = 2;
        entry.header.axisY = 4;
        std::vector<std::uint8_t> pixels(16, std::uint8_t(id[0] + id[1] + 3));
        if (id[0] == 6) {
            for (int y = 0; y < 4; ++y)
                pixels[std::size_t(y * 4)] = pixels[std::size_t(y * 4 + 1)] = 0;
        }
        entry.pixels = SpriteDocument::makePixels(Formats::Image::indexed(4, 4, pixels));
        entry.palette = sprites.palettes()[0];
        sprites.sprites() << entry;
    }
    const auto bytes = sprites.toSff().write();
    return bytes.ok() ? QByteArray(reinterpret_cast<const char*>(bytes.value().data()), qsizetype(bytes.value().size()))
                      : QByteArray();
}

const char* const StandWalk = "; Stand\n"
                              "[Begin Action 0]\n"
                              "Clsn2Default: 1\n"
                              " Clsn2[0] = -2, -4, 2, 0\n"
                              "0,0, 0,0, 4\n"
                              "0,1, 0,0, 3\n"
                              "\n"
                              "; Walk\n"
                              "[Begin Action 20]\n"
                              "Clsn1: 1\n"
                              " Clsn1[0] = 0, -3, 3, -1\n"
                              "0,1, 0,0, 5\n"
                              "5,0, 0,0, 7\n";

void answerModals(QList<std::function<void(QWidget*)>> steps)
{
    auto* timer = new QTimer(qApp);
    timer->setInterval(5);
    auto last = std::make_shared<QPointer<QWidget>>();
    QObject::connect(timer, &QTimer::timeout, qApp, [timer, steps, last]() mutable {
        QWidget* modal = QApplication::activeModalWidget();
        if (!modal || !modal->isVisible() || modal == last->data())
            return;
        *last = modal;
        const auto step = steps.takeFirst();
        if (steps.isEmpty()) {
            timer->stop();
            timer->deleteLater();
        }
        step(modal);
    });
    timer->start();
}

void drag(GraphicsView* view, const QPointF& from, const QPointF& to, Qt::KeyboardModifiers modifiers = {})
{
    const QPointF a = view->toWidget(from);
    const QPointF b = view->toWidget(to);
    auto send = [view, modifiers](QEvent::Type type, const QPointF& at, Qt::MouseButtons buttons) {
        QMouseEvent event(type, at, view->mapToGlobal(at), Qt::LeftButton, buttons, modifiers);
        QCoreApplication::sendEvent(view, &event);
    };
    send(QEvent::MouseButtonPress, a, Qt::LeftButton);
    for (int i = 1; i <= 4; ++i)
        send(QEvent::MouseMove, a + (b - a) * i / 4.0, Qt::LeftButton);
    send(QEvent::MouseButtonRelease, b, Qt::NoButton);
}

void click(GraphicsView* view, const QPointF& at, Qt::KeyboardModifiers modifiers = {})
{
    drag(view, at, at, modifiers);
}

QString hoverTip(QMainWindow& window, QWidget* widget)
{
    window.statusBar()->clearMessage();
    QEnterEvent enter(QPointF(1, 1), QPointF(1, 1), widget->mapToGlobal(QPointF(1, 1)));
    QCoreApplication::sendEvent(widget, &enter);
    return window.statusBar()->currentMessage();
}

} // namespace

class AnimationsParityTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        m_context = makeContext();
        Config& config = m_context->config();
        config.setValue("confirm_delete", false);
        config.setValue("preset", "Neo");
        config.setValue("show_cross1", true);
        config.setValue("show_cross2", true);
        config.setValue("show_cross3", false);
        config.setValue("show_guides", false);
        config.setValue("switch_frame_spr", true);
        config.setValue("contact_over_cross", false);
        config.setValue("reverse_spr_axis", false);
        config.setValue("pal_fit_space", false);
        config.setValue("onion_effect", 2);
        config.setValue("onion_opacity", "0.74");
        config.setValue("background_type", 0); // plain colour for the pixel checks
        config.setValue("crosshair_type", 0);
        config.setValue("showmsg_anim_clone", "0");
        config.setValue("showmsg_cbox_replicate", "0");
        config.reset("anim_editcode_geometry");
    }

    void cleanupTestCase() { delete m_context; }

    // Also checks that the 1.1 fields are written in a 1.0 project.
    void panelAndMenus()
    {
        QTemporaryDir dir;
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        AnimationPage* page = openAnimations(window, dir, StandWalk);
        QVERIFY(page);
        ProjectView* view = window.currentProjectView();

        struct Button
        {
            const char* id;
            const char* caption;
            const char* keys;
            const char* tip;
        };
        const Button buttons[] = {
            {"anim_add", "Add", "Ctrl+A", "Add a new animation"},
            {"anim_delete", "Delete", "Ctrl+Del", "Remove the current animation"},
            {"anim_duplicate", "Duplicate", "Ctrl+D", "Duplicate the current animation"},
            {"anim_code", "Edit animation code", "Ctrl+Shift+E", "Edit the code of current animation"},
            {"anim_save_gif", "Save as animated GIF", "Ctrl+G", "Save the current animation as an animated GIF..."},
            {"anim_play", "Play", "Ctrl+P", "Play the current animation"},
            {"anim_stop", "Stop", "Ctrl+T", "Stop the animation"},
            {"anim_editcode", "Edit code", "Ctrl+Shift+C", "Edit animation code in code editor"},
            {"anim_frame_add", "Add frame", "Ctrl+Shift+A", "Add a new frame in the animation"},
            {"anim_frame_delete", "Delete frame", "Ctrl+Shift+Del", "Remove the current frame"},
            {"anim_pos_up", "Move 1px up", "", "Move the sprite 1px up"},
            {"anim_pos_left", "Move 1px to the left", "", "Move the sprite 1px to the left"},
            {"anim_pos_right", "Move 1px to the right", "", "Move the sprite 1px to the right"},
            {"anim_pos_down", "Move 1px down", "", "Move the sprite 1px down"},
            {"anim_frameall_offset", "Apply offset to all next frames", "Alt+O", nullptr},
            {"anim_frame_align", "Alignment Tool", "Ctrl+Alt+A", nullptr},
            {"anim_frameall_time", "Apply time to all next frames", "Alt+T", nullptr},
            {"anim_frameall_flip", "Apply flip to all next frames", "Alt+F", nullptr},
            {"anim_frameall_trans", "Apply transparency to all next frames", "Alt+B", nullptr},
            {"anim_frameall_scale", "Apply scale to all next frames", "Alt+S", nullptr},
            {"anim_box_add_attack", "Add Attack box", "Alt+1",
             "Enable/disable the drawing mode for Attack contact box"},
            {"anim_box_add_collision", "Add Collision box", "Alt+2",
             "Enable/disable the drawing mode for Collision contact box"},
            {"anim_box_del", "Delete selected Contact boxes", "", "Remove all selected Contact boxes"},
            {"anim_box_delall", "Delete all Contact boxes", "Alt+Del", "Remove all Contact boxes of current frame"},
            {"anim_box_delall_anim", "Delete all Contact boxes of current animation", "",
             "Remove all Contact boxes of current animation"},
            {"anim_box_prev", "Copy Contact boxes of previous frame", "Ctrl+Alt+P",
             "Copy all Contact boxes of previous frame"},
            {"anim_box_type", "Switch Contact box type", "Ctrl+Alt+T", "Switch type of all selected Contact boxes"},
            {"anim_box_default", "Switch Contact boxes to default", "",
             "Switch all selected Contact boxes between regular and default"},
            {"anim_onion_find", "Find animation", "", nullptr},
        };
        for (const Button& b : buttons) {
            auto* button = view->findChild<QToolButton*>(QLatin1String(b.id));
            QVERIFY2(button, b.id);
            const QString caption = QString::fromLatin1(b.caption);
            const QString tip = b.tip ? QString::fromLatin1(b.tip) : caption;
            QCOMPARE(button->toolTip(), tip);
            QCOMPARE(button->statusTip(), tip);
            QCOMPARE(hoverTip(window, button), tip);
        }
        for (const char* name : {"anim_frame_loop", "anim_frame_intp_blend", "anim_frame_intp_offset",
                                 "anim_frame_intp_scale", "anim_frame_intp_angle"}) {
            auto* box = view->findChild<QCheckBox*>(QLatin1String(name));
            QVERIFY2(box, name);
            QCOMPARE(box->toolTip(), box->text());
            QCOMPARE(hoverTip(window, box), box->text());
        }
        QCOMPARE(view->findChild<QCheckBox*>("anim_frame_loop")->text(), QStringLiteral("Start loop in this frame"));
        QCOMPARE(hoverTip(window, view->findChild<QCheckBox*>("anim_onion_skin")), QStringLiteral("Enable OnionSkin"));

        QMenu* animations = menu(window, "menu_animations");
        QVERIFY(animations);
        Q_EMIT animations->aboutToShow();
        QCOMPARE(menuIds(animations), (QStringList{"anim_new",
                                                   "anim_open",
                                                   "anim_save",
                                                   "anim_saveas_utf8",
                                                   "anim_saveas",
                                                   "|",
                                                   "anim_editcode",
                                                   "anim_code",
                                                   "|",
                                                   "anim_add",
                                                   "anim_delete",
                                                   "anim_duplicate",
                                                   "anim_save_gif",
                                                   "anim_play",
                                                   "anim_stop",
                                                   "|",
                                                   "anim_frame_add",
                                                   "anim_frame_delete",
                                                   "anim_frameall_offset",
                                                   "anim_frameall_time",
                                                   "anim_frameall_trans",
                                                   "anim_frameall_flip",
                                                   "anim_frameall_scale",
                                                   "anim_frame_align",
                                                   "|",
                                                   "anim_box_add_attack",
                                                   "anim_box_add_collision",
                                                   "anim_box_del",
                                                   "anim_box_delall",
                                                   "anim_box_delall_anim",
                                                   "anim_box_type",
                                                   "anim_box_default",
                                                   "anim_box_prev",
                                                   "anim_box_allinst",
                                                   "|",
                                                   "anim_optimize",
                                                   "anim_import_proj"}));
        ActionRegistry& actions = m_context->actions();
        const struct
        {
            const char* id;
            const char* caption;
            const char* keys;
        } items[] = {
            {"anim_new", "New", ""},
            {"anim_open", "Open", ""},
            {"anim_save", "Save", ""},
            {"anim_saveas_utf8", "Save as UTF-8", ""},
            {"anim_saveas", "Save as...", ""},
            {"anim_editcode", "Edit code", "Ctrl+Shift+C"},
            {"anim_code", "Edit animation code", "Ctrl+Shift+E"},
            {"anim_add", "Add", "Ctrl+A"},
            {"anim_delete", "Delete", "Ctrl+Del"},
            {"anim_duplicate", "Duplicate", "Ctrl+D"},
            {"anim_save_gif", "Save as animated GIF...", "Ctrl+G"},
            {"anim_play", "Play", "Ctrl+P"},
            {"anim_stop", "Stop", "Ctrl+T"},
            {"anim_frame_add", "Add frame", "Ctrl+Shift+A"},
            {"anim_frame_delete", "Delete frame", "Ctrl+Shift+Del"},
            {"anim_frameall_offset", "Apply offset to all next frames", "Alt+O"},
            {"anim_frameall_time", "Apply time to all next frames", "Alt+T"},
            {"anim_frameall_trans", "Apply transparency to all next frames", "Alt+B"},
            {"anim_frameall_flip", "Apply flip to all next frames", "Alt+F"},
            {"anim_frameall_scale", "Apply scale to all next frames", "Alt+S"},
            {"anim_frame_align", "Alignment Tool", "Ctrl+Alt+A"},
            {"anim_box_add_attack", "Add Attack box", "Alt+1"},
            {"anim_box_add_collision", "Add Collision box", "Alt+2"},
            {"anim_box_del", "Delete selected Contact boxes", ""},
            {"anim_box_delall", "Delete all Contact boxes", "Alt+Del"},
            {"anim_box_delall_anim", "Delete all Contact boxes of current animation", ""},
            {"anim_box_type", "Switch Contact box type", "Ctrl+Alt+T"},
            {"anim_box_default", "Switch Contact boxes to default", ""},
            {"anim_box_prev", "Copy Contact boxes of previous frame", "Ctrl+Alt+P"},
            {"anim_box_allinst", "Use these Contact boxes in all other instances of this frame", "Ctrl+Alt+I"},
            {"anim_optimize", "Optimize", ""},
            {"anim_import_proj", "Import from another project...", "Ctrl+Shift+I"},
        };
        for (const auto& item : items) {
            const QString id = QString::fromLatin1(item.id);
            QCOMPARE(actions.caption(id), QString::fromLatin1(item.caption));
            QCOMPARE(actions.action(id)->shortcut(), QKeySequence(QString::fromLatin1(item.keys)));
            QVERIFY2(actions.action(id)->isEnabled(), item.id);
        }
        QVERIFY(!page->file()->isModified());
        QVERIFY(actions.action("anim_play")->isCheckable() && !actions.action("anim_play")->isChecked());
        QVERIFY(actions.action("anim_stop")->isCheckable() && actions.action("anim_stop")->isChecked());
        QVERIFY(actions.action("anim_editcode")->isCheckable());
        QVERIFY(actions.action("anim_box_add_attack")->isCheckable());
        QCOMPARE(actions.keys("anim_next_anim"), QKeySequence("Ctrl+Right"));
        QCOMPARE(actions.keys("anim_prev_anim"), QKeySequence("Ctrl+Left"));
        QCOMPARE(actions.keys("anim_next_frame"), QKeySequence("Ctrl+Shift+Right"));
        QCOMPARE(actions.keys("anim_prev_frame"), QKeySequence("Ctrl+Shift+Left"));

        QMenu* edit = menu(window, "menu_edit");
        Q_EMIT edit->aboutToShow();
        for (const char* id : {"edit_undo", "edit_redo", "edit_cut", "edit_copy", "edit_paste", "edit_find_replace",
                               "edit_find_replace_multi"}) {
            QVERIFY2(menuIds(edit).contains(QLatin1String(id)), id);
            QVERIFY2(actions.action(QLatin1String(id))->isEnabled(), id);
        }
        QVERIFY(!menu(window, "menu_code")->menuAction()->isEnabled());
        QVERIFY(!menu(window, "menu_encoding")->menuAction()->isEnabled());
        QVERIFY(actions.action("view_secondary_cross")->isEnabled());
        QVERIFY(actions.action("view_secondary_cross")->isChecked());
        QVERIFY(actions.action("view_extra_cross")->isEnabled());
        QVERIFY(!actions.action("view_extra_cross")->isChecked());

        auto* xScale = view->findChild<QDoubleSpinBox*>("anim_frame_xscale");
        auto* angle = view->findChild<QDoubleSpinBox*>("anim_frame_angle");
        auto* blend = view->findChild<QCheckBox*>("anim_frame_intp_blend");
        QVERIFY(xScale && angle && blend);
        for (QWidget* w : std::initializer_list<QWidget*>{xScale, angle, blend,
                                                          view->findChild<QDoubleSpinBox*>("anim_frame_yscale")})
            QVERIFY(w->isEnabled());
        xScale->setValue(2);
        QCOMPARE(line(page, 4), QStringLiteral("0,0, 0,0, 4, , , 2,1"));
        angle->setValue(45);
        QCOMPARE(line(page, 4), QStringLiteral("0,0, 0,0, 4, , , 2,1, 45"));
        blend->setChecked(true);
        QCOMPARE(line(page, 4), QStringLiteral("Interpolate Blend"));
        page->file()->document()->setModified(false);
    }

    // What the canvas and the timeline draw for a frame.
    void drawing()
    {
        QTemporaryDir dir;
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        AnimationPage* page = openAnimations(window, dir,
                                             "; Stand\n[Begin Action 0]\nClsn2: 1\n Clsn2[0] = -8, -12, 8, 0\n"
                                             "0,0, 0,0, 4\n0,1, 0,0, 3\nLoopStart\n5,0, 0,0, 6\n");
        QVERIFY(page);
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        ProjectView* view = window.currentProjectView();
        GraphicsView* canvas = page->view();
        canvas->centerOrigin();
        ActionRegistry& actions = m_context->actions();
        const QRect box(QPoint(-8, -12), QPoint(8, 0));

        QCOMPARE(canvas->zoom(), 3.0);
        QCOMPARE(canvas->images().size(), 1);
        QCOMPARE(canvas->images().first().topLeft, QPoint(-2, -4));
        QVERIFY(canvas->images().first().transform.isIdentity());
        QCOMPARE(canvas->boxes().size(), 1);
        QCOMPARE(canvas->boxes().first().rect, box);
        QCOMPARE(canvas->boxes().first().color, QColor(0, 0, 255));

        view->findChild<QSpinBox*>("anim_frame_xaxis")->setValue(20);
        view->findChild<QSpinBox*>("anim_frame_yaxis")->setValue(10);
        QCOMPARE(line(page, 4), QStringLiteral("0,0, 20,10, 4"));
        QCOMPARE(canvas->images().first().transform.map(QPointF(0, 0)), QPointF(20, 10));
        QCOMPARE(canvas->boxes().first().rect, box);
        QVERIFY(canvas->mainCrossVisible());
        QCOMPARE(canvas->mainCrossPosition(), QPoint(0, 0));
        QVERIFY(canvas->secondaryCrossVisible());
        QCOMPARE(canvas->secondaryCrossPosition(), QPoint(20, 10));
        QVERIFY(!canvas->extraCrossVisible());
        auto pixel = [canvas](double x, double y) {
            return canvas->grab().toImage().pixelColor((canvas->toWidget(QPointF(x, y)) + QPointF(1, 1)).toPoint());
        };
        QCOMPARE(pixel(-5, 0), QColor(Qt::black));
        QCOMPARE(pixel(-5, -1), QColor(Qt::white));
        QCOMPARE(pixel(25, 10), QColor(Qt::white));
        QCOMPARE(pixel(25, 9), QColor(0, 0, 255));
        actions.action("view_extra_cross")->setChecked(true);
        QVERIFY(canvas->extraCrossVisible());
        QCOMPARE(pixel(-5, 0), QColor(Qt::white));
        QCOMPARE(pixel(-5, -1).green(), 160);
        actions.action("view_extra_cross")->setChecked(false);
        actions.action("view_secondary_cross")->setChecked(false);
        QVERIFY(!canvas->secondaryCrossVisible());
        actions.action("view_secondary_cross")->setChecked(true);

        view->findChild<QComboBox*>("anim_frame_flip")->setCurrentIndex(1);
        QCOMPARE(canvas->images().first().transform.map(QPointF(1, 0)), QPointF(19, 10));
        view->findChild<QComboBox*>("anim_frame_flip")->setCurrentIndex(0);
        view->findChild<QDoubleSpinBox*>("anim_frame_xscale")->setValue(2);
        view->findChild<QDoubleSpinBox*>("anim_frame_yscale")->setValue(0.5);
        view->findChild<QDoubleSpinBox*>("anim_frame_angle")->setValue(90);
        QCOMPARE(line(page, 4), QStringLiteral("0,0, 20,10, 4, , , 2,0.5, 90"));
        const QPointF right = canvas->images().first().transform.map(QPointF(1, 0)); // 2 units, turned counterclockwise
        QVERIFY2(qAbs(right.x() - 20) < 1e-9 && qAbs(right.y() - 8) < 1e-9,
                 qPrintable(QStringLiteral("%1,%2").arg(right.x()).arg(right.y())));
        QCOMPARE(canvas->boxes().first().rect, box);
        page->edit([](QStringList& lines) { lines[4] = QStringLiteral("0,0, 0,0, 4"); });

        auto* onion = view->findChild<QCheckBox*>("anim_onion_skin");
        onion->setChecked(true);
        QCOMPARE(canvas->images().size(), 2);
        const GraphicsView::Image behind = canvas->images().at(0);
        const GraphicsView::Image sprite = canvas->images().at(1);
        QVERIFY(!behind.draggable && sprite.draggable);
        QCOMPARE(behind.opacity, 0.74);
        QVERIFY(behind.image.pixelColor(1, 1).red() > sprite.image.pixelColor(1, 1).red() + 100);
        onion->setChecked(false);

        auto rowColours = [canvas](int y) {
            const QImage image = canvas->grab().toImage();
            QSet<QRgb> colours;
            const int row = int(canvas->toWidget(QPointF(0, y)).y()) + 1;
            for (int x = int(canvas->toWidget(QPointF(-6, 0)).x()); x < int(canvas->toWidget(QPointF(-3, 0)).x()); ++x)
                colours.insert(image.pixel(x, row));
            return colours;
        };
        QCOMPARE(rowColours(-10).size(), 1);
        page->setSelectedBox({0, 0});
        QVERIFY(canvas->boxes().first().selected);
        QVERIFY(rowColours(-10).size() > 1);
        const QImage selected = canvas->grab().toImage();
        const QRectF r(canvas->toWidget(QPointF(-8, -12)), canvas->toWidget(QPointF(8, 0)));
        const QPointF handles[] = {
            r.topLeft(),     QPointF(r.center().x(), r.top()),    r.topRight(),   QPointF(r.right(), r.center().y()),
            r.bottomRight(), QPointF(r.center().x(), r.bottom()), r.bottomLeft(), QPointF(r.left(), r.center().y())};
        for (const QPointF& at : handles)
            QCOMPARE(selected.pixelColor(at.toPoint()), QColor(Qt::white));
        page->setSelectedBox({});

        TimelineWidget* timeline = page->timeline();
        auto blockColour = [timeline](int frame) {
            const QImage image = timeline->grab().toImage();
            int first = -1;
            int last = -1;
            for (int x = 0; x < timeline->width(); ++x) {
                if (timeline->frameAt(x) == frame) {
                    if (first < 0)
                        first = x;
                    last = x;
                }
            }
            return image.pixelColor((first + last) / 2, 39);
        };
        QCOMPARE(blockColour(0), QColor(120, 100, 30));
        QCOMPARE(blockColour(1), QColor(150, 128, 40));
        QCOMPARE(blockColour(2), QColor(70, 150, 70));
        QCOMPARE(timeline->playhead(), 0);
        page->setCurrentFrame(2);
        QCOMPARE(timeline->playhead(), 7);
        page->file()->document()->setModified(false);
    }

    // Dragging an opaque pixel moves the offset and a transparent one pans.
    void canvasInteraction()
    {
        QTemporaryDir dir;
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        AnimationPage* page = openAnimations(window, dir,
                                             "[Begin Action 0]\nClsn2: 2\n Clsn2[0] = -20, -30, -12, -20\n"
                                             " Clsn2[1] = 12, -30, 20, -20\n6,0, 0,0, 4\n0,0, 0,0, 4\n");
        QVERIFY(page);
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        GraphicsView* canvas = page->view();
        canvas->centerOrigin();
        QTextDocument* text = page->file()->document();
        ActionRegistry& actions = m_context->actions();

        drag(canvas, QPointF(0.5, -2.5), QPointF(10.5, 2.5));
        QCOMPARE(line(page, 4), QStringLiteral("6,0, 10,5, 4"));
        actions.action("edit_undo")->trigger();
        QCOMPARE(line(page, 4), QStringLiteral("6,0, 0,0, 4"));
        actions.action("edit_redo")->trigger();
        QCOMPARE(line(page, 4), QStringLiteral("6,0, 10,5, 4"));
        actions.action("edit_undo")->trigger();
        const QPoint origin = canvas->origin();
        drag(canvas, QPointF(-1.5, -2.5), QPointF(8.5, 2.5));
        QCOMPARE(line(page, 4), QStringLiteral("6,0, 0,0, 4"));
        QCOMPARE(canvas->origin(), origin + QPoint(30, 15));
        canvas->centerOrigin();

        actions.action("anim_pos_right")->trigger();
        actions.action("anim_pos_right")->trigger();
        QCOMPARE(line(page, 4), QStringLiteral("6,0, 2,0, 4"));
        actions.action("edit_undo")->trigger();
        QCOMPARE(line(page, 4), QStringLiteral("6,0, 1,0, 4"));
        actions.action("edit_redo")->trigger();
        actions.action("anim_pos_left")->trigger();
        actions.action("anim_pos_up")->trigger();
        QCOMPARE(line(page, 4), QStringLiteral("6,0, 1,-1, 4"));
        actions.action("anim_pos_down")->trigger();
        actions.action("anim_pos_down")->trigger();
        QCOMPARE(line(page, 4), QStringLiteral("6,0, 1,1, 4"));
        QCOMPARE(canvas->images().first().transform.map(QPointF(0, 0)), QPointF(1, 1));
        actions.action("edit_undo")->trigger();
        QCOMPARE(line(page, 4), QStringLiteral("6,0, 1,0, 4"));
        page->edit([](QStringList& lines) { lines[4] = QStringLiteral("6,0, 0,0, 4"); });

        const QPointF first(-16, -25);
        const QPointF second(16, -25);
        click(canvas, first);
        QCOMPARE(page->selectedBox(), (AirEdits::BoxRef{0, 0}));
        click(canvas, second, Qt::ShiftModifier);
        QCOMPARE(page->selectedBox(), (AirEdits::BoxRef{0, 1}));
        click(canvas, first, Qt::ControlModifier);
        QCOMPARE(page->selectedBox(), (AirEdits::BoxRef{0, 1}));
        int shown = 0;
        for (const GraphicsView::Box& b : canvas->boxes())
            shown += b.selected ? 1 : 0;
        QCOMPARE(shown, 1);
        QCOMPARE(line(page, 3), QStringLiteral(" Clsn2[1] = 12, -30, 20, -20"));
        actions.action("anim_editcode")->trigger();
        actions.action("anim_editcode")->trigger();
        QVERIFY(!page->selectedBox().isValid());
        text->setModified(false);
    }

    void onionSkin()
    {
        QTemporaryDir dir;
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        AnimationPage* page = openAnimations(window, dir,
                                             "; Stand\n[Begin Action 0]\n0,0, 0,0, 4\n0,1, 5,0, 3\n\n"
                                             "; Walk\n[Begin Action 20]\n5,0, 0,0, 5\n\n"
                                             "; Run\n[Begin Action 210]\n0,1, 0,0, 2\n5,0, 0,0, 2\n0,0, 0,0, 2\n");
        QVERIFY(page);
        ProjectView* view = window.currentProjectView();
        auto* onion = view->findChild<QCheckBox*>("anim_onion_skin");
        auto* animation = view->findChild<QScrollBar*>("anim_onion_sel");
        auto* frame = view->findChild<QScrollBar*>("anim_onion_framesel");
        auto* label = view->findChild<QLabel*>("anim_onion_lb");
        QVERIFY(onion && animation && frame && label);
        QVERIFY(!onion->isChecked());
        QCOMPARE(label->text(), QString());
        QCOMPARE(frame->maximum(), 1);

        page->setCurrent(2);
        onion->setChecked(true);
        QCOMPARE(animation->value(), 0);
        QCOMPARE(frame->value(), 0);
        QCOMPARE(label->text(), QStringLiteral("Animation: 0, Frame: 0,0"));
        QCOMPARE(page->view()->images().size(), 2);
        frame->setValue(1);
        QCOMPARE(label->text(), QStringLiteral("Animation: 0, Frame: 0,1"));
        QCOMPARE(page->view()->images().first().transform.map(QPointF(0, 0)), QPointF(5, 0));
        animation->setValue(1);
        QCOMPARE(frame->maximum(), 0);
        QCOMPARE(frame->value(), 0);
        QCOMPARE(label->text(), QStringLiteral("Animation: 20, Frame: 5,0"));

        QString title;
        QString prompt = QStringLiteral("unset");
        answerModals({[&title, &prompt](QWidget* w) {
            auto* input = qobject_cast<QInputDialog*>(w);
            QVERIFY(input);
            title = input->windowTitle();
            prompt = input->labelText();
            QCOMPARE(input->textValue(), QString());
            input->setTextValue(QStringLiteral("210"));
            input->accept();
        }});
        m_context->actions().action("anim_onion_find")->trigger();
        QCOMPARE(title, QStringLiteral("Find animation"));
        QCOMPARE(prompt, QString());
        QCOMPARE(animation->value(), 2);
        QCOMPARE(label->text(), QStringLiteral("Animation: 210, Frame: 0,1"));
        QStringList texts;
        answerModals({[](QWidget* w) {
                          auto* input = qobject_cast<QInputDialog*>(w);
                          QVERIFY(input);
                          input->setTextValue(QStringLiteral("999"));
                          input->accept();
                      },
                      [&texts](QWidget* w) {
                          auto* box = qobject_cast<QMessageBox*>(w);
                          QVERIFY(box);
                          texts << box->windowTitle() << box->text();
                          box->accept();
                      }});
        m_context->actions().action("anim_onion_find")->trigger();
        QCOMPARE(texts, (QStringList{boxTitle("Find animation"), "No animation found!"}));
        QCOMPARE(animation->value(), 2);
        onion->setChecked(false);
        QCOMPARE(page->view()->images().size(), 1);
    }

    void duplicateAndImport()
    {
        QTemporaryDir dir;
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        AnimationPage* page = openAnimations(window, dir, StandWalk);
        QVERIFY(page);
        QStringList seen;
        auto inputStep = [&seen](const QString& typed) {
            return [&seen, typed](QWidget* w) {
                auto* input = qobject_cast<QInputDialog*>(w);
                QVERIFY(input);
                seen << input->windowTitle() + "|" + input->labelText() + "|" + input->textValue();
                input->setTextValue(typed);
                input->accept();
            };
        };
        answerModals({[&seen](QWidget* w) {
                          auto* box = qobject_cast<QMessageBox*>(w);
                          QVERIFY(box);
                          seen << box->windowTitle() + "|" + box->text();
                          QCOMPARE(box->icon(), QMessageBox::Question);
                          QCOMPARE(box->defaultButton(), box->button(QMessageBox::Yes));
                          QVERIFY(box->checkBox());
                          QCOMPARE(box->checkBox()->text(), QStringLiteral("Don't show this again"));
                          box->button(QMessageBox::Yes)->click();
                      },
                      inputStep(QStringLiteral("abc")),
                      [&seen](QWidget* w) {
                          auto* box = qobject_cast<QMessageBox*>(w);
                          QVERIFY(box);
                          seen << box->windowTitle() + "|" + box->text();
                          box->accept();
                      },
                      inputStep(QStringLiteral("50"))});
        m_context->actions().action("anim_duplicate")->trigger();
        QCOMPARE(seen, (QStringList{boxTitle("Duplicate") + "|Do you want to define a new identifier?",
                                    "Duplicate animation|Identifier:|",
                                    boxTitle("Duplicate animation") + "|The identifier must be a number.",
                                    "Duplicate animation|Identifier:|"}));
        QCOMPARE(page->document().count(), 3);
        QCOMPARE(page->current(), 1);
        QCOMPARE(page->animation()->number, 50);
        QCOMPARE(page->animation()->name, QStringLiteral("Stand"));
        QVERIFY(!page->file()->document()->toPlainText().contains("abc"));
        answerBoxes({QMessageBox::No}, nullptr);
        m_context->actions().action("anim_duplicate")->trigger();
        QCOMPARE(page->document().count(), 4);
        QCOMPARE(page->animation()->number, 50);

        QStringList texts;
        answerBoxes({QMessageBox::Ok}, &texts);
        m_context->actions().action("anim_import_proj")->trigger();
        QCOMPARE(texts, QStringList{"You can't import from the destination project!"});
        page->file()->document()->setModified(false);
    }

    // Apply scale and the Alignment Tool.
    void frameCommands()
    {
        QTemporaryDir dir;
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        AnimationPage* page = openAnimations(window, dir,
                                             "; A\n[Begin Action 0]\nClsn2: 1\n Clsn2[0] = -2, -4, 2, 0\n"       // 0-3
                                             "0,0, 0,0, 4, , , 2,0.5, 45\n0,1, 1,1, 3, , , 1,1, 10\n"            // 4-5
                                             "\n; B\n[Begin Action 20]\n5,0, 0,0, 5\n\n; C\n[Begin Action 30]\n" // 6-12
                                             "0,0, 0,0, 2\n");                                                   // 13
        QVERIFY(page);
        ActionRegistry& actions = m_context->actions();
        actions.action("anim_frameall_scale")->trigger();
        QCOMPARE(line(page, 5), QStringLiteral("0,1, 1,1, 3, , , 2,0.5, 10"));
        actions.action("edit_undo")->trigger();

        page->setCurrent(1);
        QTextDocument* text = page->file()->document();
        QStringList captions;
        QString defaults;
        bool modal = false;
        bool openAfterApply = false;
        QStringList afterOffset;
        whenModal<QDialog>([&](QDialog* dialog) {
            const auto close = qScopeGuard([dialog] { dialog->findChild<QPushButton*>("pushButton_2")->click(); });
            modal = dialog->isModal();
            QCOMPARE(dialog->objectName(), QStringLiteral("AnimAlign"));
            QCOMPARE(dialog->windowTitle(), QStringLiteral("Alignment Tool"));
            auto* mode = dialog->findChild<QComboBox*>("mode");
            auto* x = dialog->findChild<QSpinBox*>("xVal");
            auto* y = dialog->findChild<QSpinBox*>("yVal");
            auto* list = dialog->findChild<QListWidget*>("list");
            QVERIFY(mode && x && y && list);
            QVERIFY(list->selectionMode() != QAbstractItemView::SingleSelection);
            for (int i = 0; i < mode->count(); ++i)
                captions << mode->itemText(i);
            defaults = QStringLiteral("%1 %2 %3 %4 %5")
                           .arg(x->value())
                           .arg(y->value())
                           .arg(x->minimum())
                           .arg(x->maximum())
                           .arg(y->minimum());
            for (int i = 0; i < list->count(); ++i)
                captions << list->item(i)->text() + (list->item(i)->isSelected() ? "*" : "");
            for (const char* name : {"pushButton", "pushButton_2", "pushButton_4", "pushButton_3", "pushButton_5"})
                captions << dialog->findChild<QPushButton*>(QLatin1String(name))->text();
            QVERIFY(dialog->findChild<QPushButton*>("pushButton")->isDefault());
            auto selected = [list] {
                QList<int> rows;
                for (int i = 0; i < list->count(); ++i) {
                    if (list->item(i)->isSelected())
                        rows << i;
                }
                return rows;
            };
            dialog->findChild<QPushButton*>("pushButton_3")->click();
            QCOMPARE(selected(), (QList<int>{0, 2}));
            dialog->findChild<QPushButton*>("pushButton_4")->click();
            QCOMPARE(selected(), QList<int>{});
            dialog->findChild<QPushButton*>("pushButton_5")->click();
            QCOMPARE(selected(), (QList<int>{0, 1, 2}));
            list->item(2)->setSelected(false);
            x->setValue(3);
            y->setValue(-2);
            dialog->findChild<QPushButton*>("pushButton")->click();
            openAfterApply = dialog->isVisible();
            afterOffset << text->findBlockByNumber(4).text() << text->findBlockByNumber(5).text()
                        << text->findBlockByNumber(9).text() << text->findBlockByNumber(13).text();
            mode->setCurrentIndex(1); // Offset Contact boxes
            x->setValue(1);
            y->setValue(1);
            dialog->findChild<QPushButton*>("pushButton")->click();
        });
        actions.action("anim_frame_align")->trigger();
        QVERIFY(modal);
        QCOMPARE(captions,
                 (QStringList{"Offset axis", "Offset Contact boxes", "Resize Contact boxes (top-left)",
                              "Resize Contact boxes (bottom-right)", "Scale Contact boxes", "0 - A", "20 - B*",
                              "30 - C", "Apply", "Close", "Clear selection", "Invert selection", "Select all"}));
        QCOMPARE(defaults, QStringLiteral("0 0 -30000 30000 -30000"));
        QVERIFY(openAfterApply);
        QCOMPARE(afterOffset, (QStringList{"0,0, 3,-2, 4, , , 2,0.5, 45", "0,1, 4,-1, 3, , , 1,1, 10", "5,0, 3,-2, 5",
                                           "0,0, 0,0, 2"}));
        QCOMPARE(line(page, 3), QStringLiteral("  Clsn2[0] = -1, -3, 3, 1"));
        actions.action("edit_undo")->trigger();
        QCOMPARE(line(page, 3), QStringLiteral(" Clsn2[0] = -2, -4, 2, 0"));
        QCOMPARE(line(page, 4), QStringLiteral("0,0, 3,-2, 4, , , 2,0.5, 45"));
        QCOMPARE(line(page, 9), QStringLiteral("5,0, 3,-2, 5"));
        actions.action("edit_undo")->trigger();
        QCOMPARE(line(page, 4), QStringLiteral("0,0, 0,0, 4, , , 2,0.5, 45"));
        QCOMPARE(line(page, 9), QStringLiteral("5,0, 0,0, 5"));

        page->setCurrent(2);
        QString again;
        whenModal<QDialog>([&again](QDialog* dialog) {
            auto* list = dialog->findChild<QListWidget*>("list");
            again = QStringLiteral("%1 %2 %3")
                        .arg(dialog->findChild<QSpinBox*>("xVal")->value())
                        .arg(dialog->findChild<QSpinBox*>("yVal")->value())
                        .arg(list->selectedItems().size() == 1 ? list->selectedItems().first()->text() : QString());
            dialog->findChild<QPushButton*>("pushButton_2")->click();
        });
        actions.action("anim_frame_align")->trigger();
        QCOMPARE(again, QStringLiteral("0 0 30 - C"));
        text->setModified(false);
    }

    void textModeAndUndo()
    {
        QTemporaryDir dir;
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        AnimationPage* page = openAnimations(window, dir, StandWalk);
        QVERIFY(page);
        ProjectView* view = window.currentProjectView();
        QTabWidget* tabs = view->fileTabs(QStringLiteral("animations"));
        QVERIFY(tabs);
        ActionRegistry& actions = m_context->actions();
        auto tab = [tabs] {
            return tabs->tabText(tabs->currentIndex());
        };
        QCOMPARE(tab(), QStringLiteral("a.air"));

        page->setCurrent(0);
        page->setCurrentFrame(1);
        page->setCurrent(1);
        page->setCurrentFrame(1);
        view->findChild<QSpinBox*>("anim_frame_time")->setValue(9);
        QCOMPARE(line(page, 12), QStringLiteral("5,0, 0,0, 9"));
        QCOMPARE(tab(), QStringLiteral("a.air*"));
        page->view()->setFocus();
        actions.action("edit_undo")->trigger();
        QCOMPARE(line(page, 12), QStringLiteral("5,0, 0,0, 7"));
        QCOMPARE(page->current(), 1);
        QCOMPARE(page->currentFrame(), 1);
        QCOMPARE(tab(), QStringLiteral("a.air"));
        actions.action("edit_redo")->trigger();
        QCOMPARE(page->currentFrame(), 1);
        actions.action("edit_undo")->trigger();

        actions.action("anim_editcode")->trigger();
        CodeEditor* editor = page->codePage()->editor();
        QCOMPARE(editor->textCursor().blockNumber(), 12);
        QCOMPARE(editor->textCursor().positionInBlock(), 0);
        auto moveTo = [editor](int n) {
            editor->setTextCursor(QTextCursor(editor->document()->findBlockByNumber(n)));
        };
        moveTo(1);
        QCOMPARE(page->current(), 0);
        QCOMPARE(page->currentFrame(), 1);
        for (int n : {10, 9, 7, 6, 3}) { // box, comment and blank lines
            moveTo(n);
            QCOMPARE(page->current(), 0);
            QCOMPARE(page->currentFrame(), 1);
        }
        moveTo(12);
        QCOMPARE(page->current(), 1);
        QCOMPARE(page->currentFrame(), 1);
        editor->moveCursor(QTextCursor::EndOfLine);
        QTest::keyClicks(editor, "0");
        QCOMPARE(line(page, 12), QStringLiteral("5,0, 0,0, 70"));
        QCOMPARE(page->current(), 1);
        QCOMPARE(page->currentFrame(), 1);
        QCOMPARE(view->findChild<QSpinBox*>("anim_frame_time")->value(), 70);
        QCOMPARE(view->findChild<QLabel*>("anim_frame_sel_lb")->text(), QStringLiteral("2 / 2 - 75 ticks"));
        actions.action("anim_editcode")->trigger();
        QVERIFY(!page->textMode());
        page->file()->document()->setModified(false);
    }

    void codeWindow()
    {
        QTemporaryDir dir;
        m_context->config().setValue("anim_editcode_geometry", Config::encodeRect(QRect(5000, 5000, 300, 400)));
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        AnimationPage* page = openAnimations(window, dir, StandWalk);
        QVERIFY(page);
        ActionRegistry& actions = m_context->actions();
        actions.action("anim_code")->trigger();
        auto* code = window.findChild<QDialog*>("AnimEditTxt");
        QVERIFY(code);
        QVERIFY(code->isVisible());
        QVERIFY(!code->isModal());
        QVERIFY(!QApplication::activeModalWidget());
        QCOMPARE(code->windowTitle(), QStringLiteral("Edit animation code"));
        QVERIFY(code->screen()->availableGeometry().contains(code->frameGeometry()));
        auto* editor = code->findChild<CodeEditor*>();
        QVERIFY(editor);
        QCOMPARE(editor->toPlainText(), QStringLiteral("; Stand\n[Begin Action 0]\nClsn2Default: 1\n"
                                                       " Clsn2[0] = -2, -4, 2, 0\n0,0, 0,0, 4\n0,1, 0,0, 3"));
        actions.action("anim_next_frame")->trigger();
        QCOMPARE(page->currentFrame(), 1);

        auto typeLine = [editor](int n, const QString& value) {
            QTextCursor cursor(editor->document()->findBlockByNumber(n));
            cursor.select(QTextCursor::LineUnderCursor);
            cursor.insertText(value);
        };
        typeLine(4, QStringLiteral("0,0, 0,0, 8"));
        code->findChild<QPushButton*>("pushButton_2")->click();
        QCOMPARE(line(page, 4), QStringLiteral("0,0, 0,0, 8"));
        QVERIFY(code->isVisible());

        typeLine(4, QStringLiteral("0,0, 0,0, 9"));
        QStringList texts;
        answerBoxes({QMessageBox::No}, &texts);
        code->findChild<QPushButton*>("pushButton")->click();
        QCOMPARE(texts, QStringList{"Do you want to discard the changes made in the animation code?"});
        QVERIFY(code->isVisible());
        QPointer<QDialog> closing = code;
        answerBoxes({QMessageBox::Yes}, nullptr);
        code->findChild<QPushButton*>("pushButton")->click();
        QTRY_VERIFY(!closing);
        QCOMPARE(line(page, 4), QStringLiteral("0,0, 0,0, 8"));

        m_context->config().setValue("anim_editcode_geometry", Config::encodeRect(QRect(-3000, -3000, 3000, 3000)));
        actions.action("anim_code")->trigger();
        code = window.findChild<QDialog*>("AnimEditTxt");
        QVERIFY(code);
        QVERIFY(code->screen()->availableGeometry().contains(code->frameGeometry()));
        closing = code;
        code->findChild<QPushButton*>("pushButton")->click();
        QTRY_VERIFY(!closing);
        QVERIFY(!QApplication::activeModalWidget());
        m_context->config().reset("anim_editcode_geometry");
        page->file()->document()->setModified(false);
    }

    void playback()
    {
        QTemporaryDir dir;
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        AnimationPage* page = openAnimations(window, dir,
                                             "[Begin Action 0]\n0,0, 0,0, 1\nLoopStart\n0,1, 0,0, 1\n5,0, 0,0, 1\n\n"
                                             "[Begin Action 1]\n0,0, 0,0, 2\n5,0, 0,0, -1\n\n"
                                             "[Begin Action 2]\n0,0, 0,0, 30\nInterpolate Offset\n0,1, 80,0, 30\n");
        QVERIFY(page);
        ActionRegistry& actions = m_context->actions();
        QList<int> frames;
        const auto record =
            connect(page, &AnimationPage::currentChanged, this, [page, &frames] { frames << page->currentFrame(); });
        page->setCurrentFrame(2);
        frames.clear();
        actions.action("anim_play")->trigger();
        // One frame per tick, so a slow debug build needs the long timeout.
        QTRY_VERIFY_WITH_TIMEOUT(frames.size() > 8, 10000);
        actions.action("anim_stop")->trigger();
        disconnect(record);
        QCOMPARE(frames.first(), 0);
        const qsizetype end = frames.indexOf(2);
        QVERIFY(end > 0);
        QVERIFY(!frames.mid(end).contains(0));
        QVERIFY(frames.mid(end).contains(1));

        page->setCurrent(1);
        actions.action("anim_play")->trigger();
        QTRY_COMPARE(page->currentFrame(), 1);
        QTest::qWait(150);
        QCOMPARE(page->currentFrame(), 1);
        QVERIFY(page->isPlaying());
        QVERIFY(actions.action("anim_play")->isChecked());
        actions.action("anim_stop")->trigger();
        QCOMPARE(page->currentFrame(), 1);

        page->setCurrent(2);
        actions.action("anim_play")->trigger();
        double between = -1;
        for (int i = 0; i < 500 && page->currentFrame() == 0 && between < 0; ++i) {
            QTest::qWait(10);
            const double x = page->view()->images().last().transform.dx();
            if (page->currentFrame() == 0 && x > 0 && x < 80)
                between = x;
        }
        actions.action("anim_stop")->trigger();
        QVERIFY2(between > 0, "the sprite did not move toward the next frame's offset");
    }

    void spriteExplorer()
    {
        QTemporaryDir dir;
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        AnimationPage* page = openAnimations(window, dir, "[Begin Action 0]\n0,0, 0,0, 5\n");
        QVERIFY(page);
        ProjectView* view = window.currentProjectView();
        auto* thumbnails = view->findChild<QListView*>("thumb_view");
        QVERIFY(thumbnails);
        QTRY_COMPARE(thumbnails->model()->rowCount(), 2);
        auto select = [thumbnails](QList<int> rows) {
            thumbnails->selectionModel()->clearSelection();
            for (int row : rows)
                thumbnails->selectionModel()->select(thumbnails->model()->index(row, 0), QItemSelectionModel::Select);
        };
        auto button = [view](const char* name) {
            return view->findChild<QToolButton*>(QLatin1String(name));
        };
        QCOMPARE(button("thumb_action2")->text(), QStringLiteral("Add all to the current animation"));
        QCOMPARE(button("thumb_action3")->text(), QStringLiteral("New animation with selected"));
        for (const char* name : {"thumb_action1", "thumb_action2", "thumb_action3", "thumb_action4"}) {
            QVERIFY2(!button(name)->icon().isNull(), name);
            QCOMPARE(button(name)->toolButtonStyle(), Qt::ToolButtonIconOnly);
        }
        QCOMPARE(button("thumb_action3")->icon().pixmap(22).toImage(),
                 m_context->theme().iconSet().icon(QStringLiteral("anim_thumb_new")).pixmap(22).toImage());

        button("thumb_action2")->click();
        QCOMPARE(line(page, 2), QStringLiteral("0,0, 0,0, 10"));
        QCOMPARE(line(page, 3), QStringLiteral("0,1, 0,0, 10"));
        QCOMPARE(page->currentFrame(), 2);

        select({1});
        whenModal<QDialog>([](QDialog* dialog) {
            QCOMPARE(dialog->objectName(), QStringLiteral("AddAnim"));
            dialog->findChild<QLineEdit*>("name")->setText("Sel");
            dialog->findChild<QSpinBox*>("number")->setValue(7);
            dialog->findChild<QSpinBox*>("time")->setValue(-1);
            dialog->accept();
        });
        button("thumb_action3")->click();
        QCOMPARE(page->document().count(), 2);
        QCOMPARE(page->current(), 1);
        QCOMPARE(page->animation()->number, 7);
        QCOMPARE(page->animation()->frames.size(), 1);
        QCOMPARE(page->animation()->frames.first().number, 1);
        QCOMPARE(page->animation()->frames.first().time, -1);
        QVERIFY(thumbnails->selectionModel()->selectedIndexes().isEmpty());
        select({0});
        whenModal<QDialog>([](QDialog* dialog) { dialog->reject(); });
        button("thumb_action3")->click();
        QCOMPARE(page->document().count(), 2);

        select({0, 1});
        QString filter;
        FileDialogs::setSavePromptOverride([&dir, &filter](const QString& caption, const QString&, const QString& f) {
            filter = f;
            return caption == QLatin1String("Save sprites") ? dir.filePath("thumbs.png") : QString();
        });
        m_context->actions().action("thumb_save")->trigger();
        FileDialogs::setSavePromptOverride({});
        QCOMPARE(filter, QStringLiteral("Portable Network Graphic (*.png);;Graphics Interchange Format (*.gif);;"
                                        "Personal Computer eXchange (*.pcx);;Bitmap (*.bmp)"));
        QVERIFY(QFileInfo::exists(dir.filePath("thumbs_0.png")));
        QVERIFY(QFileInfo::exists(dir.filePath("thumbs_1.png")));

        const QString before = page->file()->document()->toPlainText();
        const QPoint at = thumbnails->visualRect(thumbnails->model()->index(1, 0)).center();
        for (QEvent::Type type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease, QEvent::MouseButtonDblClick,
                                  QEvent::MouseButtonRelease}) {
            QMouseEvent event(type, at, thumbnails->viewport()->mapToGlobal(at), Qt::LeftButton,
                              type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton, {});
            QCoreApplication::sendEvent(thumbnails->viewport(), &event);
        }
        QCOMPARE(page->file()->document()->toPlainText(), before);
        QCOMPARE(view->currentEditor(), QStringLiteral("animations"));
        page->file()->document()->setModified(false);
    }

    void switchFrameSprite()
    {
        QTemporaryDir dir;
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        AnimationPage* page = openAnimations(window, dir, StandWalk);
        QVERIFY(page);
        ActionRegistry& actions = m_context->actions();
        QAction* follow = actions.action("spr_frame_spr");
        follow->setChecked(false);
        page->setCurrent(1);
        page->setCurrentFrame(1);
        actions.action("editor_spr")->trigger();
        auto* sprites = window.currentProjectView()->findChild<SpritePage*>();
        QVERIFY(sprites);
        QCOMPARE(sprites->current(), 0);
        follow->setChecked(true);
        QCOMPARE(sprites->current(), 2);
        sprites->setCurrent(1);
        actions.action("editor_anim")->trigger();
        QCOMPARE(page->currentFrame(), 1);
        QVERIFY(follow->isChecked());
    }

    void invertedAxisLeavesAnimations()
    {
        QTemporaryDir dir;
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        AnimationPage* page = openAnimations(window, dir, "[Begin Action 0]\n0,0, 10,5, 4\n");
        QVERIFY(page);
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        ProjectView* view = window.currentProjectView();
        auto* x = view->findChild<QSpinBox*>("anim_frame_xaxis");
        auto* y = view->findChild<QSpinBox*>("anim_frame_yaxis");
        m_context->config().setValue("reverse_spr_axis", true);
        view->reconfigureEditors();
        QCOMPARE(x->value(), 10);
        QCOMPARE(y->value(), 5);
        m_context->actions().action("anim_pos_right")->trigger();
        m_context->actions().action("anim_pos_up")->trigger();
        QCOMPARE(line(page, 1), QStringLiteral("0,0, 11,4, 4"));
        GraphicsView* canvas = page->view();
        canvas->centerOrigin();
        drag(canvas, QPointF(11.5, 2.5), QPointF(21.5, 2.5));
        QCOMPARE(line(page, 1), QStringLiteral("0,0, 21,4, 4"));
        QCOMPARE(x->value(), 21);
        m_context->config().setValue("reverse_spr_axis", false);
        view->reconfigureEditors();
        QCOMPARE(x->value(), 21);
        page->file()->document()->setModified(false);
    }

    // Colors fit available space only changes the Advanced palette editor's grid.
    void graphicsOptions()
    {
        Config& config = m_context->config();
        {
            OptionsDialog dialog(*m_context);
            const struct
            {
                const char* key;
                const char* caption;
            } boxes[] = {{"reverse_spr_axis", "Invert sprite axis"},
                         {"contact_over_cross", "Contact box over axis"},
                         {"pal_fit_space", "Colors fit available space on Advanced Palette Editor"}};
            auto* tab = dialog.findChild<QWidget*>("gfxTab");
            QVERIFY(tab);
            for (const auto& b : boxes) {
                auto* box = tab->findChild<QCheckBox*>(QLatin1String(b.key));
                QVERIFY2(box, b.key);
                QCOMPARE(box->text(), QString::fromLatin1(b.caption));
                QVERIFY(!box->isChecked());
                box->setChecked(true);
            }
            dialog.findChild<QPushButton*>("applyBut")->click();
            for (const auto& b : boxes)
                QVERIFY2(config.boolValue(QLatin1String(b.key), false), b.key);
            for (const auto& b : boxes)
                tab->findChild<QCheckBox*>(QLatin1String(b.key))->setChecked(false);
            dialog.findChild<QPushButton*>("applyBut")->click();
            for (const auto& b : boxes)
                QVERIFY2(!config.boolValue(QLatin1String(b.key), true), b.key);
        }

        QTemporaryDir dir;
        config.setValue("pal_fit_space", true);
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        QVERIFY(openAnimations(window, dir, StandWalk));
        m_context->actions().action("editor_spr")->trigger();
        auto* panelGrid = window.currentProjectView()->findChild<PaletteWidget*>("pal_view");
        QVERIFY(panelGrid);
        panelGrid->resize(900, panelGrid->height());
        QCOMPARE(panelGrid->columns(), 16);
        SpritePage* sprites = window.currentProjectView()->findChild<SpritePage*>();
        AdvancedPaletteEditor editor(*m_context, sprites->file(), 0, 0, &window);
        auto* grid = editor.findChild<PaletteWidget*>("charPal");
        QVERIFY(grid);
        grid->resize(900, grid->height());
        QVERIFY(grid->columns() > 16);
        config.setValue("pal_fit_space", false);
    }

private:
    AnimationPage* openAnimations(MainWindow& window, const QTemporaryDir& dir, const QByteArray& air,
                                  const QByteArray& version = "1.0")
    {
        writeFile(dir.filePath("a.sff"), spriteFile());
        writeFile(dir.filePath("a.air"), air);
        writeFile(dir.filePath("a.def"), "[Info]\nname = \"A\"\nmugenversion = " + version +
                                             "\n[Files]\ncns = a.cns\nsprite = a.sff\nanim = a.air\n");
        writeFile(dir.filePath("a.cns"), "[Data]\nlife = 1000\n");
        if (!window.openProject(dir.filePath("a.def")))
            return nullptr;
        m_context->actions().action("editor_anim")->trigger();
        ProjectView* view = window.currentProjectView();
        return view && view->currentEditor() == QLatin1String("animations") ? view->findChild<AnimationPage*>()
                                                                            : nullptr;
    }

    static QString line(AnimationPage* page, int n) { return page->file()->document()->findBlockByNumber(n).text(); }

    AppContext* m_context = nullptr;
};

QTEST_MAIN(AnimationsParityTest)
#include "AnimationsParityTest.moc"
