// Tests for the Backgrounds editor, plus some message box and save checks.

#include "GuiTestSupport.h"
#include "core/actions/ActionRegistry.h"
#include "core/app/Messages.h"
#include "core/engine/RunConfiguration.h"
#include "core/sounds/SoundDocument.h"
#include "core/sounds/SoundFile.h"
#include "core/sprites/SpriteDocument.h"
#include "core/sprites/SpriteFile.h"
#include "core/widgets/GraphicsView.h"
#include "core/widgets/TimelineWidget.h"
#include "engines/mugen/MugenRun.h"
#include "formats/snd/SndDocument.h"
#include "formats/wav/WavFile.h"
#include "shell/BackgroundsEditor.h"
#include "shell/BackgroundsPage.h"
#include "shell/FileDialogs.h"
#include "shell/MainWindow.h"
#include "shell/ProjectView.h"
#include "shell/SoundPage.h"
#include "shell/SpritePage.h"
#include "shell/dialogs/OptionsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEnterEvent>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QScopeGuard>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QTextEdit>
#include <QToolButton>
#include <QUndoStack>

using namespace OFS;
using namespace OFS::GuiTest;

namespace {

// Sprites 0,0 (8x4), 0,1 (4x4) and 1,0 (20x10), axis 0,0.
QByteArray spriteFile()
{
    SpriteDocument sprites;
    sprites.palettes() << SpriteDocument::makePalette(1, 1, Formats::grayscalePalette(256));
    const int ids[][4] = {{0, 0, 8, 4}, {0, 1, 4, 4}, {1, 0, 20, 10}};
    for (const auto& id : ids) {
        SpriteEntry entry;
        entry.header.group = std::uint16_t(id[0]);
        entry.header.number = std::uint16_t(id[1]);
        entry.pixels = SpriteDocument::makePixels(
            Formats::Image::indexed(id[2], id[3], std::vector<std::uint8_t>(std::size_t(id[2] * id[3]), 5)));
        entry.palette = sprites.palettes()[0];
        sprites.sprites() << entry;
    }
    const auto bytes = sprites.toSff().write();
    return bytes.ok() ? QByteArray(reinterpret_cast<const char*>(bytes.value().data()), qsizetype(bytes.value().size()))
                      : QByteArray();
}

QByteArray soundFile()
{
    Formats::SndDocument snd;
    snd.sounds.push_back(Formats::SndSound{
        0, 0,
        Formats::encodeAudio(Formats::SampleBuffer::fromInt16(std::vector<std::int16_t>(100, 5), 1, 11025)).value()});
    const auto bytes = snd.write();
    return QByteArray(reinterpret_cast<const char*>(bytes.data()), qsizetype(bytes.size()));
}

// A 320x240 stage. Back is 20x10 at -50,20. Front is 4x4 at -45,25 over Back, moving 1 unit per tick.
const char* const StageHead = "[Info]\nname = \"Bg Test\"\nmugenversion = %1\n"
                              "[Camera]\nstartx = 0\nstarty = 0\nboundleft = -100\nboundright = 100\n"
                              "boundhigh = -25\nboundlow = 0\n"
                              "[PlayerInfo]\np1startx = -70\np1starty = 0\np2startx = 70\np2starty = 0\n"
                              "[StageInfo]\nzoffset = 190\nlocalcoord = 320, 240\n"
                              "[BGdef]\nspr = b.sff\n\n";
const char* const BackAndFront = "[BG Back]\ntype = normal\nspriteno = 1, 0\nstart = -50, 20\n\n"
                                 "[BG Front]\ntype = normal\nspriteno = 0, 1\nstart = -45, 25\nvelocity = 1, 0\n";
const char* const Controllers = "[BG Back]\ntype = normal\nspriteno = 1, 0\nid = 7\n\n"
                                "[BGCtrlDef Mover]\nlooptime = 120\nctrlID = 7\n\n"
                                "[BGCtrl Shift]\ntype = PosAdd\ntime = 30\nx = 20\n";

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

void drag(GraphicsView* view, const QPointF& from, const QPointF& to, Qt::KeyboardModifiers modifiers = {},
          Qt::MouseButton button = Qt::LeftButton)
{
    const QPointF a = view->toWidget(from);
    const QPointF b = view->toWidget(to);
    auto send = [view, modifiers, button](QEvent::Type type, const QPointF& at, Qt::MouseButtons buttons) {
        QMouseEvent event(type, at, view->mapToGlobal(at), button, buttons, modifiers);
        QCoreApplication::sendEvent(view, &event);
    };
    send(QEvent::MouseButtonPress, a, button);
    for (int i = 1; i <= 4; ++i)
        send(QEvent::MouseMove, a + (b - a) * i / 4.0, button);
    send(QEvent::MouseButtonRelease, b, Qt::NoButton);
}

void click(GraphicsView* view, const QPointF& at, Qt::KeyboardModifiers modifiers = {},
           Qt::MouseButton button = Qt::LeftButton)
{
    drag(view, at, at, modifiers, button);
}

QString hoverTip(QMainWindow& window, QWidget* widget)
{
    window.statusBar()->clearMessage();
    QEnterEvent enter(QPointF(1, 1), QPointF(1, 1), widget->mapToGlobal(QPointF(1, 1)));
    QCoreApplication::sendEvent(widget, &enter);
    return window.statusBar()->currentMessage();
}

QStringList shownIds(QMenu* menu)
{
    QStringList ids = menuIds(menu);
    for (qsizetype i = menu->actions().size() - 1; i >= 0; --i) {
        if (!menu->actions().at(i)->isVisible())
            ids.removeAt(i);
    }
    return ids;
}

void choose(QComboBox* combo, int index)
{
    combo->setCurrentIndex(index);
    Q_EMIT combo->activated(index);
}

} // namespace

class BackgroundsParityTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        m_context = makeContext();
        Config& config = m_context->config();
        config.setValue("confirm_delete", false);
        config.setValue("confirm_close", false);
        config.setValue("preset", "Neo");
        config.setValue("show_cross1", true);
        config.setValue("show_cross2", true);
        config.setValue("show_rulers", true);
        config.setValue("show_guides", false);
        config.setValue("show_organizer", false);
        config.setValue("bg_zoom_level", 2);
        config.setValue("autosave_interval", 0);
        config.reset("bg_x");
        config.reset("bg_y");
    }

    void cleanupTestCase() { delete m_context; }

    void menusAndToolbar()
    {
        QTemporaryDir dir;
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        BackgroundsPage* page = openStage(window, dir, BackAndFront);
        QVERIFY(page);
        ProjectView* view = window.currentProjectView();
        QMenu* backgrounds = menu(window, "menu_backgrounds");
        QVERIFY(backgrounds);
        Q_EMIT backgrounds->aboutToShow();
        QCOMPARE(
            shownIds(backgrounds).join(QLatin1Char(' ')),
            QStringLiteral("bgs_add bgs_delete bgs_duplicate | bgs_play bgs_pause bgs_stop | <Controllers> | <BG>"));
        QCOMPARE(menuIds(menu(window, "menu_bg_controllers")),
                 (QStringList{"bgctrls_def_add", "bgctrls_def_delete", "|", "bgctrls_add", "bgctrls_delete"}));
        QAction* group = nullptr;
        for (QAction* a : backgrounds->actions()) {
            if (a->text() == QLatin1String("BG"))
                group = a;
        }
        QVERIFY(group && group->isCheckable() && group->isChecked());
        ActionRegistry& actions = m_context->actions();
        for (const char* id : {"bgs_play", "bgs_pause", "bgs_stop"})
            QVERIFY2(actions.action(QLatin1String(id))->isCheckable(), id);
        QVERIFY(actions.action("bgs_stop")->isChecked());
        actions.action("editor_def")->trigger();
        Q_EMIT backgrounds->aboutToShow();
        QCOMPARE(shownIds(backgrounds).join(QLatin1Char(' ')),
                 QStringLiteral("bgs_add bgs_delete bgs_duplicate | bgs_play bgs_pause bgs_stop | <Controllers>"));
        actions.action("editor_bg")->trigger();

        const struct
        {
            const char* id;
            const char* caption;
        } buttons[] = {{"bgs_add", "Add"},
                       {"bgs_delete", "Remove"},
                       {"bgs_duplicate", "Duplicate"},
                       {"bgs_play", "Play"},
                       {"bgs_pause", "Pause"},
                       {"bgs_stop", "Stop"},
                       {"bgctrls_def_add", "Add definition"},
                       {"bgctrls_def_delete", "Remove definition"},
                       {"bgctrls_add", "Add"},
                       {"bgctrls_delete", "Remove"}};
        for (const auto& b : buttons) {
            auto* button = view->findChild<QToolButton*>(QLatin1String(b.id));
            QVERIFY2(button, b.id);
            QCOMPARE(button->toolTip(), QString::fromLatin1(b.caption));
            QCOMPARE(hoverTip(window, button), QString::fromLatin1(b.caption));
        }
        QCOMPARE(hoverTip(window, view->findChild<QCheckBox*>("bgs_mask")), QStringLiteral("Masked"));
        QCOMPARE(hoverTip(window, view->findChild<QCheckBox*>("bgs_pos_link")), QStringLiteral("Link to previous"));

        auto* extra = window.findChild<QToolBar*>("extraToolBar");
        QVERIFY(extra);
        QCOMPARE(toolBarIds(extra), (QStringList{"editor_new",
                                                 "editor_open",
                                                 "editor_save",
                                                 "editor_saveas",
                                                 "|",
                                                 "edit_undo",
                                                 "edit_redo",
                                                 "|",
                                                 "edit_cut",
                                                 "edit_copy",
                                                 "edit_paste",
                                                 "|",
                                                 "edit_find_replace",
                                                 "|",
                                                 "view_organizer",
                                                 "|",
                                                 "view_zoomreset",
                                                 "view_zoomfit",
                                                 "view_zoomout",
                                                 "view_zoomin",
                                                 "view_center_origin",
                                                 "view_guide_lines",
                                                 "view_attack_box",
                                                 "view_collision_box"}));
        const struct
        {
            const char* id;
            const char* keys;
        } keys[] = {{"editor_new", "Ctrl+Shift+N"},   {"editor_open", "Ctrl+Shift+O"}, {"editor_save", "Ctrl+Shift+S"},
                    {"edit_undo", "Ctrl+Z"},          {"edit_redo", "Ctrl+Y"},         {"edit_find_replace", "Ctrl+F"},
                    {"view_organizer", "Ctrl+Alt+O"}, {"view_zoomreset", "Ctrl+0"},    {"view_zoomout", "Ctrl+-"},
                    {"view_zoomin", "Ctrl++"},        {"view_center_origin", "Ctrl+?"}};
        for (const auto& k : keys)
            QCOMPARE(actions.keys(QLatin1String(k.id)), QKeySequence(QString::fromLatin1(k.keys)));
        QVERIFY(!actions.action("view_organizer")->isChecked());
        QVERIFY(!actions.action("view_guide_lines")->isChecked());
        QVERIFY(actions.action("view_attack_box")->isChecked());
        QVERIFY(actions.action("view_collision_box")->isChecked());
        QVERIFY(!page->file()->isModified());
    }

    // The canvas while editing and playing. The crosses mark the players' starts.
    void canvas()
    {
        QTemporaryDir dir;
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        BackgroundsPage* page = openStage(window, dir, BackAndFront);
        QVERIFY(page);
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        ProjectView* view = window.currentProjectView();
        GraphicsView* canvas = page->view();
        ActionRegistry& actions = m_context->actions();

        QVERIFY(canvas->mainCrossVisible());
        QCOMPARE(canvas->mainCrossPosition(), QPoint(-70, 190));
        QVERIFY(canvas->secondaryCrossVisible());
        QCOMPARE(canvas->secondaryCrossPosition(), QPoint(70, 190));
        actions.action("view_main_cross")->setChecked(false);
        QVERIFY(!canvas->mainCrossVisible());
        actions.action("view_secondary_cross")->setChecked(false);
        QVERIFY(!canvas->secondaryCrossVisible());
        actions.action("view_main_cross")->setChecked(true);
        actions.action("view_secondary_cross")->setChecked(true);
        QVERIFY(canvas->mainCrossVisible() && canvas->secondaryCrossVisible());
        actions.action("view_rulers")->setChecked(false);
        QVERIFY(!canvas->rulersVisible());
        actions.action("view_rulers")->setChecked(true);
        QVERIFY(canvas->rulersVisible());
        actions.action("view_guide_lines")->setChecked(true);
        QVERIFY(canvas->guidesVisible());
        QCOMPARE(canvas->guides(), QPoint(0, 0));
        const QImage guides = canvas->grab().toImage();
        QCOMPARE(guides.pixelColor((canvas->toWidget(QPointF(0, 120)) + QPointF(1, 0)).toPoint()), QColor(0, 255, 255));
        actions.action("view_guide_lines")->setChecked(false);
        QVERIFY(!canvas->guidesVisible());
        actions.action("view_extra_cross")->setChecked(true);
        QVERIFY(!canvas->extraCrossVisible());
        actions.action("view_extra_cross")->setChecked(false);
        QVERIFY(!actions.action("view_code_bar")->isEnabled());

        canvas->setZoomIndex(2); // 1x
        canvas->centerOrigin();
        QCOMPARE(canvas->backgroundColor(), QColor(255, 0, 255));
        QCOMPARE(canvas->grab().toImage().pixelColor(canvas->toWidget(QPointF(100.5, 100.5)).toPoint()),
                 QColor(255, 0, 255));
        QCOMPARE(canvas->frames().size(), 1);
        QCOMPARE(canvas->frames().first().rect, QRectF(-160, 0, 320, 240));
        QCOMPARE(canvas->frames().first().color, QColor(255, 0, 0));
        click(canvas, QPointF(100, 100));
        QCOMPARE(page->current(), 0);
        QCOMPARE(canvas->frames().size(), 3);
        const GraphicsView::Frame selection = canvas->frames().at(1);
        QCOMPARE(selection.rect, QRectF(-50, 20, 20, 10));
        QCOMPARE(selection.color, QColor(Qt::white));
        QVERIFY(selection.dashed && selection.handles);
        QCOMPARE(canvas->frames().at(2).rect, QRectF(-160, 0, 320, 240));
        QCOMPARE(canvas->frames().at(2).color, QColor(0, 255, 0));
        auto* windowMode = view->findChild<QComboBox*>("bgs_window_mode");
        choose(windowMode, 1);
        view->findChild<QSpinBox*>("bgs_window_top")->setValue(10);
        view->findChild<QSpinBox*>("bgs_window_left")->setValue(20);
        QVERIFY(has(page, "window = 20, 10, 319, 239"));
        QCOMPARE(canvas->frames().at(2).rect.topLeft(), QPointF(-140, 10));
        choose(windowMode, 0);

        QVERIFY(page->timeline()->tickMode());
        int tickOne = 0; // six pixels per tick
        while (tickOne < page->timeline()->width() && page->timeline()->tickAt(tickOne) != 1)
            ++tickOne;
        QCOMPARE(page->timeline()->tickAt(tickOne + 5), 1);
        QCOMPARE(page->timeline()->tickAt(tickOne + 6), 2);
        page->setTick(37);
        QCOMPARE(page->timeline()->playhead(), 37);
        page->setTick(0);

        canvas->setOrigin(QPoint(10, 10));
        actions.action("view_center_origin")->trigger();
        QCOMPARE(canvas->origin(), canvas->rect().adjusted(20, 20, 0, 0).center());

        actions.action("bgs_play")->trigger();
        QCOMPARE(canvas->backgroundColor(), QColor(0, 0, 0));
        QCOMPARE(canvas->grab().toImage().pixelColor(canvas->toWidget(QPointF(-170.5, 100.5)).toPoint()),
                 QColor(0, 0, 0));
        QCOMPARE(canvas->frames().size(), 1);
        QCOMPARE(canvas->frames().first().color, QColor(0, 255, 0));
        QVERIFY(!canvas->images().isEmpty());
        for (const GraphicsView::Image& image : canvas->images())
            QVERIFY(QRectF(-160, 0, 320, 240).contains(image.clip));
        actions.action("bgs_stop")->trigger();
        QCOMPARE(canvas->backgroundColor(), QColor(255, 0, 255));
        QCOMPARE(canvas->frames().first().color, QColor(255, 0, 0));
        page->file()->document()->setModified(false);

        QTemporaryDir motifDir;
        QVERIFY(openMotif(window, motifDir));
        BackgroundsPage* motif = window.currentProjectView()->findChild<BackgroundsPage*>();
        QVERIFY(motif && motif != page);
        QVERIFY(!motif->view()->mainCrossVisible());
        QVERIFY(!motif->view()->secondaryCrossVisible());
    }

    // bg_zoom_level 2 means two steps above 1x, so 3x. It is not written back.
    void zoomAndTickPerProject()
    {
        QTemporaryDir one;
        QTemporaryDir two;
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        BackgroundsPage* first = openStage(window, one, BackAndFront);
        QVERIFY(first);
        QCOMPARE(first->view()->zoom(), 3.0);
        ActionRegistry& actions = m_context->actions();
        actions.action("view_zoomin")->trigger();
        actions.action("view_zoomin")->trigger();
        QCOMPARE(first->view()->zoom(), 5.0);
        first->setTick(40);
        BackgroundsPage* second = openStage(window, two, BackAndFront);
        QVERIFY(second && second != first);
        QCOMPARE(second->view()->zoom(), 3.0);
        QCOMPARE(second->tick(), 0);
        second->setTick(12);
        auto* tabs = window.findChild<QTabBar*>("projectTabs");
        tabs->setCurrentIndex(0);
        QCOMPARE(window.currentProjectView()->findChild<BackgroundsPage*>(), first);
        QCOMPARE(first->view()->zoom(), 5.0);
        QCOMPARE(first->tick(), 40);
        tabs->setCurrentIndex(1);
        QCOMPARE(second->tick(), 12);
        QCOMPARE(second->view()->zoom(), 3.0);
        QCOMPARE(m_context->config().intValue("bg_zoom_level", -1), 2);
    }

    void mouseAndKeys()
    {
        QTemporaryDir dir;
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        BackgroundsPage* page = openStage(window, dir, BackAndFront);
        QVERIFY(page);
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        GraphicsView* canvas = page->view();
        canvas->centerOrigin();
        ActionRegistry& actions = m_context->actions();
        QCOMPARE(page->current(), 0);
        click(canvas, QPointF(-43.5, 26.5)); // Front lies over Back here
        QCOMPARE(page->current(), 1);
        click(canvas, QPointF(-35.5, 22.5));
        QCOMPARE(page->current(), 0);
        click(canvas, QPointF(120.5, 200.5));
        QCOMPARE(page->current(), 0);

        drag(canvas, QPointF(-35.5, 22.5), QPointF(-5.5, 32.5));
        QVERIFY(has(page, "start = -20, 30"));
        canvas->setFocus();
        actions.action("edit_undo")->trigger();
        QVERIFY(has(page, "start = -50, 20"));
        const QPoint origin = canvas->origin();
        drag(canvas, QPointF(-35.5, 22.5), QPointF(-25.5, 22.5), Qt::ControlModifier);
        QVERIFY(has(page, "start = -50, 20"));
        QCOMPARE(canvas->origin(), origin + QPoint(30, 0));
        canvas->centerOrigin();

        const QString text = page->file()->document()->toPlainText();
        click(canvas, QPointF(-43.5, 26.5), {}, Qt::RightButton);
        for (Qt::Key key : {Qt::Key_Left, Qt::Key_Right, Qt::Key_Up, Qt::Key_Down}) {
            QTest::keyClick(canvas, key);
            QTest::keyClick(canvas, key, Qt::ShiftModifier);
            QTest::keyClick(canvas, key, Qt::AltModifier);
        }
        QCOMPARE(page->file()->document()->toPlainText(), text);
        QCOMPARE(page->current(), 0);
        QCOMPARE(page->camera(), QPointF());

        auto frontLeft = [page] {
            for (const GraphicsView::Image& image : page->view()->images()) {
                if (image.image.width() == 4)
                    return image.topLeft.x();
            }
            return -9999;
        };
        QCOMPARE(frontLeft(), -45);
        TimelineWidget* timeline = page->timeline();
        int x = 0;
        while (x < timeline->width() && timeline->tickAt(x) != 12)
            ++x;
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(x + 1, 30), timeline->mapToGlobal(QPointF(x + 1, 30)),
                          Qt::LeftButton, Qt::LeftButton, {});
        QCoreApplication::sendEvent(timeline, &press);
        QCOMPARE(page->tick(), 12);
        QCOMPARE(frontLeft(), -33);
        page->file()->document()->setModified(false);
    }

    // Organize automatically does nothing for backgrounds.
    void organizerMoves()
    {
        QTemporaryDir dir;
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        BackgroundsPage* page = openStage(window, dir,
                                          "[BG A]\ntype = normal\n\n[BG B]\ntype = normal\n\n[BG C]\ntype = normal\n\n"
                                          "[BG D]\ntype = normal\n");
        QVERIFY(page);
        ActionRegistry& actions = m_context->actions();
        actions.action("view_organizer")->setChecked(true);
        auto* organizer = window.currentProjectView()->findChild<QListView*>("org_list");
        QVERIFY(organizer);
        QTRY_COMPARE(organizer->model()->rowCount(), 4);
        auto order = [page] {
            QString names;
            for (const BgElement& e : page->currentGroup()->elements)
                names += e.section.name;
            return names;
        };
        auto pick = [organizer](int row) {
            organizer->selectionModel()->select(organizer->model()->index(row, 0), QItemSelectionModel::ClearAndSelect);
        };
        pick(0);
        actions.action("org_down")->trigger();
        QCOMPARE(order(), QStringLiteral("BACD"));
        actions.action("org_down_10")->trigger();
        QCOMPARE(order(), QStringLiteral("BCDA"));
        actions.action("org_top")->trigger();
        QCOMPARE(order(), QStringLiteral("ABCD"));
        actions.action("org_bottom")->trigger();
        QCOMPARE(order(), QStringLiteral("BCDA"));
        actions.action("org_up_10")->trigger();
        QCOMPARE(order(), QStringLiteral("ABCD"));
        const QString text = page->file()->document()->toPlainText();
        actions.action("org_auto")->trigger();
        QCOMPARE(page->file()->document()->toPlainText(), text);
        page->view()->setFocus();
        actions.action("edit_undo")->trigger();
        QCOMPARE(order(), QStringLiteral("BCDA"));
        actions.action("view_organizer")->setChecked(false);
        page->file()->document()->setModified(false);
    }

    void panelsPerProject()
    {
        QTemporaryDir one;
        QTemporaryDir two;
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        BackgroundsPage* withControllers = openStage(window, one, Controllers);
        QVERIFY(withControllers);
        BackgroundsPage* without = openStage(window, two, BackAndFront);
        QVERIFY(without);
        auto panel = [&window](const char* name) {
            return window.currentProjectView()->findChild<QWidget*>(QLatin1String(name));
        };
        auto* tabs = window.findChild<QTabBar*>("projectTabs");
        for (int round = 0; round < 2; ++round) {
            tabs->setCurrentIndex(0);
            QCOMPARE(qobject_cast<QLabel*>(panel("bgctrls_def_sel_lb"))->text(), QStringLiteral("1 / 1"));
            QCOMPARE(qobject_cast<QLineEdit*>(panel("bgctrls_name"))->text(), QStringLiteral("Shift"));
            QCOMPARE(qobject_cast<QLabel*>(panel("bgs_sel_lb"))->text(), QStringLiteral("1 / 1"));
            tabs->setCurrentIndex(1);
            QCOMPARE(qobject_cast<QLabel*>(panel("bgctrls_def_sel_lb"))->text(), QStringLiteral("0 / 0"));
            QCOMPARE(qobject_cast<QLineEdit*>(panel("bgctrls_name"))->text(), QString());
            QVERIFY(!panel("bgctrls_name")->isEnabled());
            QCOMPARE(qobject_cast<QLabel*>(panel("bgs_sel_lb"))->text(), QStringLiteral("1 / 2"));
        }
    }

    void elementFields()
    {
        QTemporaryDir dir;
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        BackgroundsPage* page = openStage(window, dir, "[BG Back]\ntype = normal\nspriteno = 1, 0\n");
        QVERIFY(page);
        ProjectView* view = window.currentProjectView();
        auto field = [&view](const char* name) {
            return view->findChild<QWidget*>(QLatin1String(name));
        };
        auto* type = view->findChild<QComboBox*>("bgs_type");
        QVERIFY(type);
        QCOMPARE(type->currentText(), QStringLiteral("Normal"));
        const char* const always[] = {"bgs_start_x",     "bgs_delta_y",    "bgs_velocity_x",   "bgs_id",
                                      "bgs_layer",       "bgs_pos_link",   "bgs_tile_x",       "bgs_tile_spacing_y",
                                      "bgs_trans",       "bgs_transsrc",   "bgs_transdst",     "bgs_window_delta_x",
                                      "bgs_window_mode", "bgs_sin_x_ampl", "bgs_sin_y_period", "bgs_name"};
        // 1.0 has no parallax animation and no scale start or delta.
        const struct
        {
            int type;
            bool sprite, animation, mask, parallax;
        } types[] = {{1, true, false, true, false},
                     {2, false, true, false, false},
                     {3, true, false, true, true},
                     {4, false, false, false, false}};
        for (const auto& t : types) {
            choose(type, t.type);
            QCOMPARE(type->currentIndex(), t.type);
            QCOMPARE(field("bgs_spr_grp")->isEnabled(), t.sprite);
            QCOMPARE(field("bgs_spr_num")->isEnabled(), t.sprite);
            QCOMPARE(field("bgs_anim_no")->isEnabled(), t.animation);
            QCOMPARE(field("bgs_mask")->isEnabled(), t.mask);
            QCOMPARE(field("bgs_parallax_mode")->isEnabled(), t.parallax);
            QCOMPARE(field("bgs_parallax_top")->isEnabled(), t.parallax);
            QCOMPARE(field("bgs_y_scale_start")->isEnabled(), t.parallax);
            QVERIFY(!field("bgs_scale_start_x")->isEnabled());
            QVERIFY(!field("bgs_scale_delta_y")->isEnabled());
            QVERIFY(!field("bgs_window_top")->isEnabled());
            for (const char* name : always)
                QVERIFY2(field(name)->isEnabled(), name);
            if (t.type == 2)
                QVERIFY(!view->findChild<QCheckBox*>("bgs_mask")->isChecked());
            if (t.type == 3) {
                view->findChild<QDoubleSpinBox*>("bgs_y_scale_start")->setValue(90);
                QVERIFY(has(page, "yscalestart = 90"));
                view->findChild<QDoubleSpinBox*>("bgs_y_scale_delta")->setValue(1.5);
                QVERIFY(has(page, "yscaledelta = 1.5"));
            }
        }
        choose(type, 0); // Null
        QVERIFY(has(page, "type = Null"));
        QCOMPARE(type->currentText(), QStringLiteral("Null"));
        choose(type, 1);
        QVERIFY(has(page, "type = Normal"));

        const struct
        {
            const char* name;
            double value;
            const char* line;
        } writes[] = {
            {"bgs_velocity_x", 1.5, "velocity = 1.5, 0"},
            {"bgs_velocity_x", 0, nullptr},
            {"bgs_velocity_y", -2, "velocity = 0, -2"},
            {"bgs_id", 5, "id = 5"},
            {"bgs_tile_y", 2, "tile = 0, 2"},
            {"bgs_tile_spacing_x", 4, "tilespacing = 4, 0"},
            {"bgs_spr_num", 1, "spriteno = 1, 1"},
            {"bgs_window_delta_x", 0.5, "windowdelta = 0.5, 0"},
            {"bgs_sin_x_ampl", 10, "sin.x = 10, 0, 0"},
            {"bgs_sin_x_period", 60, "sin.x = 10, 60, 0"},
            {"bgs_sin_x_offset", 15, "sin.x = 10, 60, 15"},
            {"bgs_sin_y_period", 30, "sin.y = 0, 30, 0"},
        };
        for (const auto& w : writes) {
            if (auto* d = view->findChild<QDoubleSpinBox*>(QLatin1String(w.name)))
                d->setValue(w.value);
            else
                view->findChild<QSpinBox*>(QLatin1String(w.name))->setValue(int(w.value));
            if (w.line)
                QVERIFY2(has(page, w.line), w.line);
        }
        view->findChild<QCheckBox*>("bgs_mask")->setChecked(true);
        QVERIFY(has(page, "mask = 1"));
        view->findChild<QCheckBox*>("bgs_mask")->setChecked(false);
        QVERIFY(!page->file()->document()->toPlainText().contains("mask ="));
        view->findChild<QCheckBox*>("bgs_pos_link")->setChecked(true);
        QVERIFY(has(page, "positionlink = 1"));
        view->findChild<QCheckBox*>("bgs_pos_link")->setChecked(false);
        QVERIFY(!page->file()->document()->toPlainText().contains("positionlink"));

        auto* windowMode = view->findChild<QComboBox*>("bgs_window_mode");
        QCOMPARE(windowMode->count(), 2);
        choose(windowMode, 1);
        QVERIFY(has(page, "window = 0, 0, 319, 239"));
        QCOMPARE(view->findChild<QSpinBox*>("bgs_window_bottom")->value(), 239);
        QCOMPARE(view->findChild<QSpinBox*>("bgs_window_right")->value(), 319);
        QVERIFY(field("bgs_window_top")->isEnabled());
        view->findChild<QSpinBox*>("bgs_window_top")->setValue(10);
        QVERIFY(has(page, "window = 0, 10, 319, 239"));
        view->findChild<QSpinBox*>("bgs_window_left")->setValue(20);
        QVERIFY(has(page, "window = 20, 10, 319, 239"));
        choose(windowMode, 0);
        QVERIFY(!page->file()->document()->toPlainText().contains("window ="));
        page->file()->document()->setModified(false);

        QTemporaryDir dir11;
        BackgroundsPage* page11 = openStage(window, dir11, "[BG Back]\ntype = normal\nspriteno = 1, 0\n", "1.1");
        QVERIFY(page11);
        view = window.currentProjectView();
        windowMode = view->findChild<QComboBox*>("bgs_window_mode");
        QCOMPARE(windowMode->count(), 3);
        QCOMPARE(windowMode->itemText(2), QStringLiteral("Top center (mask)"));
        choose(windowMode, 2);
        QVERIFY(has(page11, "maskwindow = -160, 0, 160, 240")); // x from the screen centre
        choose(windowMode, 1);
        QVERIFY(has(page11, "window = 0, 0, 319, 239"));
        choose(windowMode, 0);
        QVERIFY(field("bgs_scale_start_x")->isEnabled());
        view->findChild<QDoubleSpinBox*>("bgs_scale_start_x")->setValue(2);
        QVERIFY(has(page11, "scalestart = 2, 1"));
        view->findChild<QDoubleSpinBox*>("bgs_scale_delta_y")->setValue(0.25);
        QVERIFY(has(page11, "scaledelta = 0, 0.25"));
        type = view->findChild<QComboBox*>("bgs_type");
        choose(type, 3);
        QVERIFY(field("bgs_anim_no")->isEnabled()); // a parallax may be animated in 1.1
        view->findChild<QSpinBox*>("bgs_anim_no")->setValue(5);
        QVERIFY(has(page11, "actionno = 5"));
        QCOMPARE(view->findChild<QSpinBox*>("bgs_anim_no")->value(), 5);
        page11->file()->document()->setModified(false);
    }

    void controllers()
    {
        QTemporaryDir dir;
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        BackgroundsPage* page = openStage(window, dir, Controllers);
        QVERIFY(page);
        ProjectView* view = window.currentProjectView();
        ActionRegistry& actions = m_context->actions();
        auto widget = [view](const char* name) {
            return view->findChild<QWidget*>(QLatin1String(name));
        };
        auto label = [view](const char* name) {
            return view->findChild<QLabel*>(QLatin1String(name))->text();
        };
        auto* ctrlType = view->findChild<QComboBox*>("bgctrls_type");
        auto* loop = view->findChild<QSpinBox*>("bgctrls_def_loop");
        QVERIFY(ctrlType && loop);
        QStringList types;
        for (int i = 0; i < ctrlType->count(); ++i)
            types << ctrlType->itemText(i);
        QCOMPARE(types, (QStringList{"Null", "Enable", "Visible", "Animation", "Set position", "Add position",
                                     "Set velocity", "Add velocity", "Sine (x-axis)", "Sine (y-axis)"}));

        loop->setValue(90);
        QVERIFY(has(page, "looptime = 90"));

        auto shown = [&widget] {
            QStringList names;
            for (const char* name : {"bgctrls_bool", "bgctrls_anim_no", "bgctrls_x_lb", "bgctrls_x", "bgctrls_y",
                                     "bgctrls_ampl", "bgctrls_period", "bgctrls_offset"}) {
                if (!widget(name)->isHidden())
                    names << QString::fromLatin1(name).mid(8);
            }
            return names.join(QLatin1Char(' '));
        };
        QCOMPARE(ctrlType->currentText(), QStringLiteral("Add position"));
        QCOMPARE(shown(), QStringLiteral("x_lb x y"));
        QVERIFY(!widget("bgctrls_start_time")->isHidden());
        choose(ctrlType, 0);
        QCOMPARE(shown(), QString());
        QVERIFY(!widget("bgctrls_start_time")->isHidden());
        choose(ctrlType, 1);
        QCOMPARE(shown(), QStringLiteral("bool"));
        choose(ctrlType, 3);
        QCOMPARE(shown(), QStringLiteral("anim_no"));
        choose(ctrlType, 8);
        QCOMPARE(shown(), QStringLiteral("ampl period offset"));
        choose(ctrlType, 5);
        QVERIFY(has(page, "type = PosAdd"));

        auto* name = view->findChild<QLineEdit*>("bgctrls_name");
        name->setFocus();
        name->selectAll();
        QTest::keyClicks(name, "Jump");
        QVERIFY(has(page, "[BGCtrl Jump]"));
        page->view()->setFocus();
        while (!has(page, "[BGCtrl Shift]"))
            actions.action("edit_undo")->trigger();
        QCOMPARE(name->text(), QStringLiteral("Shift"));

        actions.action("bgctrls_add")->trigger();
        QVERIFY(page->file()->document()->toPlainText().contains("[BGCtrl ]\ntype = Null"));
        QCOMPARE(label("bgctrls_sel_lb"), QStringLiteral("2 / 2"));
        QCOMPARE(ctrlType->currentText(), QStringLiteral("Null"));
        QCOMPARE(view->findChild<QSpinBox*>("bgctrls_start_time")->value(), -1);
        QCOMPARE(view->findChild<QSpinBox*>("bgctrls_end_time")->value(), -1);
        QCOMPARE(view->findChild<QSpinBox*>("bgctrls_loop_time")->value(), -1);
        m_context->config().setValue("confirm_delete", true);
        QStringList asked;
        auto ask = [&asked] {
            whenModal<QMessageBox>([&asked](QMessageBox* box) {
                asked << box->windowTitle() + "|" + box->text();
                box->button(QMessageBox::Yes)->click();
            });
        };
        ask();
        actions.action("bgctrls_delete")->trigger();
        QVERIFY(!page->file()->document()->toPlainText().contains("[BGCtrl ]"));
        QCOMPARE(label("bgctrls_sel_lb"), QStringLiteral("1 / 1"));

        actions.action("bgctrls_def_add")->trigger();
        QCOMPARE(page->file()->document()->toPlainText().section("[BGCtrl Shift]", 1),
                 QStringLiteral("\ntype = PosAdd\ntime = 30\n\n[BGCtrlDef ]\n"));
        QCOMPARE(label("bgctrls_def_sel_lb"), QStringLiteral("2 / 2"));
        QCOMPARE(view->findChild<QLineEdit*>("bgctrls_def_name")->text(), QString());
        QCOMPARE(loop->value(), -1);
        QCOMPARE(view->findChild<QSpinBox*>("bgctrls_def_id_1")->value(), -1);
        QVERIFY(widget("bgctrls_def_id_1")->isEnabled());
        QVERIFY(!widget("bgctrls_def_id_2")->isEnabled());
        QCOMPARE(label("bgctrls_sel_lb"), QStringLiteral("0 / 0"));
        QVERIFY(!widget("bgctrls_type")->isEnabled());
        page->view()->setFocus();
        actions.action("edit_undo")->trigger();
        QCOMPARE(label("bgctrls_def_sel_lb"), QStringLiteral("1 / 1"));
        QCOMPARE(view->findChild<QLineEdit*>("bgctrls_def_name")->text(), QStringLiteral("Mover"));

        ask();
        actions.action("bgctrls_def_delete")->trigger();
        m_context->config().setValue("confirm_delete", false);
        QCOMPARE(asked, (QStringList{boxTitle("Remove") + "|Do you really want to delete the controller?",
                                     boxTitle("Remove definition") +
                                         "|Do you really want to delete the controller definition?"}));
        QVERIFY(!page->file()->document()->toPlainText().contains("BGCtrl"));
        QCOMPARE(label("bgctrls_def_sel_lb"), QStringLiteral("0 / 0"));
        QCOMPARE(name->text(), QString());
        QVERIFY(!name->isEnabled());

        actions.action("bgctrls_def_add")->trigger();
        QVERIFY(page->file()->document()->toPlainText().contains("id = 7\n\n[BGCtrlDef ]"));
        QCOMPARE(label("bgctrls_def_sel_lb"), QStringLiteral("1 / 1"));
        page->file()->document()->setModified(false);
    }

    // Running a screenpack copies it to data/<name>/ and removes it afterwards.
    void runMotif()
    {
#ifdef Q_OS_WIN
        const QString suffix = QStringLiteral(".exe");
#else
        const QString suffix;
#endif
        QTemporaryDir engineDir;
        const QString exe = engineDir.filePath("mugen" + suffix);
        QVERIFY(QFile::copy(QCoreApplication::applicationDirPath() + "/fake_mugen" + suffix, exe));
        QDir().mkpath(engineDir.filePath("data"));
        QTemporaryDir dir;
        MainWindow window(*m_context);
        window.show();
        QVERIFY(openMotif(window, dir));
        Config& engineConfig = m_context->engineConfig("mugen");
        m_context->config().setValue("last_MUGENext", 0);
        RunConfiguration::save(engineConfig, {{"Motif", QDir::toNativeSeparators(exe), "", "-r %0"},
                                              {"Motif again", QDir::toNativeSeparators(exe), "", "-r %0 -p"}});
        auto* output = window.findChild<QTextEdit*>("engineOutput");
        QVERIFY(output);
        QStringList chooser;
        whenModal<QDialog>([&chooser](QDialog* dialog) {
            chooser << dialog->objectName() << dialog->windowTitle() << dialog->findChild<QLabel*>("label")->text();
            auto* list = dialog->findChild<QListWidget*>("list");
            chooser << QString::number(list->viewMode() == QListView::IconMode) << QString::number(list->currentRow());
            for (int i = 0; i < list->count(); ++i)
                chooser << list->item(i)->text();
            list->setCurrentRow(1);
            dialog->findChild<QDialogButtonBox*>("buttonBox")->button(QDialogButtonBox::Ok)->click();
        });
        window.runInEngine();
        QTRY_VERIFY_WITH_TIMEOUT(!window.isEngineRunning(), 15000);
        QCOMPARE(chooser,
                 (QStringList{"RunDlg", "Run in Engine", "Choose the target:", "1", "0", "Motif", "Motif again"}));
        const QString log = output->toPlainText();
        QVERIFY2(log.contains("Arguments: -r ofs_0 -p"), qPrintable(log));
        QVERIFY2(log.contains("Motif definition: found"), qPrintable(log));
        QVERIFY(!QFileInfo::exists(engineDir.filePath("data/ofs_0")));
        QVERIFY(QFileInfo::exists(engineDir.filePath("data")));
        QCOMPARE(m_context->config().intValue("last_MUGENext", -1), 1);

        output->clear();
        int preselected = -1;
        whenModal<QDialog>([&preselected](QDialog* dialog) {
            preselected = dialog->findChild<QListWidget*>("list")->currentRow();
            dialog->reject();
        });
        window.runInEngine();
        QCOMPARE(preselected, 1);
        QVERIFY(!window.isEngineRunning());
        QVERIFY(!output->toPlainText().contains("Arguments"));
        RunConfiguration::save(engineConfig, {});

        const RunCopy copy = Mugen::prepareRunCopy(*window.currentProjectView()->project(), engineDir.path());
        QVERIFY2(copy.isValid(), qPrintable(copy.error));
        QCOMPARE(copy.name, QStringLiteral("ofs_0"));
        QStringList copied;
        for (const QString& file : copy.files)
            copied << QDir(engineDir.filePath("data/ofs_0")).relativeFilePath(file);
        copied.sort();
        QCOMPARE(copied.join(QLatin1Char(' ')),
                 QStringLiteral("fight.def select.def system.def system.sff system.snd"));
        copy.remove();
        QVERIFY(!QFileInfo::exists(engineDir.filePath("data/ofs_0")));
        QVERIFY(QFileInfo::exists(engineDir.filePath("data")));
    }

    // Closing a stage after editing and saving its backgrounds must not crash.
    void closeAfterEditing()
    {
        QTemporaryDir dir;
        MainWindow window(*m_context);
        window.show();
        for (int round = 0; round < 2; ++round) {
            BackgroundsPage* page = nullptr;
            if (round == 0) {
                page = openStage(window, dir, BackAndFront);
            } else {
                QVERIFY(window.openProject(dir.filePath("b.def")));
                m_context->actions().action("editor_bg")->trigger();
                page = window.currentProjectView()->findChild<BackgroundsPage*>();
            }
            QVERIFY(page);
            window.currentProjectView()->findChild<QDoubleSpinBox*>("bgs_start_x")->setValue(-60 + round);
            m_context->actions().action("file_save")->trigger();
            QTRY_VERIFY(!page->file()->isModified());
            m_context->actions().action("file_close")->trigger();
            QTRY_VERIFY(window.projectViews().isEmpty());
        }
        QVERIFY(readFile(dir.filePath("b.def")).contains("start = -59, 20"));
    }

    // A version 1 .fnt font can't be opened as a single file.
    void fontVersion1()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("f.fnt"), QByteArray("ElecbyteFnt\0", 12) + QByteArray(20, '\0'));
        MainWindow window(*m_context);
        window.show();
        FileDialogs::setOpenPromptOverride(
            [&dir](const QString&, const QString&, const QString&) { return dir.filePath("f.fnt"); });
        QStringList titles;
        whenModal<QMessageBox>([&titles](QMessageBox* box) {
            titles << box->windowTitle();
            box->accept();
        });
        m_context->actions().action("file_open_single")->trigger();
        FileDialogs::setOpenPromptOverride({});
        QCOMPARE(titles, QStringList{boxTitle("Open file")});
        QVERIFY(window.projectViews().isEmpty());
    }

    // Don't show this again stores No as "2" and Yes as "1", then answers without asking.
    void dontShowAgain()
    {
        Config& config = m_context->config();
        const QString key = QStringLiteral("showmsg_crop_spr");
        config.setValue(key, "0");
        auto answer = [](QMessageBox::StandardButton button) {
            whenModal<QMessageBox>([button](QMessageBox* box) {
                box->checkBox()->setChecked(true);
                box->button(button)->click();
            });
        };
        answer(QMessageBox::No);
        QVERIFY(!askQuestion(nullptr, config, key, "Crop image", "Do you want to crop the image?"));
        QCOMPARE(config.value(key), QStringLiteral("2"));
        QVERIFY(!askQuestion(nullptr, config, key, "Crop image", "Do you want to crop the image?"));
        config.setValue(key, "0");
        answer(QMessageBox::Yes);
        QVERIFY(askQuestion(nullptr, config, key, "Crop image", "Do you want to crop the image?"));
        QCOMPARE(config.value(key), QStringLiteral("1"));
        QVERIFY(askQuestion(nullptr, config, key, "Crop image", "Do you want to crop the image?"));
        config.setValue(key, "0");
    }

    // Saving image changes on a linked sprite asks about its other instances, even with no change.
    void shareOnSaveImageChanges()
    {
        QTemporaryDir dir;
        SpriteDocument doc;
        doc.palettes() << SpriteDocument::makePalette(1, 1, Formats::grayscalePalette(256));
        const SpritePixelsPtr shared =
            SpriteDocument::makePixels(Formats::Image::indexed(2, 2, std::vector<std::uint8_t>(4, 3)));
        for (int n = 0; n < 3; ++n) {
            SpriteEntry entry;
            entry.header.number = std::uint16_t(n);
            entry.pixels =
                n < 2 ? shared
                      : SpriteDocument::makePixels(Formats::Image::indexed(2, 2, std::vector<std::uint8_t>(4, 9)));
            entry.palette = doc.palettes()[0];
            doc.sprites() << entry;
        }
        const auto bytes = doc.toSff().write();
        QVERIFY(bytes.ok());
        writeFile(dir.filePath("c.sff"),
                  QByteArray(reinterpret_cast<const char*>(bytes.value().data()), qsizetype(bytes.value().size())));
        writeFile(dir.filePath("c.def"),
                  "[Info]\nname = \"C\"\nmugenversion = 1.0\n[Files]\ncns = c.cns\nsprite = c.sff\n");
        writeFile(dir.filePath("c.cns"), "[Data]\nlife = 1000\n");
        m_context->config().setValue("showmsg_share_spr", "0");
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        QVERIFY(window.openProject(dir.filePath("c.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        auto* page = window.currentProjectView()->findChild<SpritePage*>();
        QVERIFY(page);
        SpriteFile* file = page->file();
        QCOMPARE(file->document().linkedSprites(0).size(), 2);
        page->setCurrent(0);
        actions.action("spr_edit_img")->trigger();
        QStringList asked;
        answerBoxes({QMessageBox::Yes}, &asked);
        actions.action("spr_saveimg")->trigger();
        QCOMPARE(asked, QStringList{"Do you want to use this image in all other instances of this sprite?"});
        QCOMPARE(file->document().linkedSprites(0).size(), 2);
        answerBoxes({QMessageBox::No}, &asked);
        actions.action("spr_saveimg")->trigger();
        QCOMPARE(asked.size(), 2);
        QCOMPARE(file->document().linkedSprites(0).size(), 1);
        QVERIFY(file->document().sprites()[0].pixels != file->document().sprites()[1].pixels);
        actions.action("spr_edit_img")->trigger();
        page->setCurrent(2);
        actions.action("spr_edit_img")->trigger();
        actions.action("spr_saveimg")->trigger();
        QVERIFY(!QApplication::activeModalWidget());
        actions.action("spr_edit_img")->trigger();
        while (file->undoStack()->canUndo())
            file->undoStack()->undo();
    }

    void emptySaveRefusals()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("k.sff"), spriteFile());
        writeFile(dir.filePath("k.snd"), soundFile());
        writeFile(dir.filePath("k.def"), "[Info]\nname = \"K\"\nmugenversion = 1.0\n"
                                         "[Files]\ncns = k.cns\nsprite = k.sff\nsound = k.snd\n");
        writeFile(dir.filePath("k.cns"), "[Data]\nlife = 1000\n");
        const QByteArray sff = readFile(dir.filePath("k.sff"));
        const QByteArray snd = readFile(dir.filePath("k.snd"));
        Config& config = m_context->config();
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("k.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        SpriteFile* sprites = window.currentProjectView()->findChild<SpritePage*>()->file();
        SpriteDocument empty = sprites->document();
        empty.sprites().clear();
        sprites->apply("Delete", empty);
        int fileDialogs = 0;
        FileDialogs::setSavePromptOverride([&fileDialogs](const QString&, const QString&, const QString&) {
            ++fileDialogs;
            return QString();
        });
        config.setValue("showmsg_sprs_empty", "true");
        QStringList texts;
        answerModals({[](QWidget* w) {
                          QCOMPARE(w->objectName(), QStringLiteral("SaveSpr"));
                          qobject_cast<QDialog*>(w)->accept();
                      },
                      [&texts](QWidget* w) {
                          auto* box = qobject_cast<QMessageBox*>(w);
                          QVERIFY(box);
                          texts << box->windowTitle() + "|" + box->text();
                          box->accept();
                      }});
        actions.action("spr_saveas")->trigger();
        QCOMPARE(texts, QStringList{boxTitle("Save SFF") +
                                    "|You can't save an empty SFF because M.U.G.E.N. will not open it."});
        QCOMPARE(fileDialogs, 0);
        texts.clear();
        answerBoxes({QMessageBox::Ok}, &texts);
        actions.action("file_save")->trigger();
        QTRY_COMPARE(texts, QStringList{"You can't save an empty SFF because M.U.G.E.N. will not open it."});
        QCOMPARE(readFile(dir.filePath("k.sff")), sff);
        QVERIFY(sprites->isModified());
        config.setValue("showmsg_sprs_empty", "false");
        sprites->undoStack()->undo();

        actions.action("editor_snd")->trigger();
        SoundFile* sounds = window.currentProjectView()->findChild<SoundPage*>()->file();
        SoundDocument noSounds = sounds->document();
        noSounds.sounds().clear();
        sounds->apply("Delete", noSounds);
        config.setValue("showmsg_snds_empty", "true");
        texts.clear();
        answerBoxes({QMessageBox::Ok}, &texts);
        actions.action("snd_saveas")->trigger();
        QCOMPARE(texts, QStringList{"You can't save an empty SND because M.U.G.E.N. will not open it."});
        QCOMPARE(fileDialogs, 0);
        QCOMPARE(readFile(dir.filePath("k.snd")), snd);
        config.setValue("showmsg_snds_empty", "false");
        FileDialogs::setSavePromptOverride({});
        sounds->undoStack()->undo();
        QTRY_VERIFY(!QApplication::activeModalWidget());
    }

    void autosaveRestartsOnApply()
    {
        m_context->config().setValue("autosave_interval", 1);
        MainWindow window(*m_context);
        window.show();
        auto* timer = window.findChild<QTimer*>("autosaveTimer");
        QVERIFY(timer && timer->isActive());
        QCOMPARE(timer->interval(), 60 * 1000);
        int afterFirst = -1;
        int beforeSecond = -1;
        int afterSecond = -1;
        whenModal<QDialog>([timer, &afterFirst, &beforeSecond, &afterSecond](QDialog* dialog) {
            QCOMPARE(dialog->objectName(), QStringLiteral("Options"));
            const auto close = qScopeGuard([dialog] { dialog->findChild<QPushButton*>("closeBut")->click(); });
            dialog->findChild<QSpinBox*>("saveInterval")->setValue(2);
            dialog->findChild<QPushButton*>("applyBut")->click();
            afterFirst = timer->remainingTime();
            QTest::qWait(3000);
            beforeSecond = timer->remainingTime();
            dialog->findChild<QPushButton*>("applyBut")->click();
            afterSecond = timer->remainingTime();
        });
        m_context->actions().action("tools_options")->trigger();
        QCOMPARE(timer->interval(), 2 * 60 * 1000);
        QVERIFY2(afterFirst > 60 * 1000, qPrintable(QString::number(afterFirst)));
        QVERIFY2(afterSecond > beforeSecond, qPrintable(QStringLiteral("%1 %2").arg(beforeSecond).arg(afterSecond)));
        m_context->config().setValue("autosave_interval", 0);
    }

private:
    BackgroundsPage* openStage(MainWindow& window, const QTemporaryDir& dir, const char* elements,
                               const char* version = "1.0")
    {
        writeFile(dir.filePath("b.sff"), spriteFile());
        writeFile(dir.filePath("b.def"),
                  QString::fromLatin1(StageHead).arg(QLatin1String(version)).toLatin1() + elements);
        if (!window.openProject(dir.filePath("b.def")))
            return nullptr;
        m_context->actions().action("editor_bg")->trigger();
        ProjectView* view = window.currentProjectView();
        return view && view->currentEditor() == QLatin1String("backgrounds") ? view->findChild<BackgroundsPage*>()
                                                                             : nullptr;
    }

    bool openMotif(MainWindow& window, const QTemporaryDir& dir)
    {
        writeFile(dir.filePath("system.sff"), spriteFile());
        writeFile(dir.filePath("system.snd"), soundFile());
        writeFile(dir.filePath("select.def"), "[Characters]\nkfm\n[ExtraStages]\n");
        writeFile(dir.filePath("fight.def"), "[Files]\nsff = fight.sff\n[FightFX]\nscale = 1\n");
        writeFile(dir.filePath("system.def"), "[Info]\nname = \"Mine\"\nmugenversion = 1.0\nlocalcoord = 320, 240\n"
                                              "[Files]\nspr = system.sff\nsnd = system.snd\nselect = select.def\n"
                                              "fight = fight.def\n[Title Info]\n[TitleBGdef]\n"
                                              "[TitleBG 0]\ntype = normal\nspriteno = 0, 0\n");
        if (!window.openProject(dir.filePath("system.def")))
            return false;
        m_context->actions().action("editor_bg")->trigger();
        return window.currentProjectView()->currentEditor() == QLatin1String("backgrounds");
    }

    static bool has(BackgroundsPage* page, const char* line)
    {
        return page->file()->document()->toPlainText().split(QLatin1Char('\n')).contains(QLatin1String(line));
    }

    AppContext* m_context = nullptr;
};

QTEST_MAIN(BackgroundsParityTest)
#include "BackgroundsParityTest.moc"
