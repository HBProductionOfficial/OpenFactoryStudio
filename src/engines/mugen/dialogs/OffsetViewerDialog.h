#pragma once

#include <QDialog>

#include <memory>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;

namespace OFS {
class Project;
}

namespace OFS::Mugen {

class AnimationPreview;
class ProjectGraphics;
struct AirAction;

// States > Offset viewer: helps find the offsets of binds and helpers. It writes nothing.
class OffsetViewerDialog : public QDialog
{
    Q_OBJECT

public:
    OffsetViewerDialog(Project* project, const QList<Project*>& projects, QWidget* parent = nullptr);
    ~OffsetViewerDialog() override;

    AnimationPreview* preview() const { return m_preview; }

private:
    void fillAnimations(QComboBox* combo, const ProjectGraphics& graphics) const;
    void fillFrames(QComboBox* frames, const QComboBox* animations, const ProjectGraphics* graphics) const;
    const AirAction* selectedAnimation(const QComboBox* combo, const ProjectGraphics* graphics) const;
    void setSecondProject(int index);
    void updatePreview();

    QList<Project*> m_projects;
    std::unique_ptr<ProjectGraphics> m_first;
    std::unique_ptr<ProjectGraphics> m_second;
    bool m_updating = false;

    AnimationPreview* m_preview;
    QComboBox* m_resolution;
    QCheckBox* m_flipH;
    QCheckBox* m_flipV;
    QComboBox* m_animation1;
    QComboBox* m_frame1;
    QDoubleSpinBox* m_x;
    QDoubleSpinBox* m_y;
    QComboBox* m_projectsBox;
    QComboBox* m_animation2;
    QComboBox* m_frame2;
};

} // namespace OFS::Mugen
