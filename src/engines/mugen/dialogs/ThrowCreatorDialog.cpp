#include "engines/mugen/dialogs/ThrowCreatorDialog.h"

#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "engines/mugen/AirEdits.h"
#include "engines/mugen/ProjectGraphics.h"
#include "engines/mugen/dialogs/AddAnimationDialog.h"
#include "engines/mugen/dialogs/AnimationPreview.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace OFS::Mugen {

namespace {

constexpr int DefaultStartAnim = 800;
constexpr int DefaultThrowAnim = 810;
constexpr int DefaultThrownAnim = 820;

QLabel* label(const QString& text, const char* name, QWidget* parent)
{
    auto* l = new QLabel(text, parent);
    l->setObjectName(QLatin1String(name));
    return l;
}

QLabel* heading(const QString& text, const char* name, QWidget* parent)
{
    QLabel* l = label(text, name, parent);
    l->setAlignment(Qt::AlignCenter);
    QFont font = l->font();
    font.setBold(true);
    l->setFont(font);
    return l;
}

QSpinBox* spinBox(const char* name, int minimum, int maximum, int value, QWidget* parent)
{
    auto* s = new QSpinBox(parent);
    s->setObjectName(QLatin1String(name));
    s->setRange(minimum, maximum);
    s->setValue(value);
    return s;
}

QDoubleSpinBox* doubleSpinBox(const char* name, double value, QWidget* parent)
{
    auto* s = new QDoubleSpinBox(parent);
    s->setObjectName(QLatin1String(name));
    s->setDecimals(3);
    s->setRange(-1000, 1000);
    s->setValue(value);
    return s;
}

QComboBox* comboBox(const char* name, QWidget* parent)
{
    auto* c = new QComboBox(parent);
    c->setObjectName(QLatin1String(name));
    c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    c->setMinimumContentsLength(8);
    return c;
}

QToolButton* toolButton(const char* name, QWidget* parent)
{
    auto* b = new QToolButton(parent);
    b->setObjectName(QLatin1String(name));
    b->setAutoRaise(true);
    return b;
}

} // namespace

