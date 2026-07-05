#pragma once

#include <QMainWindow>

namespace OFS {

class EngineManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(EngineManager* engines, QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    EngineManager* m_engines;
};

} // namespace OFS
