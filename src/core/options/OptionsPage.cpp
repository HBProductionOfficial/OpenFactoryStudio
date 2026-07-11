#include "core/options/OptionsPage.h"

#include <QMap>

namespace OFS {

namespace {

QMap<QString, OptionsPageRegistry::Factory>& factories()
{
    static QMap<QString, OptionsPageRegistry::Factory> map;
    return map;
}

} // namespace

OptionsPage::OptionsPage(QWidget* parent)
    : QWidget(parent)
{}

OptionsPage::~OptionsPage() = default;

void OptionsPageRegistry::registerPage(const QString& engine, Factory factory)
{
    factories().insert(engine, std::move(factory));
}

void OptionsPageRegistry::unregisterPage(const QString& engine)
{
    factories().remove(engine);
}

bool OptionsPageRegistry::hasPage(const QString& engine)
{
    return factories().contains(engine);
}

QStringList OptionsPageRegistry::engines()
{
    return factories().keys();
}

OptionsPage* OptionsPageRegistry::create(const QString& engine, Config& engineConfig, QWidget* parent)
{
    const auto it = factories().constFind(engine);
    if (it == factories().cend() || !it.value())
        return nullptr;
    return it.value()(engineConfig, parent);
}

} // namespace OFS
