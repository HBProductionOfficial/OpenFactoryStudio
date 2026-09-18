#include "shell/dialogs/ScriptEditorDialog.h"

#include "shell/FileDialogs.h"

#include "core/app/AppContext.h"
#include "core/code/CodeEditor.h"
#include "core/code/LuaHighlighter.h"
#include "core/engine/Engine.h"
#include "core/engine/EngineManager.h"
#include "core/script/ScriptFolders.h"
#include "core/script/ScriptTranslation.h"
#include "core/settings/Config.h"
#include "core/theme/ThemeManager.h"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QListView>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSplitter>
#include <QStringListModel>
#include <QToolBar>
#include <QVBoxLayout>

namespace OFS {

namespace {

const QString GeometryKey = QStringLiteral("script_geometry");

QAction* addTool(QToolBar* bar, const QString& id, const QString& text, const QString& tip, const ThemeManager& theme)
{
    QAction* action = bar->addAction(theme.iconSet().icon(id), text);
    action->setObjectName(id);
    action->setToolTip(tip);
    return action;
}

} // namespace

ScriptEditorDialog::ScriptEditorDialog(AppContext& context, const Script::Environment& environment,
                                       const QString& dataDir, QWidget* parent)
    : QDialog(parent)
    , m_context(context)
    , m_environment(environment)
    , m_dataDir(dataDir)
{
    setObjectName(QStringLiteral("ScriptEd"));
    setWindowFlag(Qt::WindowMaximizeButtonHint);
    buildUi();
    refreshList();
    updateTitle();

    // Size and place from last time (client area, logical pixels).
    const QStringList rect = m_context.config().value(GeometryKey).split(QLatin1Char(','));
    if (rect.size() == 4) {
        const QRect geometry(rect[0].toInt(), rect[1].toInt(), rect[2].toInt(), rect[3].toInt());
        if (geometry.width() > 0 && geometry.height() > 0)
            setGeometry(geometry);
    } else {
        resize(1170, 780);
    }
}

ScriptEditorDialog::~ScriptEditorDialog() = default;

void ScriptEditorDialog::buildUi()
{
    const ThemeManager& theme = m_context.theme();
    m_toolBar = new QToolBar(this);
    m_toolBar->setIconSize(QSize(32, 32));
    m_new = addTool(m_toolBar, QStringLiteral("script_new"), tr("New"), tr("New"), theme);
    m_open = addTool(m_toolBar, QStringLiteral("script_open"), tr("Open"), tr("Open..."), theme);
    m_save = addTool(m_toolBar, QStringLiteral("script_save"), tr("Save"), tr("Save"), theme);
    m_saveAs = addTool(m_toolBar, QStringLiteral("script_saveas"), tr("Save as"), tr("Save as..."), theme);
    m_run = addTool(m_toolBar, QStringLiteral("script_run"), tr("Run"), tr("Run"), theme);
    m_translate = addTool(m_toolBar, QStringLiteral("script_translate"), tr("Save for translation"),
                          tr("Save for translation"), theme);

    m_editor = new CodeEditor(this);
    m_highlighter = new LuaHighlighter(m_editor->document());
    m_highlighter->setColorSet(theme.colorSet());
    m_editor->setColorSet(theme.colorSet());
    m_messages = new QStringListModel(this);
    m_messageView = new QListView(this);
    m_messageView->setModel(m_messages);
    m_messageView->setToolTip(tr("Message output"));
    m_messageView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_output = new QPlainTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setToolTip(tr("Text output (debug)"));

    auto* split = new QSplitter(Qt::Vertical, this);
    split->addWidget(m_editor);
    split->addWidget(m_messageView);
    split->addWidget(m_output);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    split->setStretchFactor(2, 2);
    split->setChildrenCollapsible(false);

    m_engines = new QComboBox(this);
    m_engines->addItem(tr("All engines"), QString());
    for (const Engine* engine : m_context.engines().engines())
        m_engines->addItem(engine->displayName(), engine->name());
    m_types = new QComboBox(this);
    m_types->addItem(tr("Common"), int(Script::ScriptType::Common));
    m_types->addItem(tr("Project"), int(Script::ScriptType::Project));
    m_types->addItem(tr("Code snippet"), int(Script::ScriptType::CodeSnippet));
    m_types->addItem(tr("Tool"), int(Script::ScriptType::Tool));
    m_types->setCurrentIndex(3);
    m_listModel = new QStringListModel(this);
    m_list = new QListView(this);
    m_list->setModel(m_listModel);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto* grid = new QGridLayout(this);
    grid->addWidget(m_toolBar, 0, 0, 2, 1, Qt::AlignTop);
    grid->addWidget(m_engines, 0, 1);
    grid->addWidget(m_types, 1, 1);
    grid->addWidget(split, 2, 0);
    grid->addWidget(m_list, 2, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnMinimumWidth(1, 250);
    grid->setRowStretch(2, 1);

    connect(m_new, &QAction::triggered, this, &ScriptEditorDialog::newScript);
    connect(m_open, &QAction::triggered, this, [this] {
        if (!confirmDiscard())
            return;
        const QString start = m_file.isEmpty() ? selectedFolder() : QFileInfo(m_file).absolutePath();
        const QString path = FileDialogs::getOpenFileName(this, tr("Open script"), start, tr("Lua Script (*.lua)"));
        if (!path.isEmpty())
            openFile(path);
    });
    connect(m_save, &QAction::triggered, this, &ScriptEditorDialog::save);
    connect(m_saveAs, &QAction::triggered, this, [this] {
        QString path =
            FileDialogs::getSaveFileName(this, tr("Save script"), selectedFolder(), tr("Lua Script (*.lua)"));
        if (path.isEmpty())
            return;
        if (QFileInfo(path).suffix().isEmpty())
            path += QStringLiteral(".lua");
        saveAs(path);
    });
    connect(m_run, &QAction::triggered, this, &ScriptEditorDialog::run);
    connect(m_translate, &QAction::triggered, this, &ScriptEditorDialog::saveForTranslation);
    connect(m_engines, &QComboBox::currentIndexChanged, this, &ScriptEditorDialog::refreshList);
    connect(m_types, &QComboBox::currentIndexChanged, this, &ScriptEditorDialog::refreshList);
    connect(m_list, &QListView::doubleClicked, this, &ScriptEditorDialog::openListed);
    connect(m_messageView, &QListView::doubleClicked, this, &ScriptEditorDialog::goToMessage);
    connect(m_editor->document(), &QTextDocument::modificationChanged, this, &ScriptEditorDialog::updateTitle);
}

bool ScriptEditorDialog::isModified() const
{
    return m_editor->document()->isModified();
}

QString ScriptEditorDialog::selectedFolder() const
{
    return Script::scriptFolder(m_dataDir, m_engines->currentData().toString(),
                                Script::ScriptType(m_types->currentData().toInt()));
}

void ScriptEditorDialog::refreshList()
{
    m_listModel->setStringList(Script::listScripts(selectedFolder()));
}

void ScriptEditorDialog::updateTitle()
{
    const QString name = m_file.isEmpty() ? tr("Untitled") : QFileInfo(m_file).fileName();
    setWindowTitle(tr("Script Editor") + QStringLiteral(" - ") + name + QStringLiteral("[*]"));
    setWindowModified(isModified());
}

bool ScriptEditorDialog::confirmDiscard()
{
    if (!isModified() || m_editor->document()->isEmpty())
        return true;
    const QMessageBox::StandardButton answer =
        QMessageBox::question(this, tr("Script Editor"), tr("The script was changed. Do you want to save it?"),
                              QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
    if (answer == QMessageBox::Cancel)
        return false;
    return answer == QMessageBox::No || save();
}

void ScriptEditorDialog::newScript()
{
    if (m_running || !confirmDiscard())
        return;
    m_editor->clear();
    m_editor->document()->setModified(false);
    m_file.clear();
    updateTitle();
}

bool ScriptEditorDialog::openFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Open script"), file.errorString());
        return false;
    }
    m_editor->setPlainText(QString::fromUtf8(file.readAll()));
    m_editor->moveCursor(QTextCursor::End);
    m_editor->document()->setModified(false);
    m_file = QFileInfo(path).absoluteFilePath();
    updateTitle();
    return true;
}

bool ScriptEditorDialog::save()
{
    if (m_file.isEmpty()) {
        QString path =
            FileDialogs::getSaveFileName(this, tr("Save script"), selectedFolder(), tr("Lua Script (*.lua)"));
        if (path.isEmpty())
            return false;
        if (QFileInfo(path).suffix().isEmpty())
            path += QStringLiteral(".lua");
        return saveAs(path);
    }
    return saveAs(m_file);
}

bool ScriptEditorDialog::saveAs(const QString& path)
{
    // UTF-8 without a byte order mark, CR LF line ends.
    QSaveFile file(path);
    QString text = m_editor->toPlainText();
    text.replace(QLatin1Char('\n'), QStringLiteral("\r\n"));
    if (!file.open(QIODevice::WriteOnly) || file.write(text.toUtf8()) < 0 || !file.commit()) {
        QMessageBox::warning(this, tr("Save script"), file.errorString());
        return false;
    }
    m_file = QFileInfo(path).absoluteFilePath();
    m_editor->document()->setModified(false);
    updateTitle();
    refreshList();
    return true;
}

void ScriptEditorDialog::setRunning(bool running)
{
    m_running = running;
    for (QAction* action : {m_new, m_open, m_save, m_saveAs, m_translate})
        action->setEnabled(!running);
    m_editor->setReadOnly(running);
    m_engines->setEnabled(!running);
    m_types->setEnabled(!running);
    m_list->setEnabled(!running);
    m_run->setText(running ? tr("Stop") : tr("Run"));
    m_run->setToolTip(running ? tr("Stop") : tr("Run"));
}

void ScriptEditorDialog::run()
{
    if (m_running) {
        m_stopRequested = true;
        return;
    }
    m_output->clear();
    m_messages->setStringList({});
    Script::Environment environment = m_environment;
    environment.parent = this;
    environment.output = [this](const QString& text) {
        m_output->moveCursor(QTextCursor::End);
        m_output->insertPlainText(text);
    };
    environment.stopRequested = [this] {
        return m_stopRequested;
    };
    m_stopRequested = false;
    setRunning(true);
    const Script::RunResult result = Script::runScript(m_editor->toPlainText(), QString(), environment);
    setRunning(false);
    if (result.stopped)
        m_messages->setStringList({tr("The script was stopped.")});
    else if (!result.ok)
        reportError(result.error);
}

void ScriptEditorDialog::reportError(const QString& error)
{
    // Each "...]:<line>: message" of the error goes into the list.
    static const QRegularExpression located(
        QStringLiteral("^\\s*(?:sol: [a-z]+ error: )?\\[string \"[^\"]*\"\\]:(\\d+):\\s*(.*)$"));
    QStringList lines;
    for (const QString& line : error.split(QLatin1Char('\n'))) {
        const QRegularExpressionMatch m = located.match(line);
        if (m.hasMatch())
            lines << tr("Line %1: %2").arg(m.captured(1), m.captured(2));
    }
    if (lines.isEmpty())
        lines << error.section(QLatin1Char('\n'), 0, 0);
    m_messages->setStringList(lines);
    QMessageBox::critical(this, tr("Script error!"), error);
}

void ScriptEditorDialog::goToMessage(const QModelIndex& index)
{
    static const QRegularExpression line(QStringLiteral("^\\D*(\\d+):"));
    const QRegularExpressionMatch m = line.match(index.data().toString());
    if (m.hasMatch()) {
        m_editor->goToLine(m.captured(1).toInt());
        m_editor->setFocus();
    }
}

void ScriptEditorDialog::openListed(const QModelIndex& index)
{
    if (!index.isValid() || !confirmDiscard())
        return;
    openFile(QDir(selectedFolder()).filePath(index.data().toString() + QStringLiteral(".lua")));
}

bool ScriptEditorDialog::saveForTranslation()
{
    // Save for translation: a Qt Linguist file beside the script with the texts of its FF.translate calls.
    if (m_file.isEmpty()) {
        QMessageBox::critical(this, tr("Error!"), tr("You must save the script before using this function."));
        return false;
    }
    if (isModified() && !save())
        return false;
    const QFileInfo info(m_file);
    const QString path = QDir(info.absolutePath()).filePath(info.completeBaseName() + QStringLiteral(".ts"));
    const QStringList strings = Script::translatableStrings(m_editor->toPlainText());
    QString error;
    if (!Script::writeTranslationFile(path, info.completeBaseName(), strings, &error)) {
        QMessageBox::critical(this, tr("Error!"), error);
        return false;
    }
    m_messages->setStringList(
        {tr("%n text(s) written to %1", nullptr, int(strings.size())).arg(QDir::toNativeSeparators(path))});
    return true;
}

void ScriptEditorDialog::storeGeometry()
{
    const QRect r = geometry();
    m_context.config().setValue(GeometryKey,
                                QStringLiteral("%1,%2,%3,%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height()));
}

void ScriptEditorDialog::closeEvent(QCloseEvent* event)
{
    if (m_running) {
        m_stopRequested = true;
        event->ignore();
        return;
    }
    if (!confirmDiscard()) {
        event->ignore();
        return;
    }
    storeGeometry();
    event->accept();
}

void ScriptEditorDialog::reject()
{
    if (m_running) {
        m_stopRequested = true;
        return;
    }
    if (!confirmDiscard())
        return;
    storeGeometry();
    QDialog::reject();
}

} // namespace OFS
