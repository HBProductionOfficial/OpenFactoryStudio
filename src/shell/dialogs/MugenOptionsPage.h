#pragma once

#include "core/engine/RunConfiguration.h"
#include "core/options/OptionsPage.h"

#include <QHash>
#include <QList>

class QKeySequenceEdit;
class QLabel;
class QListWidget;
class QPushButton;

namespace OFS {

class Config;

// M.U.G.E.N tab of the Options dialog. Changes are stored at once in mugen_config.xml with these keys:
//     run_count, run_<n>_name, run_<n>_exe, run_<n>_icon, run_<n>_params
//     clsn_default_attack, clsn_default_collision          "R,G,B"
//     p1_left, p1_right, p1_up, p1_down, p1_a, p1_b, p1_c, p1_x, p1_y, p1_z, p1_start (and p2_...)
//                                                          portable key text ("A", "Left", "4")
class MugenOptionsPage : public OptionsPage
{
    Q_OBJECT

public:
    explicit MugenOptionsPage(Config& config, QWidget* parent = nullptr);

    QString title() const override;
    void load() override;
    // Resets the controls and colors. Run configurations are user data and are kept.
    void loadDefaults() override;
    void apply() override;
    QMap<QString, QString> values() const override;
    void setValues(const QMap<QString, QString>& values) override;

    QList<RunConfiguration> runConfigurations() const { return m_runs; }
    void setRunConfigurations(const QList<RunConfiguration>& runs);
    void addRunConfiguration(const RunConfiguration& run);
    void removeRunConfiguration(int index);
    void moveRunConfiguration(int index, int delta);
    int currentRunConfiguration() const;

    QColor clsnColor(bool attack) const;
    void setClsnColor(bool attack, const QColor& color);

    static QStringList controlNames();
    static QString defaultValue(const QString& key);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QWidget* createRunGroup();
    QWidget* createClsnGroup();
    QWidget* createControlsGroup(int player);
    void store(const QString& key, const QString& value);
    void save();
    void refreshRunList(int select);
    void updateRunButtons();
    void updateClsnLabels();
    void addClicked();
    void editClicked();
    void clearClicked();
    void chooseClsnColor(bool attack);

    Config& m_config;
    bool m_loading = false;
    QList<RunConfiguration> m_runs;
    QListWidget* m_runList = nullptr;
    QPushButton* m_remove = nullptr;
    QPushButton* m_edit = nullptr;
    QPushButton* m_clear = nullptr;
    QPushButton* m_up = nullptr;
    QPushButton* m_down = nullptr;
    QLabel* m_attack = nullptr;
    QLabel* m_collision = nullptr;
    QHash<QString, QKeySequenceEdit*> m_keys;
};

} // namespace OFS
