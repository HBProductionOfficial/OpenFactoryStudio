#pragma once

#include "core/CoreExport.h"

#include <QMap>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <functional>

namespace OFS {

class Config;

// An engine's tab of the Options dialog, working on AppContext::engineConfig(). A page may store
// every change at once, like the M.U.G.E.N page, and then apply() is empty.
class OFS_CORE_EXPORT OptionsPage : public QWidget
{
    Q_OBJECT

public:
    explicit OptionsPage(QWidget* parent = nullptr);
    ~OptionsPage() override;

    virtual QString title() const = 0;

    virtual void load() = 0;
    virtual void loadDefaults() = 0;
    virtual void apply() = 0;

    virtual QMap<QString, QString> values() const = 0;
    virtual void setValues(const QMap<QString, QString>& values) = 0;

Q_SIGNALS:
    void changed();
};

class OFS_CORE_EXPORT OptionsPageRegistry
{
public:
    using Factory = std::function<OptionsPage*(Config& engineConfig, QWidget* parent)>;

    static void registerPage(const QString& engine, Factory factory);
    static void unregisterPage(const QString& engine);
    static bool hasPage(const QString& engine);
    static QStringList engines();

    static OptionsPage* create(const QString& engine, Config& engineConfig, QWidget* parent);
};

} // namespace OFS
