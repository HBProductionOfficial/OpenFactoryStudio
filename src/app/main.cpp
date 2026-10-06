#include "core/Version.h"
#include "core/app/AppContext.h"
#include "core/app/Paths.h"
#include "core/settings/Config.h"
#include "core/settings/SettingsFiles.h"
#include "core/widgets/RenderSurface.h"
#include "shell/BusyWindow.h"
#include "shell/MainWindow.h"
#include "shell/dialogs/FirstRunDialog.h"

#include <QApplication>
#include <QStringList>
#include <QStyleFactory>
#include <QStyleHints>

int main(int argc, char* argv[])
{
    // No display name, because Qt would add it to every window title.
    QApplication::setApplicationName(QStringLiteral(OFS_DISPLAY_NAME));
    QApplication::setApplicationVersion(QStringLiteral(OFS_VERSION_STRING));
    QApplication::setOrganizationName(QStringLiteral(OFS_ORGANIZATION));
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    // Qt reads the DPI scaling factor before the app starts, so a change applies on the next start.
    if (!qEnvironmentVariableIsSet("QT_SCALE_FACTOR")) {
        const double scale = OFS::AppContext::interfaceScaleFactor(OFS::SettingsFiles::config(OFS::Paths::userDir()));
        if (scale != 1.0)
            qputenv("QT_SCALE_FACTOR", QByteArray::number(scale));
    }

    QApplication app(argc, argv);
    OFS::installDialogHelpButtons(app);
#if defined(Q_OS_WIN)
    // The interface styles are designed on top of the classic Windows look.
    if (QStyleFactory::keys().contains(QStringLiteral("windowsvista"), Qt::CaseInsensitive))
        QApplication::setStyle(QStringLiteral("windowsvista"));
    // Light title bars in any system color mode. Same as Fighter Factory.
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
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
