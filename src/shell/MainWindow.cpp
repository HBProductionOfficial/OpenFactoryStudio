#include "shell/MainWindow.h"

#include "core/Version.h"
#include "core/engine/Engine.h"
#include "core/engine/EngineManager.h"

#include <QStatusBar>

namespace OFS {

MainWindow::MainWindow(EngineManager* engines, QWidget* parent)
    : QMainWindow(parent)
    , m_engines(engines)
{
    setObjectName(QStringLiteral("MainWindow"));
    setWindowTitle(QStringLiteral(OFS_DISPLAY_NAME));
    resize(1280, 800);

    QStringList names;
    for (const Engine* engine : m_engines->engines())
        names << engine->displayName();
    statusBar()->showMessage(names.isEmpty() ? tr("No engine plugins found") : names.join(QStringLiteral(", ")));
}

MainWindow::~MainWindow() = default;

} // namespace OFS
