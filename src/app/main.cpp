#include "core/Version.h"
#include "core/app/AppContext.h"
#include "core/app/Paths.h"
#include "core/settings/Config.h"
#include "core/widgets/RenderSurface.h"
#include "shell/MainWindow.h"
#include "shell/dialogs/FirstRunDialog.h"

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
    OFS::Rendering::choose(context.config(), OFS::Paths::userDir());
    context.initializeEngines();

    if (OFS::FirstRunDialog::isNeeded(context)) {
        OFS::FirstRunDialog firstRun(context);
        firstRun.exec();
        firstRun.applySelection();
    }

    OFS::MainWindow window(context);
    window.show();

    const QStringList args = QApplication::arguments().mid(1);
    for (const QString& arg : args) {
        if (!arg.startsWith(QLatin1Char('-')))
            window.openProject(arg);
    }
    const int result = app.exec();
    OFS::Rendering::finish();
    return result;
}
