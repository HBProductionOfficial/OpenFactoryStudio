#include "shell/dialogs/MugenOptionsPage.h"

#include "core/app/AppContext.h"

#include "core/settings/Config.h"
#include "core/settings/ShortcutSettings.h"
#include "shell/dialogs/RunConfigDialog.h"

#include <QColorDialog>
#include <QEvent>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>

namespace OFS {

namespace {

const QString kAttackKey = QStringLiteral("clsn_default_attack");
const QString kCollisionKey = QStringLiteral("clsn_default_collision");

// Built-in defaults, used when data/engines/mugen/config.xml lacks a key.
const QHash<QString, QString>& builtinDefaults()
{
    static const QHash<QString, QString> values = {
        {kAttackKey, QStringLiteral("255,128,0")},
        {kCollisionKey, QStringLiteral("0,255,255")},
        {QStringLiteral("p1_left"), QStringLiteral("A")},
        {QStringLiteral("p1_right"), QStringLiteral("D")},
        {QStringLiteral("p1_up"), QStringLiteral("W")},
        {QStringLiteral("p1_down"), QStringLiteral("S")},
        {QStringLiteral("p1_a"), QStringLiteral("F")},
        {QStringLiteral("p1_b"), QStringLiteral("G")},
        {QStringLiteral("p1_c"), QStringLiteral("H")},
        {QStringLiteral("p1_x"), QStringLiteral("R")},
        {QStringLiteral("p1_y"), QStringLiteral("T")},
        {QStringLiteral("p1_z"), QStringLiteral("Y")},
        {QStringLiteral("p1_start"), QStringLiteral("U")},
        {QStringLiteral("p2_left"), QStringLiteral("Left")},
        {QStringLiteral("p2_right"), QStringLiteral("Right")},
        {QStringLiteral("p2_up"), QStringLiteral("Up")},
        {QStringLiteral("p2_down"), QStringLiteral("Down")},
        {QStringLiteral("p2_a"), QStringLiteral("4")},
        {QStringLiteral("p2_b"), QStringLiteral("5")},
        {QStringLiteral("p2_c"), QStringLiteral("6")},
        {QStringLiteral("p2_x"), QStringLiteral("7")},
        {QStringLiteral("p2_y"), QStringLiteral("8")},
        {QStringLiteral("p2_z"), QStringLiteral("9")},
        {QStringLiteral("p2_start"), QStringLiteral("0")},
    };
    return values;
}

QColor parseColor(const QString& text, const QColor& fallback)
{
    const QStringList parts = text.split(QLatin1Char(','));
    if (parts.size() != 3)
        return fallback;
    int v[3] = {0, 0, 0};
    for (int i = 0; i < 3; ++i) {
        bool ok = false;
        v[i] = parts[i].trimmed().toInt(&ok);
        if (!ok || v[i] < 0 || v[i] > 255)
            return fallback;
    }
    return QColor(v[0], v[1], v[2]);
}

QString formatColor(const QColor& color)
{
    return QStringLiteral("%1,%2,%3").arg(color.red()).arg(color.green()).arg(color.blue());
}

QString controlCaption(const QString& name)
{
    if (name == QLatin1String("left"))
        return MugenOptionsPage::tr("Left");
    if (name == QLatin1String("right"))
        return MugenOptionsPage::tr("Right");
    if (name == QLatin1String("up"))
        return MugenOptionsPage::tr("Up");
    if (name == QLatin1String("down"))
        return MugenOptionsPage::tr("Down");
    if (name == QLatin1String("start"))
        return MugenOptionsPage::tr("Start");
    return name.toUpper();
}

} // namespace

MugenOptionsPage::MugenOptionsPage(Config& config, QWidget* parent)
    : OptionsPage(parent)
    , m_config(config)
{
    setObjectName(QStringLiteral("engineTab"));

    auto* banner = new QLabel(tr("Attention! All settings here auto apply when changed."), this);
    banner->setObjectName(QStringLiteral("eng_msg_lb"));
    banner->setAlignment(Qt::AlignCenter);
    banner->setMinimumHeight(27);
    // A warning note: the same yellow with black text on every theme.
    banner->setStyleSheet(QStringLiteral("QLabel#eng_msg_lb { background-color: #fff9a0; color: #000000; }"));

    auto* controls = new QHBoxLayout;
    controls->addWidget(createControlsGroup(1));
    controls->addWidget(createControlsGroup(2));

    auto* right = new QVBoxLayout;
    right->addWidget(createClsnGroup());
    right->addLayout(controls);
    right->addStretch(1);

    auto* body = new QHBoxLayout;
    body->addWidget(createRunGroup());
    body->addSpacing(8);
    body->addLayout(right, 1);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(banner);
    layout->addLayout(body, 1);

    load();
}

QString MugenOptionsPage::title() const
{
    return QStringLiteral("MUGENext");
}

QStringList MugenOptionsPage::controlNames()
{
    return {QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("up"),   QStringLiteral("down"),
            QStringLiteral("a"),    QStringLiteral("b"),     QStringLiteral("c"),    QStringLiteral("x"),
            QStringLiteral("y"),    QStringLiteral("z"),     QStringLiteral("start")};
}

QString MugenOptionsPage::defaultValue(const QString& key)
{
    return builtinDefaults().value(key);
}

QWidget* MugenOptionsPage::createRunGroup()
{
    auto* group = new QGroupBox(tr("Run configurations:"), this);
    group->setObjectName(QStringLiteral("runGrp"));
    group->setFixedWidth(276);

    auto* add = new QPushButton(tr("Add"), group);
    m_remove = new QPushButton(tr("Remove"), group);
    m_up = new QPushButton(QStringLiteral("˄"), group);
    m_edit = new QPushButton(tr("Edit"), group);
    m_clear = new QPushButton(tr("Clear"), group);
    m_down = new QPushButton(QStringLiteral("˅"), group);
    m_runList = new QListWidget(group);
    add->setObjectName(QStringLiteral("engAdd"));
    m_remove->setObjectName(QStringLiteral("engDel"));
    m_up->setObjectName(QStringLiteral("engUp"));
    m_edit->setObjectName(QStringLiteral("engEdit"));
    m_clear->setObjectName(QStringLiteral("engClear"));
    m_down->setObjectName(QStringLiteral("engDown"));
    m_runList->setObjectName(QStringLiteral("engineList"));
    for (QPushButton* button : {add, m_remove, m_up, m_edit, m_clear, m_down})
        button->setAutoDefault(false);
    m_up->setFixedWidth(50);
    m_down->setFixedWidth(50);

    auto* grid = new QGridLayout(group);
    grid->addWidget(add, 0, 0);
    grid->addWidget(m_remove, 0, 1);
    grid->addWidget(m_up, 0, 2);
    grid->addWidget(m_edit, 1, 0);
    grid->addWidget(m_clear, 1, 1);
    grid->addWidget(m_down, 1, 2);
    grid->addWidget(m_runList, 2, 0, 1, 3);

    connect(add, &QPushButton::clicked, this, &MugenOptionsPage::addClicked);
    connect(m_edit, &QPushButton::clicked, this, &MugenOptionsPage::editClicked);
    connect(m_runList, &QListWidget::itemDoubleClicked, this, &MugenOptionsPage::editClicked);
    connect(m_remove, &QPushButton::clicked, this, [this] { removeRunConfiguration(currentRunConfiguration()); });
    connect(m_clear, &QPushButton::clicked, this, &MugenOptionsPage::clearClicked);
    connect(m_up, &QPushButton::clicked, this, [this] { moveRunConfiguration(currentRunConfiguration(), -1); });
    connect(m_down, &QPushButton::clicked, this, [this] { moveRunConfiguration(currentRunConfiguration(), 1); });
    connect(m_runList, &QListWidget::currentRowChanged, this, &MugenOptionsPage::updateRunButtons);
    return group;
}

QWidget* MugenOptionsPage::createClsnGroup()
{
    auto* group = new QGroupBox(tr("CLSN Default color:"), this);
    m_attack = new QLabel(tr("Attack"), group);
    m_collision = new QLabel(tr("Collision"), group);
    m_attack->setObjectName(QStringLiteral("clsnAttack"));
    m_collision->setObjectName(QStringLiteral("clsnCollision"));
    auto* layout = new QVBoxLayout(group);
    for (QLabel* label : {m_attack, m_collision}) {
        label->setAlignment(Qt::AlignCenter);
        label->setMinimumHeight(21);
        label->setCursor(Qt::PointingHandCursor);
        label->installEventFilter(this);
        layout->addWidget(label);
    }
    return group;
}

QWidget* MugenOptionsPage::createControlsGroup(int player)
{
    auto* group = new QGroupBox(player == 1 ? tr("P1 controls:") : tr("P2 controls:"), this);
    auto* grid = new QGridLayout(group);
    grid->setColumnStretch(1, 1);
    int row = 0;
    for (const QString& name : controlNames()) {
        const QString key = QStringLiteral("p%1_%2").arg(player).arg(name);
        auto* edit = new QKeySequenceEdit(group);
        edit->setObjectName(key);
        edit->setMaximumSequenceLength(1);
        auto* label = new QLabel(controlCaption(name), group);
        label->setBuddy(edit);
        grid->addWidget(label, row, 0);
        grid->addWidget(edit, row, 1);
        connect(edit, &QKeySequenceEdit::keySequenceChanged, this, [this, key](const QKeySequence& keys) {
            if (!m_loading)
                store(key, ShortcutSettings::toText(keys));
        });
        m_keys.insert(key, edit);
        ++row;
    }
    return group;
}

void MugenOptionsPage::load()
{
    m_loading = true;
    for (auto it = m_keys.cbegin(); it != m_keys.cend(); ++it)
        it.value()->setKeySequence(ShortcutSettings::fromText(m_config.value(it.key(), defaultValue(it.key()))));
    m_runs = RunConfiguration::load(m_config);
    m_loading = false;
    refreshRunList(m_runs.isEmpty() ? -1 : 0);
    updateClsnLabels();
}

void MugenOptionsPage::loadDefaults()
{
    QStringList keys = m_keys.keys();
    keys << kAttackKey << kCollisionKey;
    for (const QString& key : std::as_const(keys))
        m_config.setValue(key, m_config.hasDefault(key) ? m_config.defaultValue(key) : defaultValue(key));
    save();
    load();
    Q_EMIT changed();
}

void MugenOptionsPage::apply()
{
    // Changes are stored as they are made.
}

QMap<QString, QString> MugenOptionsPage::values() const
{
    QMap<QString, QString> result = m_config.allValues();
    for (auto it = builtinDefaults().cbegin(); it != builtinDefaults().cend(); ++it) {
        if (!result.contains(it.key()))
            result.insert(it.key(), it.value());
    }
    return result;
}

void MugenOptionsPage::setValues(const QMap<QString, QString>& values)
{
    m_config.merge(values);
    save();
    load();
    Q_EMIT changed();
}

void MugenOptionsPage::store(const QString& key, const QString& value)
{
    m_config.setValue(key, value);
    save();
    Q_EMIT changed();
}

void MugenOptionsPage::save()
{
    // Run configurations are written as <engine> elements, same as Fighter Factory.
    if (!m_config.userPath().isEmpty())
        RunConfiguration::writeEngineSettings(m_config, m_config.userPath());
}

void MugenOptionsPage::setRunConfigurations(const QList<RunConfiguration>& runs)
{
    m_runs = runs;
    RunConfiguration::save(m_config, m_runs);
    save();
    refreshRunList(m_runs.isEmpty() ? -1 : 0);
    Q_EMIT changed();
}

void MugenOptionsPage::addRunConfiguration(const RunConfiguration& run)
{
    m_runs << run;
    RunConfiguration::save(m_config, m_runs);
    save();
    refreshRunList(int(m_runs.size()) - 1);
    Q_EMIT changed();
}

void MugenOptionsPage::removeRunConfiguration(int index)
{
    if (index < 0 || index >= int(m_runs.size()))
        return;
    m_runs.removeAt(index);
    RunConfiguration::save(m_config, m_runs);
    save();
    refreshRunList(qMin(index, int(m_runs.size()) - 1));
    Q_EMIT changed();
}

void MugenOptionsPage::moveRunConfiguration(int index, int delta)
{
    const int target = index + delta;
    if (index < 0 || index >= int(m_runs.size()) || target < 0 || target >= int(m_runs.size()))
        return;
    m_runs.move(index, target);
    RunConfiguration::save(m_config, m_runs);
    save();
    refreshRunList(target);
    Q_EMIT changed();
}

int MugenOptionsPage::currentRunConfiguration() const
{
    return m_runList->currentRow();
}

void MugenOptionsPage::refreshRunList(int select)
{
    m_runList->clear();
    for (const RunConfiguration& run : std::as_const(m_runs)) {
        auto* item = new QListWidgetItem(run.name, m_runList);
        item->setToolTip(run.executable);
        item->setIcon(run.displayIcon());
    }
    m_runList->setCurrentRow(select);
    updateRunButtons();
}

void MugenOptionsPage::updateRunButtons()
{
    const int row = m_runList->currentRow();
    const int count = int(m_runs.size());
    const bool selected = row >= 0 && row < count;
    m_remove->setEnabled(selected);
    m_edit->setEnabled(selected);
    m_up->setEnabled(selected && row > 0);
    m_down->setEnabled(selected && row < count - 1);
    m_clear->setEnabled(count > 0);
}

void MugenOptionsPage::addClicked()
{
    RunConfigDialog dialog({}, false, this);
    if (dialog.exec() == QDialog::Accepted)
        addRunConfiguration(dialog.value());
}

void MugenOptionsPage::editClicked()
{
    const int row = currentRunConfiguration();
    if (row < 0 || row >= int(m_runs.size()))
        return;
    RunConfigDialog dialog(m_runs[row], true, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_runs[row] = dialog.value();
    RunConfiguration::save(m_config, m_runs);
    save();
    refreshRunList(row);
    Q_EMIT changed();
}

void MugenOptionsPage::clearClicked()
{
    if (m_runs.isEmpty())
        return;
    const AppContext* context = AppContext::instance();
    if ((!context || context->config().boolValue(QStringLiteral("confirm_delete"), true)) &&
        QMessageBox::question(this, tr("Run configurations"), tr("Remove every run configuration?")) !=
            QMessageBox::Yes)
        return;
    setRunConfigurations({});
}

QColor MugenOptionsPage::clsnColor(bool attack) const
{
    const QString& key = attack ? kAttackKey : kCollisionKey;
    return parseColor(m_config.value(key, defaultValue(key)), parseColor(defaultValue(key), Qt::white));
}

void MugenOptionsPage::setClsnColor(bool attack, const QColor& color)
{
    if (!color.isValid())
        return;
    store(attack ? kAttackKey : kCollisionKey, formatColor(color));
    updateClsnLabels();
}

void MugenOptionsPage::updateClsnLabels()
{
    for (bool attack : {true, false}) {
        const QColor color = clsnColor(attack);
        const QString text = qGray(color.rgb()) > 128 ? QStringLiteral("#000000") : QStringLiteral("#ffffff");
        QLabel* label = attack ? m_attack : m_collision;
        label->setStyleSheet(
            QStringLiteral("QLabel { background-color: %1; color: %2; }").arg(color.name(QColor::HexRgb), text));
    }
}

void MugenOptionsPage::chooseClsnColor(bool attack)
{
    const QColor color = QColorDialog::getColor(clsnColor(attack), this, attack ? tr("Attack") : tr("Collision"));
    if (color.isValid())
        setClsnColor(attack, color);
}

bool MugenOptionsPage::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == m_attack || watched == m_collision) && event->type() == QEvent::MouseButtonRelease &&
        static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
        chooseClsnColor(watched == m_attack);
        return true;
    }
    return OptionsPage::eventFilter(watched, event);
}

} // namespace OFS
