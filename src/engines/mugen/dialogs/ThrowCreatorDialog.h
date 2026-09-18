#pragma once

#include "engines/mugen/ThrowCreator.h"

#include <QDialog>
#include <QPointer>

#include <functional>
#include <memory>

class QComboBox;
class QDoubleSpinBox;
class QSlider;
class QSpinBox;
class QToolButton;

namespace OFS {
class CodeFile;
class Project;
} // namespace OFS

namespace OFS::Mugen {

class AnimationPreview;
class ProjectGraphics;
struct AirAction;

// States > Throw creator: builds a throw's states from the animations of both projects and inserts
// them at the cursor.
class ThrowCreatorDialog : public QDialog
{
    Q_OBJECT

public:
    // `projects` are the open projects offered as the opponent. `script` writes the states.
    ThrowCreatorDialog(Project* project, const QList<Project*>& projects, const QString& script,
                       QWidget* parent = nullptr);
    ~ThrowCreatorDialog() override;

    void setInsertHandler(std::function<void(const QString&)> handler) { m_insert = std::move(handler); }
    void setIcons(const std::function<QIcon(const QString&)>& icon);

    ThrowSettings settings() const;
    AnimationPreview* preview() const { return m_preview; }

private:
    void fillAnimations(QComboBox* combo, int preferred) const;
    const AirAction* selectedAnimation(const QComboBox* combo) const;
    void setSpriteSource(int index);
    void setElementCount(int count);
    void showElement(int element);
    void storeBind();
    void updatePreview();
    void applyResolution();
    void insert();
    void editFrame();
    void insertFrame();
    void deleteFrame();
    void addAnimation();
    void reloadAnimations();

    QList<Project*> m_projects;
    QString m_script;
    std::function<void(const QString&)> m_insert;
    std::unique_ptr<ProjectGraphics> m_p1;
    std::unique_ptr<ProjectGraphics> m_p2;
    QPointer<CodeFile> m_air;
    qint64 m_tick = 0;
    int m_frame = -1;
    QList<ThrowBind> m_binds;
    bool m_updating = false;

    AnimationPreview* m_preview;
    QSpinBox* m_stateDef;
    QComboBox* m_startAnim;
    QComboBox* m_throwAnim;
    QSpinBox* m_elements;
    QSlider* m_element;
    QDoubleSpinBox* m_bindX;
    QDoubleSpinBox* m_bindY;
    QSpinBox* m_priority;
    QComboBox* m_resolution;
    QComboBox* m_opponents;
    QSpinBox* m_xVelocity;
    QSpinBox* m_yVelocity;
    QComboBox* m_thrownAnim;
    QToolButton* m_addAnim;
    QSpinBox* m_group;
    QSpinBox* m_index;
    QSpinBox* m_typingIn = nullptr;
    int m_typingSteps = -1;
    int m_typedInRun = 0;
    QComboBox* m_flip;
    QToolButton* m_insertFrame;
    QToolButton* m_deleteFrame;
};

} // namespace OFS::Mugen
