#pragma once

#include "core/engine/RunConfiguration.h"

#include <QDialog>

class QListWidget;

namespace OFS {

// "Run in Engine": picks the run configuration when there are several.
class RunChooserDialog : public QDialog
{
    Q_OBJECT

public:
    RunChooserDialog(const QList<RunConfiguration>& configurations, int current, QWidget* parent = nullptr);

    int selectedIndex() const;

private:
    QListWidget* m_list;
};

} // namespace OFS
