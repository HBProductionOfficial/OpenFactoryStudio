#include "core/Version.h"
#include "core/app/AppContext.h"
#include "core/settings/Config.h"
#include "shell/MainWindow.h"

#include <QApplication>
#include <QStringList>
#include <QStyleFactory>

int main(int argc, char* argv[])
{
    QApplication::setApplicationName(QStringLiteral(OFS_DISPLAY_NAME));
    QApplication::setApplicationDisplayName(QStringLiteral(OFS_DISPLAY_NAME));
    QApplication::setApplicationVersion(QStringLiteral(OFS_VERSION_STRING));
    QApplication::setOrganizationName(QStringLiteral(OFS_ORGANIZATION));
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
#if defined(Q_OS_WIN)
    // The interface styles are designed on top of the classic Windows look.
    if (QStyleFactory::keys().contains(QStringLiteral("windowsvista"), Qt::CaseInsensitive))
        QApplication::setStyle(QStringLiteral("windowsvista"));
#endif

    OFS::AppContext context;
    context.loadSettings();
    context.initializeEngines();

    OFS::MainWindow window(context);
    window.show();

    const QStringList args = QApplication::arguments().mid(1);
    for (const QString& arg : args) {
        if (!arg.startsWith(QLatin1Char('-')))
            window.openProject(arg);
    }
    return app.exec();
}
