#pragma once

#include "core/engine/Engine.h"

namespace OFS::Mugen {

class MugenEngine : public Engine
{
    Q_OBJECT

public:
    explicit MugenEngine(QObject* parent = nullptr);

    QString name() const override;
    QString displayName() const override;
    QList<EngineVersion> versions() const override;
    QList<ProjectTypeInfo> projectTypes() const override;
    QStringList projectExtensions() const override;

    void initialize(AppContext& context) override;
    QList<EditorInfo> editors() const override;
    QList<MenuSpec> menus() const override;
    QList<MenuItemSpec> toolsMenuItems() const override;
    Project* openProject(const QString& filePath, QString* error) override;
};

} // namespace OFS::Mugen
