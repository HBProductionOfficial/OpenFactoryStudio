#pragma once

#include "core/anims/AirDocument.h"

#include <QObject>
#include <QPointer>

#include <functional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListView;
class QScrollBar;
class QSpinBox;
class QStandardItemModel;
class QWidget;

namespace OFS {

class AnimationCodeWindow;
class AnimationPage;
class AppContext;
class OrganizerBinding;
class SpriteDocument;
class SpriteExplorer;
class SpriteFile;

// Left panel of the Animations editor, bound to the current animation page, plus the editor's actions.
class AnimationsEditor : public QObject
{
    Q_OBJECT

public:
    explicit AnimationsEditor(AppContext& context, QObject* parent = nullptr);
    ~AnimationsEditor() override;

    void bindPanel(QWidget* panel);
    void bindExplorer(QWidget* panel);
    // The explorer panel is shared with the Sprites editor.
    void setExplorerActive(bool active);
    void setPage(AnimationPage* page);
    AnimationPage* page() const { return m_page; }
    OrganizerBinding* organizer() const { return m_organizer; }

    struct ImportSource
    {
        QString name;
        QStringList lines;
        const SpriteDocument* sprites = nullptr;
    };
    void importFromProject(const QList<ImportSource>& sources);

    bool perform(const QString& actionId);
    void copyBox();
    void cutBox();
    void pasteBox();

Q_SIGNALS:
    void stateChanged();

private:
    void refresh();
    void refreshOnion();
    void refreshExplorer();
    void applyFrame(const std::function<void(AirFrame&)>& change);
    bool performBoxes(const QString& id);
    bool performDialog(const QString& id);
    SpriteFile* projectSprites() const;
    void addExplorerFrames(bool all);
    void newAnimationFromExplorer(bool all);
    void saveGif();
    void showCodeWindow();

    AppContext& m_context;
    QPointer<AnimationPage> m_page;
    QPointer<QScrollBar> m_selector;
    QPointer<QLabel> m_selectorLabel;
    QPointer<QSpinBox> m_number;
    QPointer<QLineEdit> m_name;
    QPointer<QScrollBar> m_frameSelector;
    QPointer<QLabel> m_frameLabel;
    QPointer<QSpinBox> m_group;
    QPointer<QSpinBox> m_index;
    QPointer<QSpinBox> m_x;
    QPointer<QSpinBox> m_y;
    QPointer<QSpinBox> m_time;
    QPointer<QComboBox> m_flip;
    QPointer<QComboBox> m_transparency;
    QPointer<QSpinBox> m_source;
    QPointer<QSpinBox> m_dest;
    QPointer<QCheckBox> m_loop;
    QPointer<QDoubleSpinBox> m_xScale;
    QPointer<QDoubleSpinBox> m_yScale;
    QPointer<QDoubleSpinBox> m_angle;
    QPointer<QCheckBox> m_interpolate[4]; // blend, offset, scale, angle
    QPointer<QCheckBox> m_onion;
    QPointer<QScrollBar> m_onionSelector;
    QPointer<QScrollBar> m_onionFrameSelector;
    QPointer<QLabel> m_onionLabel;
    SpriteExplorer* m_spriteExplorer;
    QPointer<AnimationCodeWindow> m_codeWindow;
    OrganizerBinding* m_organizer = nullptr;
    bool m_updating = false;
    bool m_onionUsed = false;
    bool m_explorerActive = false;
};

} // namespace OFS
