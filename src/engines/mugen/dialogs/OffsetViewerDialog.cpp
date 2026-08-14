#include "engines/mugen/dialogs/OffsetViewerDialog.h"

#include "core/project/Project.h"
#include "engines/mugen/ProjectGraphics.h"
#include "engines/mugen/dialogs/AnimationPreview.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <utility>

namespace OFS::Mugen {

namespace {

QComboBox* comboBox(const char* name, QWidget* parent)
{
    auto* c = new QComboBox(parent);
    c->setObjectName(QLatin1String(name));
    c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    c->setMinimumContentsLength(4);
    return c;
}

QDoubleSpinBox* offsetBox(const char* name, double value, QWidget* parent)
{
    auto* s = new QDoubleSpinBox(parent);
    s->setObjectName(QLatin1String(name));
    s->setDecimals(3);
    s->setRange(-65535, 65535);
    s->setValue(value);
    return s;
}

} // namespace

OffsetViewerDialog::OffsetViewerDialog(Project* project, const QList<Project*>& projects, QWidget* parent)
    : QDialog(parent)
    , m_projects(projects)
    , m_first(std::make_unique<ProjectGraphics>(*project))
{
    setObjectName(QStringLiteral("OffsetDlg"));
    setWindowTitle(tr("Offset viewer"));
    if (!m_projects.contains(project))
        m_projects.prepend(project);

    m_preview = new AnimationPreview(this);
    m_preview->setObjectName(QStringLiteral("render"));

    m_resolution = comboBox("res", this);
    for (const char* size : {"320x240", "640x480", "800x600", "1024x768", "1280x720", "1920x1080"})
        m_resolution->addItem(QString::fromLatin1(size));
    m_resolution->setCurrentIndex(1);
    m_flipH = new QCheckBox(tr("Flip horizontally"), this);
    m_flipH->setObjectName(QStringLiteral("fliph"));
    m_flipH->setChecked(true);
    m_flipV = new QCheckBox(tr("Flip vertically"), this);
    m_flipV->setObjectName(QStringLiteral("flipv"));
    auto* options = new QVBoxLayout;
    options->addWidget(m_resolution);
    options->addWidget(m_flipH);
    options->addWidget(m_flipV);

    auto* first = new QGroupBox(tr("Animation 1:"), this);
    first->setObjectName(QStringLiteral("groupBox_3"));
    m_animation1 = comboBox("anim1", first);
    m_frame1 = comboBox("frame1", first);
    auto* firstLayout = new QHBoxLayout(first);
    firstLayout->addWidget(m_animation1, 2);
    firstLayout->addWidget(m_frame1, 1);

    m_x = offsetBox("xoff", 100, this);
    m_y = offsetBox("yoff", 0, this);
    auto* offset = new QGridLayout;
    auto* xLabel = new QLabel(tr("X:"), this);
    xLabel->setObjectName(QStringLiteral("label_3"));
    auto* yLabel = new QLabel(tr("Y:"), this);
    yLabel->setObjectName(QStringLiteral("label_4"));
    offset->addWidget(xLabel, 0, 0);
    offset->addWidget(m_x, 0, 1);
    offset->addWidget(yLabel, 1, 0);
    offset->addWidget(m_y, 1, 1);

    auto* second = new QGroupBox(tr("Animation 2:"), this);
    second->setObjectName(QStringLiteral("groupBox_4"));
    m_projectsBox = comboBox("projs", second);
    for (const Project* p : std::as_const(m_projects))
        m_projectsBox->addItem(p->name());
    m_animation2 = comboBox("anim2", second);
    m_frame2 = comboBox("frame2", second);
    auto* secondLayout = new QGridLayout(second);
    secondLayout->addWidget(m_projectsBox, 0, 0, 1, 2);
    secondLayout->addWidget(m_animation2, 1, 0);
    secondLayout->addWidget(m_frame2, 1, 1);
    secondLayout->setColumnStretch(0, 3);
    secondLayout->setColumnStretch(1, 1);

    auto* row = new QHBoxLayout;
    row->addLayout(options, 199);
    row->addWidget(first, 264);
    row->addLayout(offset, 195);
    row->addWidget(second, 357);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_preview, 1);
    layout->addLayout(row);
    resize(717, 560);

