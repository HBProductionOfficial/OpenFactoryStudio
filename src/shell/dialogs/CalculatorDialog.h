#pragma once

#include "core/tools/Calculator.h"

#include <QDialog>

#include <functional>

class QLineEdit;
class QPushButton;

namespace OFS {

class Config;

// View > Calculator: a non-modal tool window. Its position is kept in calculator_x / calculator_y.
class CalculatorDialog : public QDialog
{
    Q_OBJECT

public:
    // `config` gets the window position. nullptr uses the application's settings.
    explicit CalculatorDialog(Config* config = nullptr, QWidget* parent = nullptr);
    ~CalculatorDialog() override;

    static CalculatorDialog* showCalculator(QWidget* parent = nullptr);
    static CalculatorDialog* instance();

    QString display() const;
    Calculator& calculator() { return m_calculator; }

Q_SIGNALS:
    // For the checkable View > Calculator action.
    void visibilityChanged(bool visible);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QPushButton* addButton(const QString& text, const QString& name, int row, int column, int columnSpan,
                           const std::function<void()>& action);
    bool handleKey(QKeyEvent* event);
    void refresh();
    void restorePosition();
    void storePosition();

    Calculator m_calculator;
    Config* m_config;
    QLineEdit* m_display;
    bool m_positionRestored = false;
};

} // namespace OFS