ThrowCreatorDialog::ThrowCreatorDialog(Project* project, const QList<Project*>& projects, const QString& script,
                                       QWidget* parent)
    : QDialog(parent)
    , m_projects(projects)
    , m_script(script)
    , m_p1(std::make_unique<ProjectGraphics>(*project))
{
    setObjectName(QStringLiteral("ThrowDlg"));
    setWindowTitle(tr("Throw Creator"));
    if (!m_projects.contains(project))
        m_projects.prepend(project);

    m_preview = new AnimationPreview(this);
    m_preview->setObjectName(QStringLiteral("render"));
    auto* controls = new QWidget(this);
    controls->setObjectName(QStringLiteral("widget"));

    // P1: the attempt and the throw.
    auto* p1 = new QGroupBox(tr("P1:"), controls);
    p1->setObjectName(QStringLiteral("groupBox"));
    m_stateDef = spinBox("p1SDef", 0, 1000000, 0, p1);
    m_startAnim = comboBox("p1AnimStart", p1);
    m_throwAnim = comboBox("p1Anim", p1);
    auto* p1Layout = new QGridLayout(p1);
    p1Layout->addWidget(heading(tr("Start"), "label", p1), 0, 0, 1, 2);
    p1Layout->addWidget(label(tr("StateDef:"), "label_2", p1), 1, 0);
    p1Layout->addWidget(m_stateDef, 1, 1);
    p1Layout->addWidget(label(tr("Animation:"), "label_3", p1), 2, 0);
    p1Layout->addWidget(m_startAnim, 2, 1);
    p1Layout->addWidget(heading(tr("Throwing"), "label_5", p1), 3, 0, 1, 2);
    p1Layout->addWidget(label(tr("Animation:"), "label_4", p1), 4, 0);
    p1Layout->addWidget(m_throwAnim, 4, 1);
    p1Layout->setRowStretch(5, 1);
    p1Layout->setColumnStretch(1, 1);

    // Bind: where the opponent is held on each element.
    auto* bind = new QGroupBox(tr("Bind:"), controls);
    bind->setObjectName(QStringLiteral("groupBox_3"));
    m_elements = spinBox("elem", 0, 99, 0, bind);
    m_element = new QSlider(Qt::Horizontal, bind);
    m_element->setObjectName(QStringLiteral("horizontalSlider"));
    m_bindX = doubleSpinBox("bindX", 75, bind);
    m_bindY = doubleSpinBox("bindY", 0, bind);
    m_priority = spinBox("bindPriority", -99, 99, 0, bind);
    auto* bindLayout = new QGridLayout(bind);
    bindLayout->addWidget(label(tr("Stop on element #:"), "label_16", bind), 0, 0, 1, 2);
    bindLayout->addWidget(m_elements, 0, 2);
    bindLayout->addWidget(m_element, 1, 0, 1, 3);
    bindLayout->addWidget(label(tr("X:"), "label_13", bind), 2, 0);
    bindLayout->addWidget(m_bindX, 2, 1);
    bindLayout->addWidget(label(tr("Spr priority:"), "label_11", bind), 2, 2);
    bindLayout->addWidget(label(tr("Y:"), "label_12", bind), 3, 0);
    bindLayout->addWidget(m_bindY, 3, 1);
    bindLayout->addWidget(m_priority, 3, 2);
    bindLayout->setColumnStretch(1, 1);

    m_resolution = comboBox("res", controls);
    for (const char* size : {"320x240", "640x480", "800x600", "1024x768", "1280x720", "1920x1080"})
        m_resolution->addItem(QString::fromLatin1(size));
    m_resolution->setCurrentIndex(1);
    auto* insertButton = new QPushButton(tr("Insert"), controls);
    insertButton->setObjectName(QStringLiteral("ins"));
    insertButton->setDefault(true);
    auto* closeButton = new QPushButton(tr("Close"), controls);
    closeButton->setObjectName(QStringLiteral("close"));
    closeButton->setAutoDefault(false);

    // P2: the opponent, held then thrown.
    auto* p2 = new QGroupBox(tr("P2:"), controls);
    p2->setObjectName(QStringLiteral("groupBox_2"));
    m_opponents = comboBox("projs", p2);
    for (const Project* p : std::as_const(m_projects))
        m_opponents->addItem(p->name());
    m_xVelocity = spinBox("xVel", -65535, 65535, 0, p2);
    m_yVelocity = spinBox("yVel", -65535, 65535, 0, p2);
    m_thrownAnim = comboBox("p2Anim", p2);
    m_addAnim = toolButton("addAnim", p2);
    m_group = spinBox("grp", -1, 65535, 0, p2);
    m_index = spinBox("idx", -1, 65535, 0, p2);
    m_flip = comboBox("flipList", p2);
    m_flip->addItems({tr("No Flip"), tr("Flip H"), tr("Flip V"), tr("Flip HV")});
    m_insertFrame = toolButton("insFrame", p2);
    m_deleteFrame = toolButton("delFrame", p2);
    auto* p2Layout = new QGridLayout(p2);
    p2Layout->addWidget(m_opponents, 0, 0, 1, 4);
    p2Layout->addWidget(heading(tr("End"), "label_7", p2), 1, 0, 1, 4);
    p2Layout->addWidget(label(tr("X velocity:"), "label_8", p2), 2, 0);
    p2Layout->addWidget(m_xVelocity, 2, 1, 1, 3);
    p2Layout->addWidget(label(tr("Y velocity:"), "label_9", p2), 3, 0);
    p2Layout->addWidget(m_yVelocity, 3, 1, 1, 3);
    p2Layout->addWidget(heading(tr("Thrown"), "label_6", p2), 4, 0, 1, 4);
    p2Layout->addWidget(label(tr("Animation:"), "label_10", p2), 5, 0);
    p2Layout->addWidget(m_thrownAnim, 5, 1, 1, 2);
    p2Layout->addWidget(m_addAnim, 5, 3);
    p2Layout->addWidget(label(tr("Group:"), "label_14", p2), 6, 0);
    p2Layout->addWidget(label(tr("Index:"), "label_15", p2), 6, 1);
    p2Layout->addWidget(m_insertFrame, 6, 3);
    p2Layout->addWidget(m_group, 7, 0);
    p2Layout->addWidget(m_index, 7, 1);
    p2Layout->addWidget(m_flip, 7, 2);
    p2Layout->addWidget(m_deleteFrame, 7, 3);
    p2Layout->setColumnStretch(1, 1);
    p2Layout->setRowStretch(8, 1);

    auto* middle = new QVBoxLayout;
    middle->addWidget(bind);
    middle->addStretch(1);
    middle->addWidget(m_resolution);
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(insertButton);
    buttons->addWidget(closeButton);
    middle->addLayout(buttons);
    auto* row = new QHBoxLayout(controls);
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(p1, 345);
    row->addLayout(middle, 376);
    row->addWidget(p2, 427);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_preview, 1);
    layout->addWidget(controls);
    resize(804, 729);

    // All three are the project's own animations. The thrown one plays with the opponent's sprites.
    fillAnimations(m_startAnim, DefaultStartAnim);
    fillAnimations(m_throwAnim, DefaultThrowAnim);
    fillAnimations(m_thrownAnim, DefaultThrownAnim);
    const QString air = m_p1->animationFile();
    if (!air.isEmpty() && (project->file(air) || QFileInfo::exists(air)))
        m_air = project->codeFile(air, QStringLiteral("animations"));
    m_addAnim->setToolTip(tr("Add animation"));
    m_insertFrame->setToolTip(tr("Insert frame"));
    m_deleteFrame->setToolTip(tr("Remove current frame"));
    m_addAnim->setEnabled(m_air != nullptr);
    m_opponents->setCurrentIndex(int(m_projects.indexOf(project)));
    m_p2 = std::make_unique<ProjectGraphics>(*project);
    const AirAction* throwing = selectedAnimation(m_throwAnim);
    setElementCount(throwing ? int(throwing->frames.size()) : 0);

    connect(m_throwAnim, &QComboBox::currentIndexChanged, this, [this] {
        if (m_updating)
            return;
        const AirAction* action = selectedAnimation(m_throwAnim);
        setElementCount(action ? int(action->frames.size()) : 0);
    });
    connect(m_elements, &QSpinBox::valueChanged, this, [this](int count) {
        if (!m_updating)
            setElementCount(count);
    });
    connect(m_element, &QSlider::valueChanged, this, [this](int element) {
        if (!m_updating)
            showElement(element);
    });
    for (QDoubleSpinBox* box : {m_bindX, m_bindY})
        connect(box, &QDoubleSpinBox::valueChanged, this, &ThrowCreatorDialog::storeBind);
    connect(m_priority, &QSpinBox::valueChanged, this, &ThrowCreatorDialog::storeBind);
    connect(m_resolution, &QComboBox::currentIndexChanged, this, &ThrowCreatorDialog::applyResolution);
    connect(m_opponents, &QComboBox::currentIndexChanged, this, &ThrowCreatorDialog::setSpriteSource);
    connect(m_thrownAnim, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_updating)
            showElement(m_element->value());
    });
    for (QSpinBox* box : {m_group, m_index})
        connect(box, &QSpinBox::valueChanged, this, &ThrowCreatorDialog::editFrame);
    connect(m_flip, &QComboBox::currentIndexChanged, this, &ThrowCreatorDialog::editFrame);
    connect(m_addAnim, &QToolButton::clicked, this, &ThrowCreatorDialog::addAnimation);
    connect(m_insertFrame, &QToolButton::clicked, this, &ThrowCreatorDialog::insertFrame);
    connect(m_deleteFrame, &QToolButton::clicked, this, &ThrowCreatorDialog::deleteFrame);
    connect(insertButton, &QPushButton::clicked, this, &ThrowCreatorDialog::insert);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
}

