#pragma once

#include <QDialog>

class QComboBox;
class QLineEdit;

namespace OFS {

class AddBackgroundDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddBackgroundDialog(QWidget* parent = nullptr);

    QString name() const;
    // 0 Normal, 1 Animation, 2 Parallax, 3 Dummy.
    int type() const;

private:
    QLineEdit* m_name;
    QComboBox* m_type;
};

} // namespace OFS
