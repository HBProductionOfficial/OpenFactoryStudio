#include "core/engine/RunConfiguration.h"

#include "core/settings/Config.h"

namespace OFS {

namespace {

const QString kCount = QStringLiteral("run_count");

QString key(int index, const char* field)
{
    return QStringLiteral("run_%1_%2").arg(index).arg(QLatin1String(field));
}

} // namespace

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

} // namespace OFS