    fillAnimations(m_animation1, *m_first);
    fillFrames(m_frame1, m_animation1, m_first.get());
    m_projectsBox->setCurrentIndex(int(m_projects.indexOf(project)));
    setSecondProject(m_projectsBox->currentIndex());

    connect(m_animation1, &QComboBox::currentIndexChanged, this, [this] {
        const bool updating = std::exchange(m_updating, true);
        fillFrames(m_frame1, m_animation1, m_first.get());
        m_updating = updating;
        updatePreview();
    });
    connect(m_animation2, &QComboBox::currentIndexChanged, this, [this] {
        const bool updating = std::exchange(m_updating, true);
        fillFrames(m_frame2, m_animation2, m_second.get());
        m_updating = updating;
        updatePreview();
    });
    connect(m_projectsBox, &QComboBox::currentIndexChanged, this, &OffsetViewerDialog::setSecondProject);
    for (QComboBox* combo : {m_resolution, m_frame1, m_frame2})
        connect(combo, &QComboBox::currentIndexChanged, this, &OffsetViewerDialog::updatePreview);
    for (QCheckBox* box : {m_flipH, m_flipV})
        connect(box, &QCheckBox::toggled, this, &OffsetViewerDialog::updatePreview);
    for (QDoubleSpinBox* box : {m_x, m_y})
        connect(box, &QDoubleSpinBox::valueChanged, this, &OffsetViewerDialog::updatePreview);
}

OffsetViewerDialog::~OffsetViewerDialog() = default;

void OffsetViewerDialog::fillAnimations(QComboBox* combo, const ProjectGraphics& graphics) const
{
    combo->clear();
    for (const AirAction& action : graphics.animations())
        combo->addItem(action.caption(), action.number);
    combo->setCurrentIndex(combo->count() > 0 ? 0 : -1);
}

void OffsetViewerDialog::fillFrames(QComboBox* frames, const QComboBox* animations,
                                    const ProjectGraphics* graphics) const
{
    frames->clear();
    if (const AirAction* action = selectedAnimation(animations, graphics)) {
        for (qsizetype i = 0; i < action->frames.size(); ++i)
            frames->addItem(QString::number(i));
    }
    frames->setCurrentIndex(frames->count() > 0 ? 0 : -1);
}

const AirAction* OffsetViewerDialog::selectedAnimation(const QComboBox* combo, const ProjectGraphics* graphics) const
{
    if (!graphics || combo->currentIndex() < 0)
        return nullptr;
    return findAnimation(graphics->animations(), combo->currentData().toInt());
}

void OffsetViewerDialog::setSecondProject(int index)
{
    if (index < 0 || index >= m_projects.size())
        return;
    m_second = std::make_unique<ProjectGraphics>(*m_projects.at(index));
    const bool updating = std::exchange(m_updating, true);
    fillAnimations(m_animation2, *m_second);
    fillFrames(m_frame2, m_animation2, m_second.get());
    m_updating = updating;
    updatePreview();
}

void OffsetViewerDialog::updatePreview()
{
    if (m_updating)
        return;
    const double screen = m_resolution->currentText().section(QLatin1Char('x'), 0, 0).toDouble();
    auto figure = [screen](const ProjectGraphics* graphics, const AirAction* action, int element, bool mirrored,
                           bool upsideDown) {
        AnimationPreview::Figure f;
        if (!graphics)
            return f;
        f.scale = screen / graphics->localWidth();
        if (!action || element < 0 || element >= action->frames.size())
            return f;
        const AirFrame& frame = action->frames.at(element);
        f.image = graphics->sprite(frame.group, frame.index, &f.axis);
        f.flipH = action->flipsHorizontally(element) != mirrored;
        f.flipV = action->flipsVertically(element) != upsideDown;
        f.position = QPointF(mirrored ? -frame.x : frame.x, upsideDown ? -frame.y : frame.y) * f.scale;
        return f;
    };
    AnimationPreview::Figure first =
        figure(m_first.get(), selectedAnimation(m_animation1, m_first.get()), m_frame1->currentIndex(), false, false);
    first.cross = Qt::white;
    AnimationPreview::Figure second = figure(m_second.get(), selectedAnimation(m_animation2, m_second.get()),
                                             m_frame2->currentIndex(), m_flipH->isChecked(), m_flipV->isChecked());
    second.position += QPointF(m_x->value(), m_y->value());
    second.cross = QColor(40, 90, 255);
    m_preview->setFigures({first, second});
}

} // namespace OFS::Mugen
