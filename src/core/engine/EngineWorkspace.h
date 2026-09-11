#pragma once

#include "core/CoreExport.h"

#include <QStringList>
#include <QWidget>

namespace OFS {

class OFS_CORE_EXPORT EngineWorkspace : public QWidget
{
    Q_OBJECT

public:
    using QWidget::QWidget;

    virtual void bindPanel(QWidget* panel) = 0;
    virtual void applySettings() {}
    virtual QStringList statusSections() const { return {}; }
    virtual bool perform(const QString& actionId)
    {
        Q_UNUSED(actionId)
        return false;
    }

Q_SIGNALS:
    void statusChanged();
};

} // namespace OFS
