#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/script/LuaScript.h"
#include "engines/mugen/AirEdits.h"
#include "engines/mugen/AirFile.h"
#include "engines/mugen/MugenEngine.h"
#include "engines/mugen/ProjectGraphics.h"
#include "engines/mugen/ThrowCreator.h"
#include "engines/mugen/dialogs/AddAnimationDialog.h"
#include "engines/mugen/dialogs/AnimationPreview.h"
#include "engines/mugen/dialogs/OffsetViewerDialog.h"
#include "engines/mugen/dialogs/PalFXDialog.h"
#include "engines/mugen/dialogs/ThrowCreatorDialog.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QSlider>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>

#include <functional>
#include <memory>

using namespace OFS;
using namespace OFS::Mugen;

namespace {

QString sourceFile(const QString& path)
{
    return QStringLiteral(QT_TESTCASE_SOURCEDIR "/") + path;
}

QString throwScript()
{
    return sourceFile(QStringLiteral("../data/engines/mugen/scripts/common/Throw.lua"));
}

QString readText(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

void writeFile(const QString& path, const QByteArray& data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

template <typename T>
void whenModal(std::function<void(T*)> answer)
{
    auto* timer = new QTimer(qApp);
    timer->setInterval(5);
    QObject::connect(timer, &QTimer::timeout, qApp, [timer, answer] {
        if (auto* window = qobject_cast<T*>(QApplication::activeModalWidget()); window && window->isVisible()) {
            timer->stop();
            timer->deleteLater();
            answer(window);
        }
    });
    timer->start();
}

template <typename T>
T* child(QWidget* parent, const char* name)
{
    T* widget = parent->findChild<T*>(QLatin1String(name));
    if (!widget)
        qWarning("no %s", name);
    return widget;
}

} // namespace

class MugenToolsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void luaScript()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("t.lua");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(
            "function describe(t)\n"
            "  return t.name .. ':' .. t.count .. ':' .. string.format('%.2f', t.ratio) .. ':' .. #t.items .. ':' ..\n"
            "         t.items[2].v .. ':' .. tostring(t.flag) .. ':' .. math.type(t.count)\n"
            "end\n"
            "function number() return 3 end\n"
            "function broken() error('nope') end\n");
        f.close();
        const QVariantMap argument{{"name", "kfm"},
                                   {"count", 15},
                                   {"ratio", 1.5},
                                   {"flag", true},
                                   {"items", QVariantList{QVariantMap{{"v", "a"}}, QVariantMap{{"v", "b"}}}}};
        QString error;
        QCOMPARE(Script::callFunction(path, "describe", argument, &error),
                 QStringLiteral("kfm:15:1.50:2:b:true:integer"));
        QVERIFY(error.isEmpty());
        QCOMPARE(Script::callFunction(path, "number", {}, &error), QStringLiteral("3"));
        QCOMPARE(Script::callFunction(path, "broken", {}, &error), QString());
        QVERIFY2(error.contains("nope"), qPrintable(error));
        error.clear();
        QCOMPARE(Script::callFunction(path, "missing", {}, &error), QString());
        QVERIFY(error.contains("missing"));
        QCOMPARE(Script::callFunction(dir.filePath("none.lua"), "describe", {}, &error), QString());
        QVERIFY(!error.isEmpty());
    }

