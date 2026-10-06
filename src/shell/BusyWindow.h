#pragma once

#include <QProgressDialog>

class QApplication;

namespace OFS {

// Small progress window shown while a project loads or saves.
class BusyWindow : public QProgressDialog
{
    Q_OBJECT

public:
    enum class Focus
    {
        Take,
        Leave,
    };

    BusyWindow(const QString& title, QWidget* parent, Focus focus = Focus::Leave);
    ~BusyWindow() override;

    void setSteps(int total);
    void step();
};

// Gives every dialog except message boxes the (?) help button in its title bar, same as Fighter Factory.
void installDialogHelpButtons(QApplication& app);

} // namespace OFS
