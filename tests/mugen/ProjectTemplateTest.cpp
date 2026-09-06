#include "core/engine/EngineManager.h"
#include "core/project/Project.h"
#include "core/script/ScriptHost.h"
#include "engines/mugen/MugenEngine.h"
#include "shell/dialogs/ProjectDialogs.h"

#include <QApplication>
#include <QFile>
#include <QListWidget>
#include <QRadioButton>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QWizard>

#include <memory>

using namespace OFS;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

void answerWizard(const QString& choice, bool finish, QStringList* seen = nullptr)
{
    QTimer::singleShot(0, qApp, [choice, finish, seen] {
        auto* wizard = qobject_cast<QWizard*>(QApplication::activeModalWidget());
        QVERIFY(wizard);
        if (seen) {
            *seen << wizard->windowTitle() << wizard->currentPage()->title() << wizard->currentPage()->subTitle();
            for (const QRadioButton* radio : wizard->currentPage()->findChildren<QRadioButton*>())
                *seen << radio->text() + (radio->isChecked() ? "*" : "");
        }
        for (QRadioButton* radio : wizard->currentPage()->findChildren<QRadioButton*>()) {
            if (radio->text() == choice)
                radio->setChecked(true);
        }
        if (finish)
            wizard->button(QWizard::FinishButton)->click();
        else
            wizard->reject();
    });
}

} // namespace

class ProjectTemplateTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        m_engines.addEngine(new Mugen::MugenEngine);
        QVERIFY(m_folder.isValid());
        m_environment.engines = &m_engines;
        m_environment.projectFolder = m_folder.path();
    }

    void shippedEmptyTemplate()
    {
        const QList<ProjectTemplate> templates = findProjectTemplates(m_engines);
        QVERIFY(!templates.isEmpty());
        const ProjectTemplate empty = templates.first();
        QCOMPARE(empty.category, QStringLiteral("Basic"));
        QCOMPARE(empty.name, QStringLiteral("Empty"));
        QCOMPARE(empty.engine, QStringLiteral("mugen"));
        QVERIFY(empty.icon.endsWith("Empty.svg"));

        QStringList seen;
        answerWizard("Font", true, &seen);
        QString error;
        std::unique_ptr<Project> project(Script::runProjectTemplate(empty.script, m_environment, &error));
        QVERIFY2(project, qPrintable(error));
        QCOMPARE(seen, (QStringList{"Empty project", "Empty project", "Choose the type of the new blank project",
                                    "Character*", "Stage", "Font", "Storyboard"}));
        QCOMPARE(project->typeId(), QStringLiteral("font"));
        QVERIFY(project->isMemoryOnly());

        answerWizard(QString(), true);
        project.reset(Script::runProjectTemplate(empty.script, m_environment, &error));
        QVERIFY2(project, qPrintable(error));
        QCOMPARE(project->typeId(), QStringLiteral("char"));
        QCOMPARE(project->name(), QStringLiteral("Char name"));

        answerWizard(QString(), false);
        project.reset(Script::runProjectTemplate(empty.script, m_environment, &error));
        QVERIFY(!project);
        QVERIFY(error.isEmpty());
    }

    void scriptApi()
    {
        QTemporaryDir dir;
        const QString script = dir.filePath("t.lua");
        writeFile(script, "assert(ScriptPath ~= nil and ScriptPath:sub(-1) ~= '/')\n"
                          "assert(OFS == FF)\n"
                          "local w = FF.wizard()\n"
                          "local p = FF.wizardPage()\n"
                          "local grid = FF.gridLayout(p)\n"
                          "local name = FF.lineEdit(p)\n"
                          "name.text = 'abc'\n"
                          "local kind = FF.comboBox(p)\n"
                          "kind:addItem('one'); kind:addItem('two'); kind.currentIndex = 1\n"
                          "local spin = FF.spinBox(p)\n"
                          "spin.maximum = 50; spin.value = 70\n"
                          "local check = FF.checkBox('Check', p)\n"
                          "check.checked = true\n"
                          "grid:addWidget(FF.label('Name', p), 0, 0)\n"
                          "grid:addWidget(name, 0, 1)\n"
                          "grid:addWidget(kind, 1, 0, 1, 2)\n"
                          "w:addPage(p)\n"
                          "assert(name.text == 'abc' and kind.currentText == 'two' and kind.count == 2)\n"
                          "assert(spin.value == 50 and check.checked and w.enabled)\n"
                          "if w:exec() == 0 then return nil end\n"
                          "local project = FF.emptyProject('MUGEN', MUGEN.ProjectType.Stage)\n"
                          "assert(project.type == MUGEN.ProjectType.Stage and project.engine.name == 'mugen')\n"
                          "assert(project.name == 'Stage name' and project.version ~= nil)\n"
                          "local unused = FF.emptyProject('mugen', MUGEN.ProjectType.Character)\n"
                          "return project\n");
        answerWizard(QString(), true);
        QString error;
        std::unique_ptr<Project> project(Script::runProjectTemplate(script, m_environment, &error));
        QVERIFY2(project, qPrintable(error));
        QCOMPARE(project->typeId(), QStringLiteral("stage"));
        QVERIFY(!QApplication::activeModalWidget());

        writeFile(dir.filePath("tpl.def"), "[Info]\nname = \"Copied\"\nmugenversion = 1.0\n[Files]\ncmd = tpl.cmd\n");
        writeFile(script, "return FF.projectFromFile('tpl.def', true)\n");
        project.reset(Script::runProjectTemplate(script, m_environment, &error));
        QVERIFY2(project, qPrintable(error));
        QCOMPARE(project->name(), QStringLiteral("Copied"));
        QVERIFY(project->isMemoryOnly());
    }

    void scriptErrors()
    {
        QTemporaryDir dir;
        const QString script = dir.filePath("bad.lua");
        QString error;
        writeFile(script, "error('boom')\n");
        QVERIFY(!Script::runProjectTemplate(script, m_environment, &error));
        QVERIFY(error.contains("boom"));
        writeFile(script, "return 5\n");
        QVERIFY(!Script::runProjectTemplate(script, m_environment, &error));
        QVERIFY(error.contains("did not return a project"));
        writeFile(script, "return FF.emptyProject('nothing', 'char')\n");
        QVERIFY(!Script::runProjectTemplate(script, m_environment, &error));
        QVERIFY(error.isEmpty());
        writeFile(script, "return FF.emptyProject('mugen', 'select')\n");
        QVERIFY(!Script::runProjectTemplate(script, m_environment, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!Script::runProjectTemplate(dir.filePath("missing.lua"), m_environment, &error));
        QVERIFY(!error.isEmpty());
    }

    void templateWindow()
    {
        ProjectTemplateDialog dialog(m_engines);
        QCOMPARE(dialog.objectName(), QStringLiteral("ProjDlg"));
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Project template"));
        auto* tabs = dialog.findChild<QTabWidget*>("tabs");
        QVERIFY(tabs);
        QCOMPARE(tabs->tabText(0), QStringLiteral("Basic"));
        auto* list = qobject_cast<QListWidget*>(tabs->widget(0));
        QVERIFY(list);
        QCOMPARE(list->item(0)->text(), QStringLiteral("Empty (M.U.G.E.N)"));
        QVERIFY(dialog.selectedTemplate().script.isEmpty());
        list->setCurrentRow(0);
        QCOMPARE(dialog.selectedTemplate().name, QStringLiteral("Empty"));
    }

private:
    EngineManager m_engines;
    QTemporaryDir m_folder;
    Script::Environment m_environment;
};

QTEST_MAIN(ProjectTemplateTest)
#include "ProjectTemplateTest.moc"
