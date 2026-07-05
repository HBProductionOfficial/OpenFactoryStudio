#include "core/Version.h"
#include "core/engine/EngineManager.h"
#include "shell/MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication::setApplicationName(QStringLiteral(OFS_DISPLAY_NAME));
    QApplication::setApplicationDisplayName(QStringLiteral(OFS_DISPLAY_NAME));
    QApplication::setApplicationVersion(QStringLiteral(OFS_VERSION_STRING));
    QApplication::setOrganizationName(QStringLiteral(OFS_ORGANIZATION));
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);

    OFS::EngineManager engines;
    engines.loadPlugins();

    OFS::MainWindow window(&engines);
    window.show();
    return app.exec();
}