    void throwDefault()
    {
        ThrowSettings settings;
        settings.startAnim = 800;
        settings.throwAnim = 810;
        settings.thrownAnim = 820;
        settings.binds = QList<ThrowBind>(15);
        QString error;
        const QString text = throwStates(throwScript(), settings, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(text, readText(sourceFile("mugen/data/throw-default.txt")));
        QCOMPARE(text.count(QLatin1Char('\n')), 172);
        QVERIFY(!text.contains(QLatin1Char('\t')));
        QVERIFY(!text.contains(QStringLiteral(" \n")));
    }

    void throwSettings()
    {
        ThrowSettings settings;
        settings.stateNo = 1000;
        settings.startAnim = 800;
        settings.throwAnim = 810;
        settings.thrownAnim = 820;
        settings.xVelocity = 3;
        settings.yVelocity = -6;
        settings.binds = QList<ThrowBind>(5);
        settings.binds[0].x = 60;
        settings.binds[0].priority = 2;
        settings.binds[1].x = 40;
        settings.binds[1].y = -10;
        QString error;
        QCOMPARE(throwStates(throwScript(), settings, &error), readText(sourceFile("mugen/data/throw-modified.txt")));
        QVERIFY2(error.isEmpty(), qPrintable(error));

        settings.binds = {ThrowBind{12.345, -0.0, -3}};
        const QString text = throwStates(throwScript(), settings, &error);
        QVERIFY(text.contains("pos = 12.3, 0.0\n"));
        QVERIFY(text.contains("[State 1010, SprPriority]\ntype = SprPriority\ntrigger1 = AnimElem = 1\nvalue = -3\n"));
        QVERIFY(text.contains("[State 1010, Hurt]\ntype = TargetLifeAdd\ntrigger1 = AnimElem = 1\n"));
    }

    void animations()
    {
        const QString air = QStringLiteral("; Standing Animation\r\n[Begin Action 000]\r\nClsn2Default: 1\r\n"
                                           " Clsn2[0] = -10, 0, 10, -79\r\n0,0, 0,0, 10\r\n0,1, 0,0, 7, H\r\n"
                                           "Loopstart\r\n0,2, 1,-2, 7 ,HV, A\r\n\r\n"
                                           "[Begin Action 5]\r\n5,0, 0,0, -1 ; no name\r\n"
                                           ";-----\r\n[Begin Action 6]\r\nInterpolate Offset\r\n6,0,0,0,3\r\n"
                                           "[Begin Action x]\r\n1,1,0,0,1\r\n");
        const QList<AirAction> actions = readAnimations(air);
        QCOMPARE(actions.size(), 3);
        QCOMPARE(actions[0].caption(), QStringLiteral("0 - Standing Animation"));
        QCOMPARE(actions[0].line, 1);
        QCOMPARE(actions[0].frames.size(), 3);
        QCOMPARE(actions[0].frames[2].group, 0);
        QCOMPARE(actions[0].frames[2].index, 2);
        QCOMPARE(actions[0].frames[2].x, 1);
        QCOMPARE(actions[0].frames[2].y, -2);
        QCOMPARE(actions[0].frames[2].time, 7);
        QCOMPARE(actions[0].frames[2].line, 7);
        QVERIFY(actions[0].flipsHorizontally(1));
        QVERIFY(!actions[0].flipsVertically(1));
        QVERIFY(actions[0].flipsHorizontally(2) && actions[0].flipsVertically(2));
        QVERIFY(!actions[0].flipsHorizontally(9));
        QCOMPARE(actions[1].caption(), QStringLiteral("5"));
        QCOMPARE(actions[1].frames.size(), 1);
        QCOMPARE(actions[1].frames[0].time, -1);
        QCOMPARE(actions[2].caption(), QStringLiteral("6"));
        QCOMPARE(actions[2].frames.size(), 1);
        QCOMPARE(findAnimation(actions, 5), &actions[1]);
        QCOMPARE(findAnimation(actions, 7), nullptr);
    }
    void timeline()
    {
        Mugen::AirAction action;
        for (int time : {2, 3, -1, 4}) {
            Mugen::AirFrame frame;
            frame.time = time;
            action.frames << frame;
        }
        QCOMPARE(Mugen::elementStart(action, 0), qint64(0));
        QCOMPARE(Mugen::elementStart(action, 2), qint64(5));
        QCOMPARE(Mugen::elementStart(action, 3), Mugen::Never);
        QCOMPARE(Mugen::elementAt(action, 0), 0);
        QCOMPARE(Mugen::elementAt(action, 1), 0);
        QCOMPARE(Mugen::elementAt(action, 2), 1);
        QCOMPARE(Mugen::elementAt(action, 5), 2);
        QCOMPARE(Mugen::elementAt(action, 1000), 2);
        QCOMPARE(Mugen::elementAt(action, Mugen::Never), 2);
        action.frames.removeAt(2);
        QCOMPARE(Mugen::elementAt(action, 1000), 2);
        QCOMPARE(Mugen::elementAt(Mugen::AirAction(), 0), -1);
    }

    void frameText()
    {
        const QList<Mugen::AirAction> actions =
            Mugen::readAnimations("[Begin Action 1]\n5030,30, 0,0,-1\n  1,2, 3,4, 5, , A ; note\n7,8,0,0,9,h\n");
        QCOMPARE(actions.size(), 1);
        Mugen::AirFrame frame = actions[0].frames[0];
        QCOMPARE(frame.text(), QStringLiteral("5030,30, 0,0, -1"));
        frame.flip = "H";
        QCOMPARE(frame.text(), QStringLiteral("5030,30, 0,0, -1, H"));
        QCOMPARE(actions[0].frames[1].text(), QStringLiteral("  1,2, 3,4, 5, , A ; note"));
        QCOMPARE(actions[0].frames[2].text(), QStringLiteral("7,8, 0,0, 9, h"));
    }

    void airEdits()
    {
        QTextDocument doc;
        auto action = [&doc](int index = 0) {
            return Mugen::readAnimations(doc.toPlainText()).value(index);
        };
        auto body = [&doc] {
            return doc.toPlainText().section('\n', 1);
        };

        doc.setPlainText("[Begin Action 831]\n-1,0, 0,0, -1");
        QVERIFY(Mugen::AirEdits::splitFrame(&doc, action(), 2));
        QCOMPARE(body(), QStringLiteral("-1,0, 0,0, 2\n-1,0, 0,0, -1"));
        QVERIFY(Mugen::AirEdits::splitFrame(&doc, action(), 15));
        QCOMPARE(body(), QStringLiteral("-1,0, 0,0, 2\n-1,0, 0,0, 13\n-1,0, 0,0, -1"));
        QVERIFY(Mugen::AirEdits::splitFrame(&doc, action(), 5));
        QCOMPARE(body(), QStringLiteral("-1,0, 0,0, 2\n-1,0, 0,0, 3\n-1,0, 0,0, 10\n-1,0, 0,0, -1"));
        QVERIFY(!Mugen::AirEdits::splitFrame(&doc, action(), 5));
        doc.undo();
        QCOMPARE(body(), QStringLiteral("-1,0, 0,0, 2\n-1,0, 0,0, 13\n-1,0, 0,0, -1"));

        doc.setPlainText("[Begin Action 820]\n5030,40, 0,0, 5\nClsn2: 1\n Clsn2[0] = 1,2,3,4\n5030,30, 0,0, -1");
        QVERIFY(Mugen::AirEdits::splitFrame(&doc, action(), 4));
        QCOMPARE(body(),
                 QStringLiteral("5030,40, 0,0, 4\n-1,0, 0,0, 1\nClsn2: 1\n Clsn2[0] = 1,2,3,4\n5030,30, 0,0, -1"));
        doc.setPlainText("[Begin Action 5]\n5,0, 0,0, 4, H\n5,0,0,0,4");
        QVERIFY(Mugen::AirEdits::splitFrame(&doc, action(), 15));
        QCOMPARE(body(), QStringLiteral("5,0, 0,0, 4, H\n5,0, 0,0, 11\n-1,0, 0,0, -1"));

        doc.setPlainText("[Begin Action 1]\n-1,0, 0,0, 2\n-1,0, 0,0, 3\n-1,0, 0,0, 10\n5071,10, 0,0, -1, H");
        QVERIFY(Mugen::AirEdits::removeFrame(&doc, action(), 5));
        QCOMPARE(body(), QStringLiteral("-1,0, 0,0, 2\n-1,0, 0,0, 13\n5071,10, 0,0, -1, H"));
        doc.setPlainText("[Begin Action 1]\n5071,10, 0,0, 5, H\n-1,0, 0,0, -1");
        QVERIFY(Mugen::AirEdits::removeFrame(&doc, action(), 20));
        QCOMPARE(body(), QStringLiteral("5071,10, 0,0, -1, H"));
        QVERIFY(!Mugen::AirEdits::removeFrame(&doc, action(), 20));
        doc.setPlainText("[Begin Action 1]\n1,0, 0,0, 3\n2,0, 0,0, 4");
        QVERIFY(!Mugen::AirEdits::removeFrame(&doc, action(), 1));
        doc.setPlainText("[Begin Action 1]\nClsn2Default: 1\n Clsn2[0] = 0,0,1,1\n1,0, 0,0, 3\nClsn1: 1\n"
                         " Clsn1[0] = 0,0,2,2\n2,0, 0,0, 4\n3,0, 0,0, 5\n\n[Begin Action 2]\n9,9,0,0,1");
        QVERIFY(Mugen::AirEdits::removeFrame(&doc, action(), 3));
        QCOMPARE(doc.toPlainText(), QStringLiteral("[Begin Action 1]\nClsn2Default: 1\n Clsn2[0] = 0,0,1,1\n"
                                                   "1,0, 0,0, 7\n3,0, 0,0, 5\n\n[Begin Action 2]\n9,9,0,0,1"));
        QVERIFY(Mugen::AirEdits::removeFrame(&doc, action(1), 0) == false);
        doc.setPlainText("[Begin Action 1]\n1,0, 0,0, 3\n2,0, 0,0, 4");
        QVERIFY(Mugen::AirEdits::removeFrame(&doc, action(), 3));
        QCOMPARE(doc.toPlainText(), QStringLiteral("[Begin Action 1]\n1,0, 0,0, 7"));

        Mugen::AirFrame frame = action().frames[0];
        frame.group = 5071;
        frame.flip = "HV";
        Mugen::AirEdits::setFrame(&doc, frame);
        QCOMPARE(body(), QStringLiteral("5071,0, 0,0, 7, HV"));

        doc.setPlainText("[Begin Action 1]\n5300,1, 0,0, 8\n");
        Mugen::AirEdits::appendAnimation(&doc, 830, "Test Thrown", 0);
        QCOMPARE(doc.toPlainText(), QStringLiteral("[Begin Action 1]\n5300,1, 0,0, 8\n; Test Thrown\n"
                                                   "[Begin Action 830]\n-1,0, 0,0, -1\n"));
        doc.setPlainText("x");
        Mugen::AirEdits::appendAnimation(&doc, 0, "", 7);
        QCOMPARE(doc.toPlainText(), QStringLiteral("x\n[Begin Action 0]\n-1,0, 0,0, 7\n"));
        doc.undo();
        QCOMPARE(doc.toPlainText(), QStringLiteral("x"));
    }

    void contactBoxes()
    {
        QTextDocument doc;
        doc.setPlainText("[Begin Action 200]\nClsn2Default: 1\n Clsn2[0] = 1, 2, 3, 4\n200,0, 0,0, 3\n"
                         "Clsn1: 1\n Clsn1[0] =  16,-80, 61,-71\nClsn2: 3\n Clsn2[0] =  19,  0,-10,-80\n"
                         " Clsn2[1] =   6,-94, 18,-78\n Clsn2[2] =  19,-80, 61,-71\n200,2, 0,0, 3\n"
                         "Clsn1: 1\n Clsn1[0] = 0,0,1,1\n200,3, 0,0, 3\n");
        QCOMPARE(Mugen::AirEdits::scaleContactBoxes(&doc, {{200, 2}}, 2.0, 2.0), 4);
        QCOMPARE(doc.toPlainText(), QStringLiteral("[Begin Action 200]\nClsn2Default: 1\n Clsn2[0] = 1, 2, 3, 4\n"
                                                   "200,0, 0,0, 3\nClsn1: 1\n  Clsn1[0] = 32, -160, 123, -141\n"
                                                   "Clsn2: 3\n  Clsn2[0] = -20, -160, 39, 1\n"
                                                   "  Clsn2[1] = 12, -188, 37, -155\n"
                                                   "  Clsn2[2] = 38, -160, 123, -141\n200,2, 0,0, 3\n"
                                                   "Clsn1: 1\n Clsn1[0] = 0,0,1,1\n200,3, 0,0, 3\n"));
        doc.undo();
        QVERIFY(doc.toPlainText().contains(" Clsn1[0] =  16,-80, 61,-71\n"));
        QCOMPARE(Mugen::AirEdits::scaleContactBoxes(&doc, {{999, 0}}, 2.0, 2.0), 0);
    }

    void previewScreen()
    {
        Mugen::AnimationPreview preview;
        QCOMPARE(preview.sizeHint(), QSize(782, 452));
        preview.resize(782, 452);
        QCOMPARE(preview.origin(), QPointF(320, 341)); // half way across, at 170/240 of the height
        preview.setScreenSize(QSize(320, 240));
        QCOMPARE(preview.origin(), QPointF(160, 171));
        preview.setScreenSize(QSize(1280, 720));
        QCOMPARE(preview.sizeHint(), QSize(1258, 692));
        QVERIFY(preview.origin().y() < 452);
    }

    void throwCreatorDialog()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("thrower.def"), "[Info]\nname = \"Thrower\"\nmugenversion = 1.0\n"
                                               "[Files]\ncns = thrower.cns\nsprite = t.sff\nanim = thrower.air\n");
        writeFile(dir.filePath("thrower.cns"), "[Data]\nlife = 1000\n");
        const QByteArray air = "[Begin Action 0]\n0,0, 0,0, -1\n\n"
                               "; Throw\n[Begin Action 810]\n810,0, 0,0, 3\n810,1, 6,0, 5\n810,2, 0,0, 4\n\n"
                               "; Thrown\n[Begin Action 820]\n5000,0, 0,0, 5\n5010,0, 4,0, 5, V\n";
        writeFile(dir.filePath("thrower.air"), air);
        writeFile(dir.filePath("big.def"), "[Info]\nname = \"Big\"\nlocalcoord = 640, 480\nmugenversion = 1.1\n"
                                           "[Files]\ncns = thrower.cns\nanim = big.air\n");
        writeFile(dir.filePath("big.air"), "; Big thrown\n[Begin Action 7]\n1,0, 2,0, 5\n");
        Mugen::MugenEngine engine;
        QString error;
        std::unique_ptr<Project> thrower(engine.openProject(dir.filePath("thrower.def"), &error));
        QVERIFY2(thrower, qPrintable(error));
        std::unique_ptr<Project> big(engine.openProject(dir.filePath("big.def"), &error));
        QVERIFY2(big, qPrintable(error));

