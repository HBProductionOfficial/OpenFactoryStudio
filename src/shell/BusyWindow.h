#pragma once

#include <QProgressDialog>

namespace OFS {

class BusyWindow : public QProgressDialog
{
    Q_OBJECT

public:
    BusyWindow(const QString& title, QWidget* parent);
    ~BusyWindow() override;
};

} // namespace OFS
