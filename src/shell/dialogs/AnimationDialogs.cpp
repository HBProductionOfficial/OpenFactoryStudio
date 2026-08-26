#include "shell/dialogs/AnimationDialogs.h"

#include "core/code/CodeEditor.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QSlider>
#include <QSpinBox>
#include <QTextDocument>
#include <QVBoxLayout>

namespace OFS {

namespace {

QDialogButtonBox* okCancel(QDialog* dialog)
{
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    buttons->setObjectName(QStringLiteral("buttonBox"));
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    return buttons;
}

QPushButton* button(const QString& text, const char* name, QWidget* parent)
{
    auto* b = new QPushButton(text, parent);
    b->setObjectName(QLatin1String(name));
    b->setAutoDefault(false);
    return b;
}

} // namespace

AddAnimationDialog::AddAnimationDialog(QWidget* parent)
    : QDialog(parent)
    , m_name(new QLineEdit(this))
    , m_number(new QSpinBox(this))
    , m_time(new QSpinBox(this))
{
    setObjectName(QStringLiteral("AddAnim"));
    setWindowTitle(tr("Add animation"));
    m_name->setObjectName(QStringLiteral("name"));
    m_number->setObjectName(QStringLiteral("number"));
    m_time->setObjectName(QStringLiteral("time"));
    for (QSpinBox* spin : {m_number, m_time}) {
        spin->setRange(-300000, 300000);
        spin->setValue(0);
    }
    auto* form = new QGridLayout;
    form->addWidget(new QLabel(tr("Name:"), this), 0, 0);
    form->addWidget(m_name, 0, 1);
    form->addWidget(new QLabel(tr("Number:"), this), 1, 0);
    form->addWidget(m_number, 1, 1);
    form->addWidget(new QLabel(tr("Time:"), this), 2, 0);
    form->addWidget(m_time, 2, 1);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addStretch();
    layout->addWidget(okCancel(this));
    resize(286, 191);
}

QString AddAnimationDialog::name() const
{
    return m_name->text().trimmed();
}

int AddAnimationDialog::number() const
{
    return m_number->value();
}

int AddAnimationDialog::time() const
{
    return m_time->value();
}

AnimationAlignDialog::AnimationAlignDialog(const AirDocument& doc, int current, QWidget* parent)
    : QDialog(parent)
    , m_operation(new QComboBox(this))
    , m_x(new QSpinBox(this))
    , m_y(new QSpinBox(this))
    , m_list(new QListWidget(this))
{
    setObjectName(QStringLiteral("AnimAlign"));
    setWindowTitle(tr("Alignment Tool"));
    m_operation->setObjectName(QStringLiteral("mode"));
    m_operation->addItems({tr("Offset axis"), tr("Offset Contact boxes"), tr("Resize Contact boxes (top-left)"),
                           tr("Resize Contact boxes (bottom-right)"), tr("Scale Contact boxes")});
    m_x->setObjectName(QStringLiteral("xVal"));
    m_y->setObjectName(QStringLiteral("yVal"));
    for (QSpinBox* spin : {m_x, m_y})
        spin->setRange(-30000, 30000);
    m_list->setObjectName(QStringLiteral("list"));
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    for (const AirAnimation& animation : doc.animations())
        m_list->addItem(animation.caption());
    if (current >= 0 && current < m_list->count()) {
        m_list->item(current)->setSelected(true);
        m_list->scrollToItem(m_list->item(current), QAbstractItemView::PositionAtTop);
    }
    QPushButton* apply = button(tr("Apply"), "pushButton", this);
    apply->setDefault(true);
    QPushButton* close = button(tr("Close"), "pushButton_2", this);
    QPushButton* clear = button(tr("Clear selection"), "pushButton_4", this);
    QPushButton* invert = button(tr("Invert selection"), "pushButton_3", this);
    QPushButton* all = button(tr("Select all"), "pushButton_5", this);

    auto* form = new QGridLayout;
    form->addWidget(new QLabel(tr("Operation:"), this), 0, 0);
    form->addWidget(m_operation, 0, 1, 1, 3);
    form->addWidget(new QLabel(tr("X:"), this), 1, 0);
    form->addWidget(m_x, 1, 1);
    form->addWidget(new QLabel(tr("Y:"), this), 1, 2);
    form->addWidget(m_y, 1, 3);
    auto* left = new QVBoxLayout;
    left->addLayout(form);
    left->addStretch();
    auto* actions = new QHBoxLayout;
    actions->addWidget(apply);
    actions->addWidget(close);
    left->addLayout(actions);
    auto* right = new QVBoxLayout;
    right->addWidget(m_list, 1);
    auto* selection = new QHBoxLayout;
    selection->addWidget(clear);
    selection->addWidget(invert);
    selection->addWidget(all);
    right->addLayout(selection);
    auto* layout = new QHBoxLayout(this);
    layout->addLayout(left);
    layout->addLayout(right, 1);
    resize(618, 305);

    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(clear, &QPushButton::clicked, m_list, &QListWidget::clearSelection);
    connect(all, &QPushButton::clicked, m_list, &QListWidget::selectAll);
    connect(invert, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < m_list->count(); ++i)
            m_list->item(i)->setSelected(!m_list->item(i)->isSelected());
    });
    connect(apply, &QPushButton::clicked, this, [this] {
        QList<int> animations;
        for (int i = 0; i < m_list->count(); ++i) {
            if (m_list->item(i)->isSelected())
                animations << i;
        }
        Q_EMIT applyRequested(animations, AirEdits::AlignOperation(m_operation->currentIndex()), m_x->value(),
                              m_y->value());
    });
}