        Mugen::ThrowCreatorDialog dialog(thrower.get(), {thrower.get(), big.get()}, throwScript());
        QCOMPARE(child<QComboBox>(&dialog, "p1AnimStart")->currentText(), QStringLiteral("0"));
        QCOMPARE(child<QComboBox>(&dialog, "p1Anim")->currentText(), QStringLiteral("810 - Throw"));
        QCOMPARE(child<QComboBox>(&dialog, "p2Anim")->currentText(), QStringLiteral("820 - Thrown"));
        QCOMPARE(child<QSpinBox>(&dialog, "elem")->value(), 3);
        QCOMPARE(child<QComboBox>(&dialog, "projs")->count(), 2);
        QCOMPARE(child<QComboBox>(&dialog, "projs")->currentText(), QStringLiteral("Thrower"));
        QCOMPARE(child<QToolButton>(&dialog, "insFrame")->toolTip(), QStringLiteral("Insert frame"));

        // Element 1 starts at tick 3, element 2 at tick 8.
        Mugen::AnimationPreview* preview = dialog.preview();
        auto* slider = child<QSlider>(&dialog, "horizontalSlider");
        auto* group = child<QSpinBox>(&dialog, "grp");
        slider->setValue(1);
        QCOMPARE(group->value(), 5000);
        slider->setValue(2);
        QCOMPARE(group->value(), 5010);
        QCOMPARE(child<QComboBox>(&dialog, "flipList")->currentText(), QStringLiteral("Flip V"));
        slider->setValue(1);
        QCOMPARE(preview->figures()[0].position, QPointF(12, 0));
        QCOMPARE(preview->figures()[0].scale, 2.0);
        slider->setValue(2);
        QCOMPARE(preview->figures()[1].position, QPointF(75 * 2 - 4 * 2, 0));
        QVERIFY(preview->figures()[1].flipH);
        QVERIFY(preview->figures()[1].flipV);
        child<QComboBox>(&dialog, "res")->setCurrentText("1280x720");
        QCOMPARE(preview->screenSize(), QSize(1280, 720));
        QCOMPARE(preview->figures()[1].position, QPointF(75 * 4 - 4 * 4, 0));
        child<QComboBox>(&dialog, "res")->setCurrentText("640x480");

