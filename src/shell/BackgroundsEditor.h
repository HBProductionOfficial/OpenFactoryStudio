#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QStringList>

#include <functional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListView;
class QScrollBar;
class QSpinBox;
class QStandardItemModel;
class QWidget;

namespace OFS {

class AppContext;
class BackgroundsPage;

// Element and Controllers panels of the Backgrounds editor, bound to the current page. Every field writes its key in
// place, adds it after the section's last key or removes it at its default, as one undo step.
class BackgroundsEditor : public QObject
{
    Q_OBJECT

public:
    explicit BackgroundsEditor(AppContext& context, QObject* parent = nullptr);

    void bindPanel(QWidget* panel);
    void bindControllers(QWidget* panel);
    void bindOrganizer(QWidget* panel);
    void setOrganizerActive(bool active);
    void setPage(BackgroundsPage* page);
    BackgroundsPage* page() const { return m_page; }

    QStringList groupNames() const;
    int group() const;
    void setGroup(int group);

    bool perform(const QString& actionId);

Q_SIGNALS:
    void stateChanged();

private:
    void refresh();
    void refreshControllers();
    void refreshOrganizer();
    QList<int> organizerSelection() const;
    void moveElements(const QString& actionId);
    void updateEnabled();
    void editElement(const std::function<void(QStringList&, int)>& change);
    void editDefinition(const std::function<void(QStringList&, int)>& change);
    void editController(const std::function<void(QStringList&, int)>& change);
    void changeType(int type);
    void changeTransparency(int index);
    void changeParallaxMode(int mode);
    void changeWindowMode(int mode);
    void writeWindow();
    void writeIds();
    void writeTime();
    void changeControllerType(int type);
    double spriteWidth() const;
    bool version11() const;
    bool confirm(const QString& title, const QString& text);

    AppContext& m_context;
    QPointer<BackgroundsPage> m_page;
    bool m_updating = false;
    QSet<QString> m_alignedParallax;

    // Element panel.
    QPointer<QScrollBar> m_selector;
    QPointer<QLabel> m_selectorLabel;
    QPointer<QLineEdit> m_name;
    QPointer<QComboBox> m_type;
    QPointer<QSpinBox> m_spriteGroup;
    QPointer<QSpinBox> m_spriteNumber;
    QPointer<QSpinBox> m_action;
    QPointer<QCheckBox> m_mask;
    QPointer<QDoubleSpinBox> m_startX, m_startY, m_deltaX, m_deltaY, m_velocityX, m_velocityY;
    QPointer<QSpinBox> m_id;
    QPointer<QSpinBox> m_layer;
    QPointer<QCheckBox> m_link;
    QPointer<QSpinBox> m_tileX, m_tileY, m_spacingX, m_spacingY;
    QPointer<QComboBox> m_trans;
    QPointer<QSpinBox> m_source;
    QPointer<QSpinBox> m_dest;
    QPointer<QComboBox> m_parallaxMode;
    QPointer<QDoubleSpinBox> m_parallaxTop, m_parallaxBottom;
    QPointer<QDoubleSpinBox> m_scaleStartX, m_scaleStartY, m_scaleDeltaX, m_scaleDeltaY;
    QPointer<QDoubleSpinBox> m_yScaleStart, m_yScaleDelta;
    QPointer<QDoubleSpinBox> m_windowDeltaX, m_windowDeltaY;
    QPointer<QComboBox> m_windowMode;
    QPointer<QSpinBox> m_windowTop, m_windowLeft, m_windowBottom, m_windowRight;
    QPointer<QDoubleSpinBox> m_sinX[3];
    QPointer<QDoubleSpinBox> m_sinY[3];
    QPointer<QWidget> m_panel;

    // Controllers panel.
    QPointer<QScrollBar> m_defSelector;
    QPointer<QLabel> m_defLabel;
    QPointer<QLineEdit> m_defName;
    QPointer<QSpinBox> m_defLoop;
    QList<QPointer<QSpinBox>> m_ids;
    QPointer<QScrollBar> m_ctrlSelector;
    QPointer<QLabel> m_ctrlLabel;
    QPointer<QLineEdit> m_ctrlName;
    QPointer<QComboBox> m_ctrlType;
    QPointer<QCheckBox> m_ctrlBool;
    QPointer<QLabel> m_ctrlAnimLabel;
    QPointer<QSpinBox> m_ctrlAnim;
    QPointer<QCheckBox> m_ctrlXOn, m_ctrlYOn;
    QPointer<QDoubleSpinBox> m_ctrlX, m_ctrlY;
    QPointer<QDoubleSpinBox> m_ctrlSine[3];
    QList<QPointer<QLabel>> m_ctrlSineLabels;
    QPointer<QSpinBox> m_ctrlStart, m_ctrlEnd, m_ctrlLoop;
    QPointer<QWidget> m_controllers;

    // Organizer.
    QPointer<QListView> m_organizer;
    QStandardItemModel* m_organizerModel = nullptr;
    bool m_organizerActive = false;
};

} // namespace OFS
