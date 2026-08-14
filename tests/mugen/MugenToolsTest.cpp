#include "core/project/Project.h"
#include "core/script/LuaScript.h"
#include "engines/mugen/AirFile.h"
#include "engines/mugen/MugenEngine.h"
#include "engines/mugen/ProjectGraphics.h"
#include "engines/mugen/ThrowCreator.h"
#include "engines/mugen/dialogs/AnimationPreview.h"
#include "engines/mugen/dialogs/OffsetViewerDialog.h"
#include "engines/mugen/dialogs/ThrowCreatorDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>

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
    return sourceFile(QStringLiteral("../data/engines/mugen/scripts/throw.lua"));
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
    void throwCreatorDialog()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("thrower.def"), "[Info]\nname = \"Thrower\"\nmugenversion = 1.0\n"
                                               "[Files]\ncns = thrower.cns\nsprite = t.sff\nanim = thrower.air\n");
        writeFile(dir.filePath("thrower.cns"), "[Data]\nlife = 1000\n");
        writeFile(dir.filePath("thrower.air"), "[Begin Action 0]\n0,0, 0,0, -1\n\n"
                                               "; Throw\n[Begin Action 810]\n810,0, 0,0, 5\n810,1, 6,0, 5\n\n"
                                               "; Thrown\n[Begin Action 820]\n5000,0, 0,0, 5\n5010,0, 4,0, 5, V\n");
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
        QCOMPARE(child<QSpinBox>(&dialog, "elem")->value(), 2);
        QCOMPARE(child<QComboBox>(&dialog, "projs")->count(), 2);
        QCOMPARE(child<QComboBox>(&dialog, "projs")->currentText(), QStringLiteral("Thrower"));

        Mugen::AnimationPreview* preview = dialog.preview();
        auto* slider = child<QSlider>(&dialog, "horizontalSlider");
        slider->setValue(1);
        QCOMPARE(child<QSpinBox>(&dialog, "grp")->value(), 5010);
        QCOMPARE(child<QComboBox>(&dialog, "flipList")->currentText(), QStringLiteral("Flip V"));
        QCOMPARE(preview->figures().size(), 2);
        QCOMPARE(preview->figures()[0].position, QPointF(12, 0));
        QCOMPARE(preview->figures()[0].scale, 2.0);
        QCOMPARE(preview->figures()[1].position, QPointF(75 * 2 - 4 * 2, 0));
        QVERIFY(preview->figures()[1].flipH);
        QVERIFY(preview->figures()[1].flipV);
        child<QComboBox>(&dialog, "res")->setCurrentText("1280x720");
        QCOMPARE(preview->figures()[1].position, QPointF(75 * 4 - 4 * 4, 0));
        child<QComboBox>(&dialog, "res")->setCurrentText("640x480");

        child<QComboBox>(&dialog, "projs")->setCurrentIndex(1);
        QCOMPARE(child<QComboBox>(&dialog, "p2Anim")->currentText(), QStringLiteral("7 - Big thrown"));
        slider->setValue(0);
        QCOMPARE(child<QSpinBox>(&dialog, "grp")->value(), 1);
        QCOMPARE(preview->figures()[1].scale, 1.0);
        QCOMPARE(preview->figures()[1].position, QPointF(75 * 2 - 2, 0));

        child<QDoubleSpinBox>(&dialog, "bindX")->setValue(10);
        child<QSpinBox>(&dialog, "elem")->setValue(4);
        QCOMPARE(slider->maximum(), 3);
        slider->setValue(3);
        QCOMPARE(child<QDoubleSpinBox>(&dialog, "bindX")->value(), 75.0);
        const Mugen::ThrowSettings settings = dialog.settings();
        QCOMPARE(settings.binds.size(), 4);
        QCOMPARE(settings.binds[0].x, 10.0);
        QCOMPARE(settings.startAnim, 0);
        QCOMPARE(settings.throwAnim, 810);
        QCOMPARE(settings.thrownAnim, 7);

        QString inserted;
        dialog.setInsertHandler([&inserted](const QString& text) { inserted = text; });
        child<QPushButton>(&dialog, "ins")->click();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(inserted, Mugen::throwStates(throwScript(), settings));
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
};

QTEST_MAIN(MugenToolsTest)
#include "MugenToolsTest.moc"
