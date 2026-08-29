#include "shell/dialogs/SoundDialogs.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
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

} // namespace

AddItemsDialog::AddItemsDialog(Kind kind, QWidget* parent)
    : QDialog(parent)
    , m_group(new QSpinBox(this))
{
    setObjectName(kind == Kind::Sounds ? QStringLiteral("AddSom") : QStringLiteral("AddPal"));
    setWindowTitle(tr("Add"));
    if (kind != Kind::EditorPalettes) {
        m_detect = new QCheckBox(tr("Detect group/index from filename"), this);
        m_detect->setObjectName(QStringLiteral("detect"));
    }
    m_group->setObjectName(QStringLiteral("Grp"));
    m_group->setRange(0, 65535);
    auto* indexBox = new QGroupBox(tr("Index:"), this);
    indexBox->setObjectName(QStringLiteral("groupBox"));
    m_number = new QSpinBox(indexBox);
    m_number->setObjectName(QStringLiteral("Num"));
    m_number->setRange(0, 65535);
    auto* one = new QRadioButton(tr("1,2,3,4,..."), indexBox);
    one->setObjectName(QStringLiteral("OneInc"));
    one->setChecked(true);
    m_ten = new QRadioButton(tr("10,20,30,..."), indexBox);
    m_ten->setObjectName(QStringLiteral("TenInc"));
    auto* indexLayout = new QVBoxLayout(indexBox);
    indexLayout->addWidget(m_number);
    indexLayout->addWidget(one);
    indexLayout->addWidget(m_ten);
    auto* layout = new QVBoxLayout(this);
    if (m_detect)
        layout->addWidget(m_detect);
    auto* groupRow = new QHBoxLayout;
    groupRow->addWidget(new QLabel(tr("Group:"), this));
    groupRow->addWidget(m_group, 1);
    layout->addLayout(groupRow);
    if (kind != Kind::Sounds) {
        m_reversed = new QCheckBox(tr("Reversed"), this);
        m_reversed->setObjectName(QStringLiteral("rev"));
        layout->addWidget(m_reversed);
    }
    layout->addWidget(indexBox);
    layout->addStretch();
    layout->addWidget(okCancel(this));
    resize(297, 257);
}

bool AddItemsDialog::detect() const
{
    return m_detect && m_detect->isChecked();
}

int AddItemsDialog::group() const
{
    return m_group->value();
}

int AddItemsDialog::number() const
{
    return m_number->value();
}

int AddItemsDialog::step() const
{
    return m_ten->isChecked() ? 10 : 1;
}

bool AddItemsDialog::reversed() const
{
    return m_reversed && m_reversed->isChecked();
}

QList<int> ResampleDialog::sampleRates()
{
    return {8000, 11000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 96000, 192000};
}

ResampleDialog::ResampleDialog(const QStringList& sounds, int current, int channels, int sampleRate, int bitsPerSample,
                               QWidget* parent)
    : QDialog(parent)
    , m_list(new QListWidget(this))
    , m_channels(new QComboBox(this))
    , m_rate(new QComboBox(this))
    , m_bits(new QComboBox(this))
{
    setObjectName(QStringLiteral("ResampleDlg"));
    setWindowTitle(tr("Resample"));
    m_list->setObjectName(QStringLiteral("list"));
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->addItems(sounds);
    if (current >= 0 && current < m_list->count()) {
        m_list->item(current)->setSelected(true);
        m_list->scrollToItem(m_list->item(current));
    }
    m_channels->setObjectName(QStringLiteral("channels"));
    m_channels->addItems({tr("Mono"), tr("Stereo")});
    m_channels->setCurrentIndex(channels >= 2 ? 1 : 0);
    m_rate->setObjectName(QStringLiteral("rate"));
    for (int rate : sampleRates())
        m_rate->addItem(tr("%1 Hz").arg(rate), rate);
    const int rateIndex = m_rate->findData(sampleRate);
    if (rateIndex >= 0) {
        m_rate->setCurrentIndex(rateIndex);
    } else {
        m_rate->addItem(tr("%1 Hz").arg(sampleRate), sampleRate);
        m_rate->setCurrentIndex(m_rate->count() - 1);
    }
    m_bits->setObjectName(QStringLiteral("bits"));
    m_bits->addItem(tr("8 bits"), 8);
    m_bits->addItem(tr("16 bits"), 16);
    m_bits->setCurrentIndex(bitsPerSample == 8 ? 0 : 1);
    auto button = [this](const QString& text, const char* name) {
        auto* b = new QPushButton(text, this);
        b->setObjectName(QLatin1String(name));
        b->setAutoDefault(false);
        return b;
    };
    QPushButton* clear = button(tr("Clear"), "clearBut");
    QPushButton* invert = button(tr("Invert selection"), "invBut");
    QPushButton* all = button(tr("Select all"), "allBut");
    connect(clear, &QPushButton::clicked, m_list, &QListWidget::clearSelection);
    connect(all, &QPushButton::clicked, m_list, &QListWidget::selectAll);
    connect(invert, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < m_list->count(); ++i)
            m_list->item(i)->setSelected(!m_list->item(i)->isSelected());
    });
    auto* format = new QGridLayout;
    format->addWidget(new QLabel(tr("Channels:"), this), 0, 0);
    format->addWidget(m_channels, 0, 1);
    format->addWidget(new QLabel(tr("Sample rate:"), this), 1, 0);
    format->addWidget(m_rate, 1, 1);
    format->addWidget(new QLabel(tr("Bits per sample:"), this), 2, 0);
    format->addWidget(m_bits, 2, 1);
    auto* selection = new QHBoxLayout;
    selection->addWidget(clear);
    selection->addWidget(invert);
    selection->addWidget(all);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_list, 1);
    layout->addLayout(selection);
    layout->addLayout(format);
    layout->addWidget(okCancel(this));
    resize(367, 349);
}

QList<int> ResampleDialog::sounds() const
{
    QList<int> rows;
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->isSelected())
            rows << i;
    }
    return rows;
}

int ResampleDialog::channels() const
{
    return m_channels->currentIndex() + 1;
}

int ResampleDialog::sampleRate() const
{
    return m_rate->currentData().toInt();
}

int ResampleDialog::bitsPerSample() const
{
    return m_bits->currentData().toInt();
}

} // namespace OFS
