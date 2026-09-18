#include "core/engine/RunConfiguration.h"

#include "core/app/AppContext.h"
#include "core/settings/Config.h"
#include "core/theme/ThemeManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QSaveFile>
#include <QXmlStreamReader>

namespace OFS {

namespace {

struct RunConfigurationTexts
{
    Q_DECLARE_TR_FUNCTIONS(OFS::RunConfigurationTexts)
};

const QString kCount = QStringLiteral("run_count");

QString key(int index, const char* field)
{
    return QStringLiteral("run_%1_%2").arg(index).arg(QLatin1String(field));
}

} // namespace

QIcon RunConfiguration::displayIcon() const
{
    if (!icon.isEmpty() && QFileInfo::exists(icon))
        return QIcon(icon);
    if (const AppContext* context = AppContext::instance()) {
        const QIcon run = context->theme().iconSet().icon(QStringLiteral("file_run"));
        if (!run.isNull())
            return run;
    }
    return QGuiApplication::windowIcon();
}

QList<RunConfiguration> RunConfiguration::load(const Config& config)
{
    QList<RunConfiguration> list;
    const int count = qMax(0, config.intValue(kCount, 0));
    for (int i = 0; i < count; ++i) {
        RunConfiguration run;
        run.name = config.value(key(i, "name"));
        run.executable = config.value(key(i, "exe"));
        run.icon = config.value(key(i, "icon"));
        run.parameters = config.value(key(i, "params"));
        list << run;
    }
    return list;
}

void RunConfiguration::save(Config& config, const QList<RunConfiguration>& list)
{
    const int oldCount = qMax(0, config.intValue(kCount, 0));
    for (int i = 0; i < int(list.size()); ++i) {
        const RunConfiguration& run = list[i];
        config.setValue(key(i, "name"), run.name);
        config.setValue(key(i, "exe"), run.executable);
        config.setValue(key(i, "icon"), run.icon);
        config.setValue(key(i, "params"), run.parameters);
    }
    for (int i = int(list.size()); i < oldCount; ++i) {
        for (const char* field : {"name", "exe", "icon", "params"})
            config.reset(key(i, field));
    }
    config.setValue(kCount, int(list.size()));
}

QList<RunConfiguration> RunConfiguration::readFile(const QString& path, bool* found)
{
    QList<RunConfiguration> list;
    if (found)
        *found = false;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return list;
    QXmlStreamReader xml(&file);
    if (!xml.readNextStartElement())
        return list;
    while (xml.readNextStartElement()) {
        if (xml.name() != QLatin1String("engine")) {
            xml.skipCurrentElement();
            continue;
        }
        RunConfiguration run;
        run.name = xml.attributes().value(QLatin1String("name")).toString();
        while (xml.readNextStartElement()) {
            if (xml.name() == QLatin1String("param")) {
                const QString name = xml.attributes().value(QLatin1String("name")).toString();
                const QString value = xml.attributes().value(QLatin1String("value")).toString();
                if (name == QLatin1String("exe"))
                    run.executable = value;
                else if (name == QLatin1String("icon"))
                    run.icon = value;
                else if (name == QLatin1String("params"))
                    run.parameters = value;
            }
            xml.skipCurrentElement();
        }
        list << run;
        if (found)
            *found = true;
    }
    return list;
}

void RunConfiguration::loadEngineSettings(Config& config, const QString& defaultsPath, const QString& userPath)
{
    config.load(defaultsPath, userPath);
    bool found = false;
    const QList<RunConfiguration> runs = readFile(userPath, &found);
    if (found)
        save(config, runs);
}

namespace {

QString escaped(const QString& text)
{
    QString out = text.toHtmlEscaped();
    out.replace(QLatin1Char('\''), QLatin1String("&apos;"));
    return out;
}

} // namespace

bool RunConfiguration::writeEngineSettings(const Config& config, const QString& path, QString* error)
{
    QStringList lines{QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>")};
    const QMap<QString, QString> values = config.allValues();
    const QList<RunConfiguration> runs = load(config);
    QStringList body;
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (it.key().startsWith(QLatin1String("run_")))
            continue;
        body << QStringLiteral("    <config name=\"%1\" value=\"%2\"/>").arg(escaped(it.key()), escaped(it.value()));
    }
    for (const RunConfiguration& run : runs) {
        body << QStringLiteral("    <engine name=\"%1\">").arg(escaped(run.name));
        body << QStringLiteral("        <param name=\"exe\" value=\"%1\"/>").arg(escaped(run.executable));
        body << QStringLiteral("        <param name=\"icon\" value=\"%1\"/>").arg(escaped(run.icon));
        body << QStringLiteral("        <param name=\"params\" value=\"%1\"/>").arg(escaped(run.parameters));
        body << QStringLiteral("    </engine>");
    }
    if (body.isEmpty()) {
        lines << QStringLiteral("<configuration/>");
    } else {
        lines << QStringLiteral("<configuration>") << body << QStringLiteral("</configuration>");
    }
    const QByteArray bytes = (lines.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

QList<QPair<QString, QString>> RunConfiguration::parameterPresets()
{
    return {
        {RunConfigurationTexts::tr("Current char vs KFM"), QStringLiteral("%0 kfm")},
        {RunConfigurationTexts::tr("Current char vs KFM AI"), QStringLiteral("%0 kfm -p2.ai 1")},
        {RunConfigurationTexts::tr("Current char vs current char"), QStringLiteral("%0 %0")},
        {RunConfigurationTexts::tr("Current char vs current char AI"), QStringLiteral("%0 %0 -p2.ai 1")},
        {RunConfigurationTexts::tr("Current motif"), QStringLiteral("-r %0")},
        {RunConfigurationTexts::tr("KFM vs KFM on current Stage"), QStringLiteral("kfm kfm -s %0")},
        {RunConfigurationTexts::tr("KFM vs KFM on current motif"), QStringLiteral("kfm kfm -r %0")},
    };
}

} // namespace OFS