ThrowCreatorDialog::~ThrowCreatorDialog() = default;

void ThrowCreatorDialog::setIcons(const std::function<QIcon(const QString&)>& icon)
{
    if (!icon)
        return;
    m_addAnim->setIcon(icon(QStringLiteral("anim_add")));
    m_insertFrame->setIcon(icon(QStringLiteral("anim_frame_add")));
    m_deleteFrame->setIcon(icon(QStringLiteral("anim_frame_delete")));
}

void ThrowCreatorDialog::fillAnimations(QComboBox* combo, int preferred) const
{
    combo->clear();
    int current = 0;
    bool found = false;
    for (const AirAction& action : m_p1->animations()) {
        if (action.number == preferred && !found) {
            current = combo->count();
            found = true;
        }
        combo->addItem(action.caption(), action.number);
    }
    combo->setCurrentIndex(combo->count() > 0 ? current : -1);
}

const AirAction* ThrowCreatorDialog::selectedAnimation(const QComboBox* combo) const
{
    if (combo->currentIndex() < 0)
        return nullptr;
    return findAnimation(m_p1->animations(), combo->currentData().toInt());
}

void ThrowCreatorDialog::setSpriteSource(int index)
{
    if (index < 0 || index >= m_projects.size())
        return;
    m_p2 = std::make_unique<ProjectGraphics>(*m_projects.at(index));
    updatePreview();
}

