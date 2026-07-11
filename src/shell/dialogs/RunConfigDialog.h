#pragma once

#include "core/engine/RunConfiguration.h"

#include <QDialog>
#include <QList>

class QComboBox;
class QDialogButtonBox;
class QLineEdit;

namespace OFS {

class RunConfigDialog : public QDialog
{
    Q_OBJECT

public:
    struct ParameterPreset
    {
        QString caption;
        QString parameters;
    };

    explicit RunConfigDialog(const RunConfiguration& value = {}, bool editing = false, QWidget* parent = nullptr);

    RunConfiguration value() const;
    void setValue(const RunConfiguration& value);

    // Command-line templates offered by the Parameters combo box ("%0" is the project's main file).
    static QList<ParameterPreset> parameterPresets();

private:
    void updateOk();
    void browseExecutable();
    void browseIcon();

    QLineEdit* m_name;
    QLineEdit* m_executable;
    QLineEdit* m_icon;
    QComboBox* m_presets;
    QLineEdit* m_parameters;
    QDialogButtonBox* m_buttons;
};

} // namespace OFS