GifOptionsDialog::GifOptionsDialog(QWidget* parent)
    : QDialog(parent)
    , m_label(new QLabel(this))
    , m_scale(new QSlider(Qt::Horizontal, this))
    , m_velocity(new QDoubleSpinBox(this))
{
    setObjectName(QStringLiteral("GIFDialog"));
    setWindowTitle(tr("GIF Options"));
    m_label->setObjectName(QStringLiteral("label"));
    m_scale->setObjectName(QStringLiteral("scale"));
    m_scale->setRange(0, 10);
    m_scale->setValue(1);
    m_velocity->setObjectName(QStringLiteral("velocity"));
    m_velocity->setRange(0.1, 10.0);
    m_velocity->setDecimals(2);
    m_velocity->setSingleStep(0.1);
    m_velocity->setValue(1.0);
    auto updateLabel = [this] {
        m_label->setText(
            tr("Scale: %1").arg(m_scale->value() == 0 ? QStringLiteral("0.5") : QString::number(m_scale->value())));
    };
    updateLabel();
    connect(m_scale, &QSlider::valueChanged, this, updateLabel);
    auto* form = new QGridLayout;
    form->addWidget(m_label, 0, 0, 1, 2);
    form->addWidget(m_scale, 1, 0, 1, 2);
    form->addWidget(new QLabel(tr("Velocity:"), this), 2, 0);
    form->addWidget(m_velocity, 2, 1);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addStretch();
    layout->addWidget(okCancel(this));
    resize(276, 183);
}

double GifOptionsDialog::scale() const
{
    return m_scale->value() == 0 ? 0.5 : double(m_scale->value());
}

double GifOptionsDialog::velocity() const
{
    return m_velocity->value();
}

AnimationCodeWindow::AnimationCodeWindow(const QStringList& text, QWidget* parent)
    : QDialog(parent)
    , m_editor(new CodeEditor(this))
{
    setObjectName(QStringLiteral("AnimEditTxt"));
    setWindowTitle(tr("Edit animation code"));
    setModal(false);
    m_editor->setPlainText(text.join(QLatin1Char('\n')));
    m_editor->document()->setModified(false);
    QPushButton* close = button(tr("Close"), "pushButton", this);
    QPushButton* apply = button(tr("Apply"), "pushButton_2", this);
    apply->setDefault(true);
    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(apply);
    buttons->addWidget(close);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_editor, 1);
    layout->addLayout(buttons);
    resize(404, 640);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    connect(apply, &QPushButton::clicked, this, [this] {
        Q_EMIT applyRequested(this->text());
        m_editor->document()->setModified(false);
    });
}

QStringList AnimationCodeWindow::text() const
{
    return m_editor->toPlainText().split(QLatin1Char('\n'));
}

bool AnimationCodeWindow::isModified() const
{
    return m_editor->document()->isModified();
}

bool AnimationCodeWindow::confirmClose()
{
    if (!isModified())
        return true;
    return QMessageBox::question(this, tr("Discard changes?"),
                                 tr("Do you want to discard the changes made in the animation code?"),
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes;
}

void AnimationCodeWindow::closeEvent(QCloseEvent* event)
{
    if (!confirmClose()) {
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}

void AnimationCodeWindow::reject()
{
    if (confirmClose())
        QDialog::reject();
}

} // namespace OFS
