#pragma once

#include "core/code/VariableUsage.h"

#include <QDialog>

class QCheckBox;
class QRadioButton;

namespace OFS {

// States > Insert variable usage log options. They start from the defaults every time.
class VariableLogDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VariableLogDialog(QWidget* parent = nullptr);

    VariableLogOptions options() const;

private:
    QRadioButton* m_number;
    QRadioButton* m_name;
    QRadioButton* m_numberAndName;
    QCheckBox* m_onlyUsed;
    QCheckBox* m_onePerLine;
    QCheckBox* m_floats;
    QCheckBox* m_integers;
};

} // namespace OFS