void ThrowCreatorDialog::setElementCount(int count)
{
    while (m_binds.size() < count)
        m_binds << ThrowBind();
    m_binds.resize(count);
    const bool updating = std::exchange(m_updating, true);
    m_elements->setValue(count);
    m_element->setRange(0, qMax(0, count - 1));
    m_updating = updating;
    showElement(m_element->value());
}

void ThrowCreatorDialog::showElement(int element)
{
    const bool updating = std::exchange(m_updating, true);
    const ThrowBind bind = element >= 0 && element < m_binds.size() ? m_binds.at(element) : ThrowBind();
    m_bindX->setValue(bind.x);
    m_bindY->setValue(bind.y);
    m_priority->setValue(bind.priority);

    const AirAction* throwing = selectedAnimation(m_throwAnim);
    const int count = throwing ? int(throwing->frames.size()) : 0;
    m_tick = count > 0 ? elementStart(*throwing, qMin(element, count - 1)) : 0;
    const AirAction* thrown = selectedAnimation(m_thrownAnim);
    m_frame = thrown ? elementAt(*thrown, m_tick) : -1;
    if (m_frame >= 0) {
        const AirFrame& frame = thrown->frames.at(m_frame);
        m_group->setValue(frame.group);
        m_index->setValue(frame.index);
        m_flip->setCurrentIndex((thrown->flipsHorizontally(m_frame) ? 1 : 0) +
                                (thrown->flipsVertically(m_frame) ? 2 : 0));
    }
    const bool editable = m_air && m_frame >= 0;
    for (QWidget* w : std::initializer_list<QWidget*>{m_group, m_index, m_flip, m_insertFrame, m_deleteFrame})
        w->setEnabled(editable);
    m_updating = updating;
    updatePreview();
}

void ThrowCreatorDialog::storeBind()
{
    if (m_updating)
        return;
    const int element = m_element->value();
    if (element < 0 || element >= m_binds.size())
        return;
    m_binds[element] = ThrowBind{m_bindX->value(), m_bindY->value(), m_priority->value()};
    updatePreview();
}

void ThrowCreatorDialog::updatePreview()
{
    if (m_updating)
        return;
    const int element = m_element->value();
    const double screen = m_preview->screenSize().width();
    QList<AnimationPreview::Figure> figures;

    AnimationPreview::Figure p1;
    p1.scale = screen / m_p1->localWidth();
    p1.cross = Qt::white;
    if (const AirAction* throwing = selectedAnimation(m_throwAnim); throwing && !throwing->frames.isEmpty()) {
        const int shown = qMin(element, int(throwing->frames.size()) - 1);
        const AirFrame& frame = throwing->frames.at(shown);
        p1.image = m_p1->sprite(frame.group, frame.index, &p1.axis);
        p1.flipH = throwing->flipsHorizontally(shown);
        p1.flipV = throwing->flipsVertically(shown);
        p1.position = QPointF(frame.x, frame.y) * p1.scale;
    }
    figures << p1;

    AnimationPreview::Figure p2;
    p2.scale = screen / m_p2->localWidth();
    p2.cross = QColor(40, 90, 255);
    const ThrowBind bind = element < m_binds.size() ? m_binds.at(element) : ThrowBind();
    p2.position = QPointF(bind.x, bind.y) * p1.scale;
    p2.image = m_p2->sprite(m_group->value(), m_index->value(), &p2.axis);
    const int flip = m_flip->currentIndex();
    p2.flipH = !(flip & 1);
    p2.flipV = (flip & 2) != 0;
    if (const AirAction* thrown = selectedAnimation(m_thrownAnim); thrown && m_frame >= 0) {
        const AirFrame& frame = thrown->frames.at(m_frame);
        p2.position += QPointF(-frame.x, frame.y) * p2.scale;
    }
    figures << p2;
    m_preview->setFigures(figures);
}