        child<QComboBox>(&dialog, "projs")->setCurrentIndex(1);
        QCOMPARE(child<QComboBox>(&dialog, "p2Anim")->currentText(), QStringLiteral("820 - Thrown"));
        QCOMPARE(preview->figures()[1].scale, 1.0);
        QCOMPARE(preview->figures()[1].position, QPointF(75 * 2 - 4, 0));
        child<QComboBox>(&dialog, "projs")->setCurrentIndex(0);

        auto* file = qobject_cast<CodeFile*>(thrower->file(dir.filePath("thrower.air")));
        QVERIFY(file);
        QTextDocument* text = file->document();
        group->setValue(5011);
        QVERIFY(text->toPlainText().contains("\n5011,0, 4,0, 5, V\n"));
        QVERIFY(file->isModified());
        text->undo();
        QVERIFY(text->toPlainText().contains("\n5010,0, 4,0, 5, V\n"));
        // After select all and 5, 0, 1, 2, undo goes 5012 -> 5 -> 5010.
        dialog.show();
        dialog.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&dialog));
        group->setFocus();
        QTRY_VERIFY(group->hasFocus());
        group->selectAll();
        QTest::keyClicks(group, "5012");
        QCOMPARE(group->value(), 5012);
        QVERIFY(text->toPlainText().contains("\n5012,0, 4,0, 5, V\n"));
        text->undo();
        QVERIFY(text->toPlainText().contains("\n5,0, 4,0, 5, V\n"));
        text->undo();
        QVERIFY(text->toPlainText().contains("\n5010,0, 4,0, 5, V\n"));
        group->clearFocus();
        slider->setValue(1);
        child<QToolButton>(&dialog, "insFrame")->click();
        QVERIFY(text->toPlainText().contains("[Begin Action 820]\n5000,0, 0,0, 3\n-1,0, 0,0, 2\n5010,0"));
        QCOMPARE(group->value(), -1);
        child<QToolButton>(&dialog, "delFrame")->click();
        QVERIFY(text->toPlainText().contains("[Begin Action 820]\n5000,0, 0,0, 5\n5010,0"));
        QCOMPARE(group->value(), 5000);

        bool asked = false;
        QTimer::singleShot(0, this, [&asked] {
            auto* add = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(add);
            QCOMPARE(add->objectName(), QStringLiteral("AddAnim"));
            QCOMPARE(add->windowTitle(), QStringLiteral("Add animation"));
            QCOMPARE(add->findChild<QSpinBox*>("number")->value(), 821);
            add->findChild<QLineEdit*>("name")->setText("Spin");
            asked = true;
            add->accept();
        });
        child<QToolButton>(&dialog, "addAnim")->click();
        QVERIFY(asked);
        QVERIFY(text->toPlainText().endsWith("5010,0, 4,0, 5, V\n; Spin\n[Begin Action 821]\n-1,0, 0,0, -1\n"));
        QCOMPARE(child<QComboBox>(&dialog, "p2Anim")->currentText(), QStringLiteral("821 - Spin"));
        QCOMPARE(child<QComboBox>(&dialog, "p1Anim")->count(), 4);
        QCOMPARE(child<QComboBox>(&dialog, "p1Anim")->currentText(), QStringLiteral("810 - Throw"));
        QCOMPARE(group->value(), -1);
        QCOMPARE(child<QSpinBox>(&dialog, "elem")->value(), 3);

        child<QDoubleSpinBox>(&dialog, "bindX")->setValue(10);
        child<QSpinBox>(&dialog, "elem")->setValue(5);
        QCOMPARE(slider->maximum(), 4);
        slider->setValue(4);
        QCOMPARE(child<QDoubleSpinBox>(&dialog, "bindX")->value(), 75.0);
        const Mugen::ThrowSettings settings = dialog.settings();
        QCOMPARE(settings.binds.size(), 5);
        QCOMPARE(settings.binds[1].x, 10.0);
        QCOMPARE(settings.startAnim, 0);
        QCOMPARE(settings.throwAnim, 810);
        QCOMPARE(settings.thrownAnim, 821);

        QString inserted;
        dialog.setInsertHandler([&inserted](const QString& text) { inserted = text; });
        child<QPushButton>(&dialog, "ins")->click();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(inserted, Mugen::throwStates(throwScript(), settings));
        QVERIFY(inserted.contains("value = 821\n"));
    }

    void throwCreatorDetails()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("t.def"), "[Info]\nname = \"T\"\nmugenversion = 1.0\n"
                                         "[Files]\ncns = t.cns\nanim = t.air\n");
        writeFile(dir.filePath("t.cns"), "[Data]\nlife = 1000\n");
        writeFile(dir.filePath("t.air"), "[Begin Action 0]\n0,0, 0,0, -1\n\n"
                                         "; Throw\n[Begin Action 810]\n810,0, 0,0, 3\n810,1, 0,0, 5\n810,2, 0,0, 4\n\n"
                                         "; Thrown\n[Begin Action 820]\n5000,0, 0,0, 5\n5010,0, 0,0, 5\n\n");
        writeFile(dir.filePath("o.def"),
                  "[Info]\nname = \"O\"\nmugenversion = 1.0\n[Files]\ncns = t.cns\nanim = o.air\n");
        writeFile(dir.filePath("o.air"), "[Begin Action 7]\n1,0, 0,0, 5\n");
        Mugen::MugenEngine engine;
        QString error;
        std::unique_ptr<Project> thrower(engine.openProject(dir.filePath("t.def"), &error));
        QVERIFY2(thrower, qPrintable(error));
        std::unique_ptr<Project> other(engine.openProject(dir.filePath("o.def"), &error));
        QVERIFY2(other, qPrintable(error));
        auto dialog = std::make_unique<Mugen::ThrowCreatorDialog>(
            thrower.get(), QList<Project*>{thrower.get(), other.get()}, throwScript());
        auto* file = qobject_cast<CodeFile*>(thrower->file(dir.filePath("t.air")));
        CodeFile* otherFile = other->codeFile(dir.filePath("o.air"), QStringLiteral("animations"));
        QVERIFY(file && otherFile);
        QTextDocument* text = file->document();
        const QString otherText = otherFile->document()->toPlainText();
        auto range = [&](const char* name) {
            auto* spin = child<QAbstractSpinBox>(dialog.get(), name);
            if (auto* i = qobject_cast<QSpinBox*>(spin))
                return qMakePair(double(i->minimum()), double(i->maximum()));
            auto* d = qobject_cast<QDoubleSpinBox*>(spin);
            return qMakePair(d->minimum(), d->maximum());
        };
        QCOMPARE(range("p1SDef"), qMakePair(0.0, 1000000.0));
        QCOMPARE(range("elem"), qMakePair(0.0, 99.0));
        QCOMPARE(range("bindX"), qMakePair(-1000.0, 1000.0));
        QCOMPARE(range("bindY"), qMakePair(-1000.0, 1000.0));
        QCOMPARE(child<QDoubleSpinBox>(dialog.get(), "bindX")->decimals(), 3);
        QCOMPARE(range("bindPriority"), qMakePair(-99.0, 99.0));
        QCOMPARE(range("xVel"), qMakePair(-65535.0, 65535.0));
        QCOMPARE(range("yVel"), qMakePair(-65535.0, 65535.0));
        QCOMPARE(range("grp"), qMakePair(-1.0, 65535.0));
        QCOMPARE(range("idx"), qMakePair(-1.0, 65535.0));
        QStringList resolutions;
        auto* res = child<QComboBox>(dialog.get(), "res");
        for (int i = 0; i < res->count(); ++i)
            resolutions << res->itemText(i);
        QCOMPARE(resolutions, (QStringList{"320x240", "640x480", "800x600", "1024x768", "1280x720", "1920x1080"}));
        QCOMPARE(res->currentText(), QStringLiteral("640x480"));

        QCOMPARE(child<QToolButton>(dialog.get(), "addAnim")->toolTip(), QStringLiteral("Add animation"));
        QCOMPARE(child<QToolButton>(dialog.get(), "insFrame")->toolTip(), QStringLiteral("Insert frame"));
        QCOMPARE(child<QToolButton>(dialog.get(), "delFrame")->toolTip(), QStringLiteral("Remove current frame"));
        for (const char* name :
             {"grp", "idx", "flipList", "p2Anim", "projs", "xVel", "yVel", "elem", "horizontalSlider", "bindX", "bindY",
              "bindPriority", "p1SDef", "p1AnimStart", "p1Anim", "res", "ins", "close"})
            QVERIFY2(child<QWidget>(dialog.get(), name)->toolTip().isEmpty(), name);

        const QString before = text->toPlainText();
        whenModal<QDialog>([](QDialog* add) { add->reject(); });
        child<QToolButton>(dialog.get(), "addAnim")->click();
        QCOMPARE(text->toPlainText(), before);
        child<QComboBox>(dialog.get(), "projs")->setCurrentIndex(1);
        whenModal<QDialog>([](QDialog* add) {
            add->findChild<QSpinBox*>("number")->setValue(830);
            add->findChild<QLineEdit*>("name")->setText("Tail");
            add->accept();
        });
        child<QToolButton>(dialog.get(), "addAnim")->click();
        whenModal<QDialog>([](QDialog* add) {
            add->findChild<QSpinBox*>("number")->setValue(831);
            add->accept();
        });
        child<QToolButton>(dialog.get(), "addAnim")->click();
        QCOMPARE(text->toPlainText(), before + QStringLiteral("; Tail\n[Begin Action 830]\n-1,0, 0,0, -1\n"
                                                              "[Begin Action 831]\n-1,0, 0,0, -1\n"));
        QCOMPARE(otherFile->document()->toPlainText(), otherText);
        child<QComboBox>(dialog.get(), "projs")->setCurrentIndex(0);
        auto* thrown = child<QComboBox>(dialog.get(), "p2Anim");
        thrown->setCurrentIndex(thrown->findText(QStringLiteral("820 - Thrown")));

        Mugen::AnimationPreview* preview = dialog->preview();
        auto* slider = child<QSlider>(dialog.get(), "horizontalSlider");
        auto* group = child<QSpinBox>(dialog.get(), "grp");
        slider->setValue(2);
        QCOMPARE(group->value(), 5010);
        const QList<Mugen::AnimationPreview::Figure> figures = preview->figures();
        QCOMPARE(figures.size(), 2);
        QCOMPARE(figures[0].cross, QColor(Qt::white));
        const QColor p2Cross = figures[1].cross;
        QVERIFY(p2Cross != figures[0].cross);
        child<QSpinBox>(dialog.get(), "bindPriority")->setValue(-1);
        QCOMPARE(preview->figures()[1].cross, p2Cross);
        child<QComboBox>(dialog.get(), "p1AnimStart")->setCurrentIndex(1);
        QCOMPARE(preview->figures().size(), 2);
        QCOMPARE(preview->figures()[0].position, figures[0].position);
        group->setValue(-1);
        QVERIFY(text->toPlainText().contains(QStringLiteral("\n-1,0, 0,0, 5\n")));
        QVERIFY(preview->figures().size() < 2 || preview->figures()[1].image.isNull());
        group->setValue(5010);

        auto* elements = child<QSpinBox>(dialog.get(), "elem");
        elements->setValue(2);
        QCOMPARE(slider->value(), 1);
        QCOMPARE(group->value(), 5000);
        elements->setValue(5);
        slider->setValue(4);
        QCOMPARE(group->value(), 5010);
        QVERIFY(group->isEnabled());
        QCOMPARE(preview->figures()[0].position, figures[0].position);
        group->setValue(5011);
        QVERIFY(text->toPlainText().contains(QStringLiteral("\n5011,0, 0,0, 5\n")));
        group->setValue(5010);
        elements->setValue(0);
        QCOMPARE(group->value(), 5000);
        auto* throwing = child<QComboBox>(dialog.get(), "p1Anim");
        throwing->setCurrentIndex(throwing->findText(QStringLiteral("0")));
        QCOMPARE(elements->value(), 1);
        throwing->setCurrentIndex(throwing->findText(QStringLiteral("810 - Throw")));
        QCOMPARE(elements->value(), 3);
        child<QComboBox>(dialog.get(), "p1AnimStart")->setCurrentIndex(0);
        QCOMPARE(elements->value(), 3);

        dialog->show();
        QVERIFY(QTest::qWaitForWindowExposed(dialog.get()));
        const QSize at640 = dialog->size();
        res->setCurrentText("320x240");
        QVERIFY(dialog->height() < at640.height());
        res->setCurrentText("1920x1080");
        const QRect screen = dialog->screen()->availableGeometry();
        QVERIFY(screen.contains(dialog->frameGeometry()) ||
                dialog->frameGeometry().size().boundedTo(screen.size()) ==
                    screen.size().boundedTo(dialog->frameGeometry().size()));
        res->setCurrentText("640x480");
        QCOMPARE(preview->origin().x(), 320.0);

        group->setValue(5012);
        QTest::keyClick(dialog.get(), Qt::Key_Escape);
        QVERIFY(text->toPlainText().contains(QStringLiteral("\n5012,0, 0,0, 5\n")));

        dialog = std::make_unique<Mugen::ThrowCreatorDialog>(thrower.get(), QList<Project*>{thrower.get(), other.get()},
                                                             throwScript());
        QCOMPARE(child<QDoubleSpinBox>(dialog.get(), "bindX")->value(), 75.0);
        QCOMPARE(child<QSpinBox>(dialog.get(), "bindPriority")->value(), 0);
        QCOMPARE(child<QComboBox>(dialog.get(), "res")->currentText(), QStringLiteral("640x480"));
        QCOMPARE(child<QSpinBox>(dialog.get(), "elem")->value(), 3);
        QCOMPARE(child<QSlider>(dialog.get(), "horizontalSlider")->value(), 0);

        child<QSlider>(dialog.get(), "horizontalSlider")->setValue(2);
        const QString beforeRemove = text->toPlainText();
        child<QToolButton>(dialog.get(), "delFrame")->click();
        QVERIFY(text->toPlainText() != beforeRemove);
        text->undo();
        QCOMPARE(text->toPlainText(), beforeRemove);
    }

    void offsetViewerResets()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("v.def"),
                  "[Info]\nname = \"V\"\nmugenversion = 1.0\n[Files]\ncns = v.cns\nanim = v.air\n");
        writeFile(dir.filePath("v.cns"), "[Data]\nlife = 1000\n");
        writeFile(dir.filePath("v.air"), "[Begin Action 0]\n0,0, 0,0, 5\n0,1, 0,0, 5\n");
        Mugen::MugenEngine engine;
        QString error;
        std::unique_ptr<Project> project(engine.openProject(dir.filePath("v.def"), &error));
        QVERIFY2(project, qPrintable(error));
        {
            Mugen::OffsetViewerDialog viewer(project.get(), {project.get()});
            child<QDoubleSpinBox>(&viewer, "xoff")->setValue(12);
            child<QCheckBox>(&viewer, "fliph")->setChecked(false);
            child<QComboBox>(&viewer, "res")->setCurrentText("320x240");
            child<QComboBox>(&viewer, "frame1")->setCurrentIndex(1);
        }
        Mugen::OffsetViewerDialog viewer(project.get(), {project.get()});
        QCOMPARE(viewer.windowTitle(), QStringLiteral("Offset viewer"));
        QCOMPARE(child<QDoubleSpinBox>(&viewer, "xoff")->value(), 100.0);
        QCOMPARE(child<QDoubleSpinBox>(&viewer, "yoff")->value(), 0.0);
        QVERIFY(child<QCheckBox>(&viewer, "fliph")->isChecked());
        QVERIFY(!child<QCheckBox>(&viewer, "flipv")->isChecked());
        QCOMPARE(child<QComboBox>(&viewer, "res")->currentText(), QStringLiteral("640x480"));
        QCOMPARE(child<QComboBox>(&viewer, "frame1")->currentIndex(), 0);
    }

    void addAnimationDialog()
    {
        Mugen::AddAnimationDialog dialog({0, 5, 6}, 5);
        QCOMPARE(dialog.number(), 7);
        QCOMPARE(dialog.time(), 0);
        QCOMPARE(dialog.findChild<QSpinBox*>("number")->minimum(), -300000);
        dialog.findChild<QSpinBox*>("number")->setValue(6);
        bool warned = false;
        QTimer::singleShot(0, this, [&warned] {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            QVERIFY(box);
            QVERIFY(box->text().contains("6"));
            warned = true;
            box->accept();
        });
        dialog.accept();
        QVERIFY(warned);
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
    }

    void throwCreatorKfm()
    {
        // Set OFS_GRAB_DIR to save a picture of the dialog.
        const QString kfm = qEnvironmentVariable("OFS_CORPUS_KFM");
        if (kfm.isEmpty() || !QFileInfo::exists(kfm + "/kfm.def"))
            QSKIP("OFS_CORPUS_KFM is not set");
        Mugen::MugenEngine engine;
        QString error;
        std::unique_ptr<Project> project(engine.openProject(kfm + "/kfm.def", &error));
        QVERIFY2(project, qPrintable(error));
        Mugen::ThrowCreatorDialog dialog(project.get(), {project.get()}, throwScript());
        QCOMPARE(child<QComboBox>(&dialog, "p1AnimStart")->currentText(),
                 QStringLiteral("800 - Kung Fu Throw Attempt"));
        QCOMPARE(child<QComboBox>(&dialog, "p1Anim")->currentText(), QStringLiteral("810 - Kung Fu Throw"));
        QCOMPARE(child<QComboBox>(&dialog, "p2Anim")->currentText(),
                 QStringLiteral("820 - Thrown by Kung Fu Throw (animation for opponent)"));
        QCOMPARE(child<QSpinBox>(&dialog, "elem")->value(), 15);
        QCOMPARE(child<QSpinBox>(&dialog, "grp")->value(), 5000);
        child<QSlider>(&dialog, "horizontalSlider")->setValue(1);
        QCOMPARE(child<QSpinBox>(&dialog, "grp")->value(), 5010);
        child<QSlider>(&dialog, "horizontalSlider")->setValue(0);
        QVERIFY(!dialog.preview()->figures()[0].image.isNull());
        QVERIFY(!dialog.preview()->figures()[1].image.isNull());
        const QString grab = qEnvironmentVariable("OFS_GRAB_DIR");
        if (!grab.isEmpty()) {
            dialog.show();
            QVERIFY(QTest::qWaitForWindowExposed(&dialog));
            dialog.grab().save(grab + "/throw-creator.png");
        }
    }
    void offsetViewer()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("a.def"), "[Info]\nname = \"A\"\nmugenversion = 1.0\n"
                                         "[Files]\ncns = a.cns\nanim = a.air\n");
        writeFile(dir.filePath("a.cns"), "[Data]\nlife = 1000\n");
        writeFile(dir.filePath("a.air"), "; Standing\n[Begin Action 0]\n0,0, 0,0, 5\n0,1, 3,-2, 5, H\n\n"
                                         "; Walk\n[Begin Action 20]\n20,0, 0,0, 5\n");
        writeFile(dir.filePath("b.def"), "[Info]\nname = \"B\"\nlocalcoord = 640,480\nmugenversion = 1.1\n"
                                         "[Files]\ncns = a.cns\nanim = b.air\n");
        writeFile(dir.filePath("b.air"), "[Begin Action 5]\n5,0, 0,0, 5\n");
        Mugen::MugenEngine engine;
        QString error;
        std::unique_ptr<Project> a(engine.openProject(dir.filePath("a.def"), &error));
        QVERIFY2(a, qPrintable(error));
        std::unique_ptr<Project> b(engine.openProject(dir.filePath("b.def"), &error));
        QVERIFY2(b, qPrintable(error));

        Mugen::OffsetViewerDialog dialog(a.get(), {a.get(), b.get()});
        QCOMPARE(dialog.objectName(), QStringLiteral("OffsetDlg"));
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Offset viewer"));
        QCOMPARE(child<QComboBox>(&dialog, "res")->currentText(), QStringLiteral("640x480"));
        QVERIFY(child<QCheckBox>(&dialog, "fliph")->isChecked());
        QVERIFY(!child<QCheckBox>(&dialog, "flipv")->isChecked());
        QCOMPARE(child<QComboBox>(&dialog, "anim1")->currentText(), QStringLiteral("0 - Standing"));
        QCOMPARE(child<QComboBox>(&dialog, "frame1")->count(), 2);
        QCOMPARE(child<QDoubleSpinBox>(&dialog, "xoff")->value(), 100.0);
        QCOMPARE(child<QDoubleSpinBox>(&dialog, "xoff")->minimum(), -65535.0);
        QCOMPARE(child<QComboBox>(&dialog, "projs")->currentText(), QStringLiteral("A"));
        QCOMPARE(child<QComboBox>(&dialog, "anim2")->currentText(), QStringLiteral("0 - Standing"));

        Mugen::AnimationPreview* preview = dialog.preview();
        QCOMPARE(preview->figures().size(), 2);
        QCOMPARE(preview->figures()[0].position, QPointF(0, 0));
        QCOMPARE(preview->figures()[1].position, QPointF(100, 0));
        QVERIFY(preview->figures()[1].flipH);
        QVERIFY(!preview->figures()[1].flipV);
        child<QDoubleSpinBox>(&dialog, "yoff")->setValue(-20);
        child<QCheckBox>(&dialog, "flipv")->setChecked(true);
        QCOMPARE(preview->figures()[1].position, QPointF(100, -20));
        QVERIFY(preview->figures()[1].flipV);
        child<QCheckBox>(&dialog, "fliph")->setChecked(false);
        QVERIFY(!preview->figures()[1].flipH);

        child<QComboBox>(&dialog, "frame1")->setCurrentIndex(1);
        QCOMPARE(preview->figures()[0].position, QPointF(6, -4));
        QVERIFY(preview->figures()[0].flipH);
        child<QCheckBox>(&dialog, "fliph")->setChecked(true);
        child<QCheckBox>(&dialog, "flipv")->setChecked(false);
        child<QComboBox>(&dialog, "frame2")->setCurrentIndex(1);
        QCOMPARE(preview->figures()[1].position, QPointF(100 - 6, -20 - 4));
        QVERIFY(!preview->figures()[1].flipH);

        child<QComboBox>(&dialog, "projs")->setCurrentIndex(1);
        QCOMPARE(child<QComboBox>(&dialog, "anim2")->currentText(), QStringLiteral("5"));
        QCOMPARE(child<QComboBox>(&dialog, "frame2")->count(), 1);
        QCOMPARE(preview->figures()[1].scale, 1.0);
        QCOMPARE(preview->figures()[0].scale, 2.0);
        child<QComboBox>(&dialog, "anim1")->setCurrentIndex(1);
        QCOMPARE(child<QComboBox>(&dialog, "frame1")->count(), 1);
    }

    void offsetViewerKfm()
    {
        const QString kfm = qEnvironmentVariable("OFS_CORPUS_KFM");
        if (kfm.isEmpty() || !QFileInfo::exists(kfm + "/kfm.def"))
            QSKIP("OFS_CORPUS_KFM is not set");
        Mugen::MugenEngine engine;
        QString error;
        std::unique_ptr<Project> project(engine.openProject(kfm + "/kfm.def", &error));
        QVERIFY2(project, qPrintable(error));
        Mugen::OffsetViewerDialog dialog(project.get(), {project.get()});
        QCOMPARE(child<QComboBox>(&dialog, "anim1")->currentText(), QStringLiteral("0 - Standing Animation"));
        QCOMPARE(child<QComboBox>(&dialog, "projs")->currentText(), QStringLiteral("Kung Fu Man"));
        QVERIFY(!dialog.preview()->figures()[0].image.isNull());
        QVERIFY(!dialog.preview()->figures()[1].image.isNull());
        const QString grab = qEnvironmentVariable("OFS_GRAB_DIR");
        if (!grab.isEmpty()) {
            dialog.show();
            QVERIFY(QTest::qWaitForWindowExposed(&dialog));
            dialog.grab().save(grab + "/offset-viewer.png");
        }
    }

    void palfx()
    {
        const QString templates = sourceFile(QStringLiteral("../data/engines/mugen/palFX"));
        PalFX fire;
        QVERIFY(readPalFXTemplate(templates + "/Fire.xml", &fire));
        QCOMPARE(fire.add[0], 150);
        QCOMPARE(fire.mul[2], 120);
        QCOMPARE(fire.period, 8);
        QCOMPARE(fire.color, 256);
        QVERIFY(!fire.invert);
        // MUGEN's PalFX: the weighted grey, then (colour + add) * mul / 256.
        const PalFX none;
        QCOMPARE(none.apply(qRgb(10, 100, 200), 0), qRgb(10, 100, 200));
        PalFX inverted;
        inverted.invert = true;
        QCOMPARE(inverted.apply(qRgb(10, 100, 200), 0), qRgb(245, 155, 55));
        PalFX gray;
        gray.color = 0;
        QCOMPARE(gray.apply(qRgb(30, 60, 90), 0), qRgb(54, 54, 54));
        PalFX added;
        added.add[0] = 10;
        added.mul[0] = 128;
        added.mul[1] = -50; // counts as 0
        QCOMPARE(added.apply(qRgb(100, 100, 100), 0), qRgb(55, 0, 100));
        PalFX values;
        values.add[0] = 240;
        values.add[1] = 50;
        values.mul[0] = 250;
        values.mul[1] = 224;
        values.mul[2] = 120;
        values.sin[0] = 110;
        values.sin[1] = 55;
        values.sin[2] = 85;
        values.period = 10;
        values.color = 0;
        QCOMPARE(values.controllerText(), QStringLiteral("[State 0, PalFXWiz]\ntype = PalFX\ntrigger1 = \ntime = \n"
                                                         "add = 240,50,0\nmul = 250,224,120\nsinadd = 110,55,85,10\n"
                                                         "invertall = 0\ncolor = 0\n"));

        PalFXDialog dialog(nullptr, 0, templates);
        auto* list = dialog.findChild<QListWidget*>("templates");
        QVERIFY(list);
        QCOMPARE(list->count(), 12);
        QListWidgetItem* item = list->findItems("Fire", Qt::MatchExactly).value(0);
        QVERIFY(item);
        Q_EMIT list->itemDoubleClicked(item);
        QCOMPARE(dialog.values().add[0], 150);
        QCOMPARE(dialog.findChild<QSpinBox*>("addR")->value(), 150);
        QString inserted;
        dialog.setInsertHandler([&inserted](const QString& text) { inserted = text; });
        const QList<QPushButton*> buttons = dialog.findChildren<QPushButton*>();
        for (QPushButton* button : buttons) {
            if (button->text() == QStringLiteral("Insert in the editor"))
                button->click();
        }
        QVERIFY(inserted.startsWith("[State 0, PalFXWiz]\ntype = PalFX\n"));
        QVERIFY(inserted.contains("add = 150,50,-40\n"));
    }
};

QTEST_MAIN(MugenToolsTest)
#include "MugenToolsTest.moc"
