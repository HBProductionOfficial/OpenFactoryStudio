#pragma once

#include "core/debug/Breakpoints.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace OFS {

// Debug > Add breakpoint... and the Debugger panel's Edit selected breakpoint. The type picks which fields are used.
class BreakpointDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BreakpointDialog(QWidget* parent = nullptr);

    // When editing, the type cannot change.
    void setBreakpoint(const Debug::Breakpoint& breakpoint, bool editing);
    Debug::Breakpoint breakpoint() const;

private:
    void updateFields();

    Debug::Breakpoint m_original;
    QComboBox* m_type;
    QLineEdit* m_file;
    QPushButton* m_browse;
    QSpinBox* m_line;
    QCheckBox* m_enabled;
    QLineEdit* m_address;
    QLineEdit* m_function;
    QLineEdit* m_condition;
    QSpinBox* m_ignore;
    QCheckBox* m_oneShot;
    QDialogButtonBox* m_buttons;
};

} // namespace OFS