void ThrowCreatorDialog::applyResolution()
{
    const QStringList size = m_resolution->currentText().split(QLatin1Char('x'));
    m_preview->setScreenSize(QSize(size.value(0).toInt(), size.value(1).toInt()));
    updatePreview();
    const QPoint centre = geometry().center();
    QSize wanted = sizeHint().expandedTo(minimumSizeHint());
    const QRect available = screen() ? screen()->availableGeometry() : QRect();
    if (available.isValid())
        wanted = wanted.boundedTo(available.size());
    resize(wanted);
    if (isVisible() && available.isValid()) {
        QRect frame(QPoint(), frameGeometry().size());
        frame.moveCenter(centre);
        frame.moveLeft(
            qBound(available.left(), frame.left(), qMax(available.left(), available.right() - frame.width())));
        frame.moveTop(qBound(available.top(), frame.top(), qMax(available.top(), available.bottom() - frame.height())));
        move(frame.topLeft());
    }
}

void ThrowCreatorDialog::editFrame()
{
    if (m_updating || !m_air || m_frame < 0)
        return;
    const AirAction* thrown = selectedAnimation(m_thrownAnim);
    if (!thrown || m_frame >= thrown->frames.size())
        return;
    static const char* const flips[] = {"", "H", "V", "HV"};
    AirFrame frame = thrown->frames.at(m_frame);
    frame.group = m_group->value();
    frame.index = m_index->value();
    frame.flip = QString::fromLatin1(flips[qBound(0, m_flip->currentIndex(), 3)]);
    // Group/Index typing undoes like typing text in Fighter Factory: the first keystroke is one step and
    // later keystrokes in the same box join the second one.
    QTextDocument* document = m_air->document();
    QSpinBox* typing = m_group->hasFocus() ? m_group : (m_index->hasFocus() ? m_index : nullptr);
    const bool sameRun = typing && typing == m_typingIn && document->availableUndoSteps() == m_typingSteps;
    m_typedInRun = sameRun ? m_typedInRun + 1 : (typing ? 1 : 0);
    AirEdits::setFrame(document, frame, m_typedInRun > 2);
    m_typingIn = typing;
    m_typingSteps = document->availableUndoSteps();
    reloadAnimations();
}

void ThrowCreatorDialog::insertFrame()
{
    const AirAction* thrown = selectedAnimation(m_thrownAnim);
    if (m_air && thrown && AirEdits::splitFrame(m_air->document(), *thrown, m_tick))
        reloadAnimations();
}

void ThrowCreatorDialog::deleteFrame()
{
    const AirAction* thrown = selectedAnimation(m_thrownAnim);
    if (m_air && thrown && AirEdits::removeFrame(m_air->document(), *thrown, m_tick))
        reloadAnimations();
}

void ThrowCreatorDialog::addAnimation()
{
    if (!m_air)
        return;
    QList<int> taken;
    for (const AirAction& action : m_p1->animations())
        taken << action.number;
    AddAnimationDialog dialog(taken, m_thrownAnim->currentIndex() >= 0 ? m_thrownAnim->currentData().toInt() + 1 : 0,
                              this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    AirEdits::appendAnimation(m_air->document(), dialog.number(), dialog.name(), dialog.time());
    reloadAnimations();
    const int added = m_thrownAnim->findData(dialog.number());
    if (added >= 0)
        m_thrownAnim->setCurrentIndex(added);
}

void ThrowCreatorDialog::reloadAnimations()
{
    m_p1->reloadAnimations();
    const bool updating = std::exchange(m_updating, true);
    for (QComboBox* combo : {m_startAnim, m_throwAnim, m_thrownAnim})
        fillAnimations(combo, combo->currentData().toInt());
    m_updating = updating;
    showElement(m_element->value());
}

ThrowSettings ThrowCreatorDialog::settings() const
{
    ThrowSettings settings;
    settings.stateNo = m_stateDef->value();
    settings.startAnim = m_startAnim->currentData().toInt();
    settings.throwAnim = m_throwAnim->currentData().toInt();
    settings.thrownAnim = m_thrownAnim->currentData().toInt();
    settings.xVelocity = m_xVelocity->value();
    settings.yVelocity = m_yVelocity->value();
    settings.binds = m_binds;
    return settings;
}

void ThrowCreatorDialog::insert()
{
    QString error;
    const QString text = throwStates(m_script, settings(), &error);
    if (text.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), error);
        return;
    }
    if (m_insert)
        m_insert(text);
    accept();
}

} // namespace OFS::Mugen
