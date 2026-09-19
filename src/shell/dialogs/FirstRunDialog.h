#pragma once

#include <QDialog>
#include <QSize>

class QButtonGroup;

namespace OFS {

class AppContext;

// Shown at the first start to pick a theme, or to import the settings of a previous installation.
class FirstRunDialog : public QDialog
{
    Q_OBJECT

public:
    struct Theme
    {
        QString name;
        QString preset;
        QString iconset;
        QString stylesheet;
        QString colorset;
    };
    static QList<Theme> themes();
    static QSize previewSize() { return {160, 104}; }

    explicit FirstRunDialog(AppContext& context, QWidget* parent = nullptr);

    int selectedTheme() const;
    void selectTheme(int index);

    void applySelection();

    static bool isNeeded(const AppContext& context);

private:
    void importPrevious();

    AppContext& m_context;
    QButtonGroup* m_group;
    bool m_imported = false;
};

} // namespace OFS
