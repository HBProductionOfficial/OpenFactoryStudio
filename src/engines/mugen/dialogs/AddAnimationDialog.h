#pragma once

#include <QDialog>
#include <QList>

class QLineEdit;
class QSpinBox;

namespace OFS::Mugen {

// Throw creator > Add animation. OK refuses a number that is taken.
class AddAnimationDialog : public QDialog
{
    Q_OBJECT

public:
    AddAnimationDialog(const QList<int>& taken, int suggested, QWidget* parent = nullptr);

    QString name() const;
    int number() const;
    int time() const;

    void accept() override;

private:
    QList<int> m_taken;
    QLineEdit* m_name;
    QSpinBox* m_number;
    QSpinBox* m_time;
};

} // namespace OFS::Mugen
