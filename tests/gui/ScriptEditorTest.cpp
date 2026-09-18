// Tools > Script editor.
#include "core/app/AppContext.h"
#include "core/app/Paths.h"
#include "core/code/CodeEditor.h"
#include "core/engine/EngineManager.h"
#include "core/settings/Config.h"
#include "core/theme/ThemeManager.h"
#include "engines/mugen/MugenEngine.h"
#include "shell/dialogs/ScriptEditorDialog.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QListView>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringListModel>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolBar>

using namespace OFS;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(data);
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// Answers the next message boxes in turn, noting their titles and texts.
void answerBoxes(QList<QMessageBox::StandardButton> buttons, QStringList* seen)
{
    auto* timer = new QTimer(qApp);
    timer->setInterval(5);
    QObject::connect(timer, &QTimer::timeout, qApp, [timer, buttons, seen]() mutable {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (!box || !box->isVisible())
            return;
        if (seen)
            *seen << box->windowTitle() + QLatin1Char('|') + box->text();
        box->button(buttons.takeFirst())->click();
        if (buttons.isEmpty()) {
            timer->stop();
            timer->deleteLater();
        }
    });
    timer->start();
}

QStringList comboItems(const QComboBox* combo)
{
    QStringList items;
    for (int i = 0; i < combo->count(); ++i)
        items << combo->itemText(i);
    return items;
}

} // namespace

class ScriptEditorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        m_context.engines().addEngine(new Mugen::MugenEngine);
        QVERIFY(m_data.isValid());
        writeFile(m_data.filePath("scripts/tool/Quick action script.lua"), "function main()\nend\n");
        writeFile(m_data.filePath("engines/mugen/scripts/common/Throw.lua"), "return ''\n");
        writeFile(m_data.filePath("engines/mugen/scripts/project/Basic/Empty.lua"), "return nil\n");
        m_environment.engines = &m_context.engines();
    }

    void window()
    {
        m_context.theme().setSearchDirs({Paths::dataDir()});
        QVERIFY(m_context.theme().setColorSet(QStringLiteral("Light")));
        ScriptEditorDialog dialog(m_context, m_environment, m_data.path());
        const ColorSet& colors = m_context.theme().colorSet();
        QCOMPARE(dialog.editor()->palette().color(QPalette::Base), colors.color(QStringLiteral("background")));
        QCOMPARE(dialog.editor()->palette().color(QPalette::Text), colors.color(QStringLiteral("foreground")));
        QCOMPARE(dialog.objectName(), QStringLiteral("ScriptEd"));
        QVERIFY(dialog.windowTitle().startsWith("Script Editor"));
        QStringList tips;
        for (QAction* action : dialog.toolBar()->actions())
            tips << action->toolTip();
        QCOMPARE(tips, (QStringList{"New", "Open...", "Save", "Save as...", "Run", "Save for translation"}));
        QCOMPARE(comboItems(dialog.engineFilter()), (QStringList{"All engines", "MUGENext"}));
        QCOMPARE(comboItems(dialog.typeFilter()), (QStringList{"Common", "Project", "Code snippet", "Tool"}));
        QCOMPARE(dialog.typeFilter()->currentText(), QStringLiteral("Tool"));
        QCOMPARE(dialog.messageView()->toolTip(), QStringLiteral("Message output"));
        QCOMPARE(dialog.output()->toolTip(), QStringLiteral("Text output (debug)"));
        QVERIFY(dialog.output()->isReadOnly());

        QCOMPARE(dialog.scriptNames()->stringList(), QStringList{"Quick action script"});
        dialog.engineFilter()->setCurrentIndex(1);
        dialog.typeFilter()->setCurrentIndex(0);
        QCOMPARE(dialog.scriptNames()->stringList(), QStringList{"Throw"});
        dialog.typeFilter()->setCurrentIndex(1);
        QCOMPARE(dialog.scriptNames()->stringList(), QStringList{"Basic/Empty"});
        QCOMPARE(QDir::cleanPath(dialog.selectedFolder()),
                 QDir::cleanPath(m_data.filePath("engines/mugen/scripts/project")));
    }

    void editSaveAndRun()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("one.lua");
        writeFile(path, "-- one\ndebugOutput('hi')\n");
        ScriptEditorDialog dialog(m_context, m_environment, m_data.path());
        QVERIFY(dialog.openFile(path));
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Script Editor - one.lua[*]"));
        QVERIFY(!dialog.isWindowModified());

        dialog.editor()->moveCursor(QTextCursor::End);
        dialog.editor()->insertPlainText("function main() print('main', 2) end\n");
        QVERIFY(dialog.isWindowModified());

        dialog.run();
        QCOMPARE(dialog.output()->toPlainText(), QStringLiteral("himain\t2\n"));
        QVERIFY(dialog.messages()->stringList().isEmpty());

        QVERIFY(dialog.save());
        QVERIFY(!dialog.isWindowModified());
        QCOMPARE(readFile(path), QByteArray("-- one\r\ndebugOutput('hi')\r\nfunction main() print('main', 2) end\r\n"));
        const QString other = dir.filePath("two.lua");
        QVERIFY(dialog.saveAs(other));
        QCOMPARE(dialog.fileName(), QFileInfo(other).absoluteFilePath());
        QVERIFY(dialog.windowTitle().contains("two.lua"));
    }

    void errors()
    {
        ScriptEditorDialog dialog(m_context, m_environment, m_data.path());
        dialog.editor()->setPlainText("-- broken\nlocal t = nil\nlocal x = t.y\n");
        QStringList seen;
        answerBoxes({QMessageBox::Ok}, &seen);
        dialog.run();
        QCOMPARE(seen.size(), 1);
        QVERIFY2(seen[0].startsWith("Script error!|sol: runtime error:"), qPrintable(seen[0]));
        const QStringList messages = dialog.messages()->stringList();
        QVERIFY(!messages.isEmpty());
        QVERIFY2(messages[0].startsWith("Line 3: attempt to index a nil value"), qPrintable(messages[0]));
        Q_EMIT dialog.messageView()->doubleClicked(dialog.messages()->index(0));
        QCOMPARE(dialog.editor()->cursorLine(), 3);

        dialog.editor()->setPlainText("function main() error('late') end\n");
        seen.clear();
        answerBoxes({QMessageBox::Ok}, &seen);
        dialog.run();
        QCOMPARE(seen.size(), 1);
        QVERIFY(seen[0].contains("late"));
    }

    void stop()
    {
        ScriptEditorDialog dialog(m_context, m_environment, m_data.path());
        dialog.editor()->setPlainText("while true do end\n");
        QTimer::singleShot(100, &dialog, [&dialog] {
            QVERIFY(dialog.isRunning());
            QCOMPARE(dialog.runAction()->toolTip(), QStringLiteral("Stop"));
            dialog.runAction()->trigger();
        });
        dialog.run();
        QVERIFY(!dialog.isRunning());
        QCOMPARE(dialog.runAction()->toolTip(), QStringLiteral("Run"));
        QCOMPARE(dialog.messages()->stringList(), QStringList{"The script was stopped."});
    }

    void unsavedChanges()
    {
        ScriptEditorDialog dialog(m_context, m_environment, m_data.path());
        dialog.editor()->insertPlainText("x = 1\n");
        QStringList seen;
        answerBoxes({QMessageBox::Cancel}, &seen);
        dialog.newScript();
        QCOMPARE(dialog.editor()->toPlainText(), QStringLiteral("x = 1\n"));
        answerBoxes({QMessageBox::No}, &seen);
        dialog.newScript();
        QVERIFY(dialog.editor()->toPlainText().isEmpty());
        QCOMPARE(seen.size(), 2);
        QVERIFY(seen[0].startsWith("Script Editor|"));
    }

    void translation()
    {
        QTemporaryDir dir;
        ScriptEditorDialog dialog(m_context, m_environment, m_data.path());
        dialog.editor()->setPlainText("local a = FF.translate(\"Hello\")\nlocal b = FF.translate('World')\n");
        QStringList seen;
        answerBoxes({QMessageBox::Ok}, &seen);
        QVERIFY(!dialog.saveForTranslation());
        QCOMPARE(seen, QStringList{"Error!|You must save the script before using this function."});

        QVERIFY(dialog.saveAs(dir.filePath("words.lua")));
        QVERIFY(dialog.saveForTranslation());
        const QByteArray ts = readFile(dir.filePath("words.ts"));
        QVERIFY(ts.contains("<name>words</name>"));
        QVERIFY(ts.contains("<source>Hello</source>"));
        QVERIFY(ts.contains("<source>World</source>"));
        QVERIFY(dialog.messages()->stringList().value(0).startsWith("2 text(s) written to"));
    }

    void geometry()
    {
        {
            ScriptEditorDialog dialog(m_context, m_environment, m_data.path());
            dialog.setGeometry(120, 90, 900, 640);
            dialog.reject();
        }
        QCOMPARE(m_context.config().value("script_geometry"), QStringLiteral("120,90,900,640"));
        ScriptEditorDialog again(m_context, m_environment, m_data.path());
        QCOMPARE(again.geometry(), QRect(120, 90, 900, 640));
    }

private:
    AppContext m_context;
    QTemporaryDir m_data;
    Script::Environment m_environment;
};

QTEST_MAIN(ScriptEditorTest)
#include "ScriptEditorTest.moc"
