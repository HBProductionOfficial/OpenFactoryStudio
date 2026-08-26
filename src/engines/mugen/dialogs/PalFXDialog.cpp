#include "engines/mugen/dialogs/PalFXDialog.h"

#include "core/sprites/SpriteFile.h"

#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QXmlStreamReader>

#include <cmath>
#include <numbers>

namespace OFS::Mugen {

QRgb PalFX::apply(QRgb rgb, int tick) const
{
    double c[3] = {double(qRed(rgb)), double(qGreen(rgb)), double(qBlue(rgb))};
    const double gray = (c[0] + c[1] + c[2]) / 3.0;
    const double wave = period > 0 ? std::sin(2.0 * std::numbers::pi * tick / period) : 0.0;
    int out[3];
    for (int i = 0; i < 3; ++i) {
        double v = gray + (c[i] - gray) * color / 256.0;
        if (invert)
            v = 255.0 - v;
        v = v * mul[i] / 256.0 + add[i] + sin[i] * wave;
        out[i] = std::clamp(int(std::lround(v)), 0, 255);
    }
    return qRgba(out[0], out[1], out[2], qAlpha(rgb));
}

QString PalFX::controllerText() const
{
    auto triple = [](const int values[3]) {
        return QStringLiteral("%1,%2,%3").arg(values[0]).arg(values[1]).arg(values[2]);
    };
    return QStringLiteral("[State 0, PalFXWiz]\ntype = PalFX\ntrigger1 = \ntime = \nadd = %1\nmul = %2\n"
                          "sinadd = %3,%4\ninvertall = %5\ncolor = %6\n")
        .arg(triple(add), triple(mul), triple(sin))
        .arg(period)
        .arg(invert ? 1 : 0)
        .arg(color);
}

bool readPalFXTemplate(const QString& path, PalFX* palfx)
{
    // <palfx><add red green blue/><mul .../><sin ... period/><color value/><invert value/></palfx>
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QXmlStreamReader xml(&file);
    if (!xml.readNextStartElement() || xml.name() != QLatin1String("palfx"))
        return false;
    PalFX result;
    while (xml.readNextStartElement()) {
        const QXmlStreamAttributes attributes = xml.attributes();
        auto rgb = [&attributes](int values[3]) {
            const char* const names[] = {"red", "green", "blue"};
            for (int i = 0; i < 3; ++i) {
                if (attributes.hasAttribute(QLatin1String(names[i])))
                    values[i] = attributes.value(QLatin1String(names[i])).toInt();
            }
        };
        if (xml.name() == QLatin1String("add")) {
            rgb(result.add);
        } else if (xml.name() == QLatin1String("mul")) {
            rgb(result.mul);
        } else if (xml.name() == QLatin1String("sin")) {
            rgb(result.sin);
            if (attributes.hasAttribute(QLatin1String("period")))
                result.period = std::max(1, attributes.value(QLatin1String("period")).toInt());
        } else if (xml.name() == QLatin1String("color")) {
            result.color = attributes.value(QLatin1String("value")).toInt();
        } else if (xml.name() == QLatin1String("invert")) {
            result.invert = attributes.value(QLatin1String("value")) == QLatin1String("true");
        }
        xml.skipCurrentElement();
    }
    if (xml.hasError())
        return false;
    *palfx = result;
    return true;
}

PalFXDialog::PalFXDialog(SpriteFile* sprites, int sprite, const QString& templateDir, QWidget* parent)
    : QDialog(parent)
    , m_sprites(sprites)
    , m_selector(new QScrollBar(Qt::Horizontal, this))
    , m_selectorLabel(new QLabel(this))
    , m_period(new QSpinBox(this))
    , m_color(new QSpinBox(this))
    , m_invert(new QCheckBox(tr("Invert colors"), this))
    , m_templates(new QListWidget(this))
    , m_preview(new QLabel)
    , m_zoom(new QSlider(Qt::Horizontal, this))
    , m_timer(new QTimer(this))
{
    setObjectName(QStringLiteral("PalFXDlg"));
    setWindowTitle(tr("PalFX editor"));
    m_selector->setObjectName(QStringLiteral("sel"));
    const int count = sprites ? sprites->document().spriteCount() : 0;
    m_selector->setRange(0, std::max(0, count - 1));
    m_selector->setValue(std::clamp(sprite, 0, std::max(0, count - 1)));
    m_selectorLabel->setAlignment(Qt::AlignCenter);

    auto* grid = new QGridLayout;
    const char* const rows[] = {"add", "mul", "sin"};
    const QString captions[] = {tr("Add"), tr("Mul"), tr("Sin")};
    const char* const channels[] = {"R", "G", "B"};
    const char* const holders[] = {"widget_2", "widget_3", "widget_4"};
    for (int c = 0; c < 3; ++c) {
        auto* holder = new QWidget(this);
        holder->setObjectName(QLatin1String(holders[c]));
        auto* column = new QVBoxLayout(holder);
        column->setContentsMargins(0, 0, 0, 0);
        for (int r = 0; r < 3; ++r) {
            auto* spin = new QSpinBox(holder);
            spin->setObjectName(QLatin1String(rows[r]) + QLatin1String(channels[c]));
            spin->setRange(r == 1 ? -512 : -256, r == 1 ? 512 : 256);
            spin->setValue(r == 1 ? 256 : 0);
            column->addWidget(spin);
            (r == 0 ? m_add : r == 1 ? m_mul : m_sin)[c] = spin;
            connect(spin, &QSpinBox::valueChanged, this, &PalFXDialog::updatePreview);
        }
        grid->addWidget(holder, 1, c + 1, 3, 1);
    }
    for (int r = 0; r < 3; ++r)
        grid->addWidget(new QLabel(captions[r], this), r + 1, 0);
    m_period->setObjectName(QStringLiteral("period"));
    m_period->setRange(1, 65535);
    m_color->setObjectName(QStringLiteral("color"));
    m_color->setRange(0, 256);
    m_color->setValue(256);
    m_invert->setObjectName(QStringLiteral("invert"));
    auto* defaults = new QPushButton(tr("Default"), this);
    defaults->setAutoDefault(false);
    m_templates->setObjectName(QStringLiteral("templates"));
    for (const QFileInfo& info : QDir(templateDir).entryInfoList({QStringLiteral("*.xml")}, QDir::Files, QDir::Name)) {
        auto* item = new QListWidgetItem(info.completeBaseName(), m_templates);
        item->setData(Qt::UserRole, info.absoluteFilePath());
    }
    m_zoom->setObjectName(QStringLiteral("horizontalSlider"));
    m_zoom->setRange(0, 9);
    m_zoom->setValue(2);
    auto* insert = new QPushButton(tr("Insert in the editor"), this);
    insert->setAutoDefault(false);

    auto* valueColumn = new QVBoxLayout;
    valueColumn->addLayout(grid);
    auto* more = new QGridLayout;
    more->addWidget(new QLabel(tr("Period:"), this), 0, 0);
    more->addWidget(m_period, 0, 1);
    more->addWidget(new QLabel(tr("Color:"), this), 1, 0);
    more->addWidget(m_color, 1, 1);
    more->addWidget(m_invert, 2, 0, 1, 2);
    more->addWidget(defaults, 3, 0, 1, 2);
    valueColumn->addLayout(more);
    valueColumn->addWidget(new QLabel(tr("Templates:"), this));
    valueColumn->addWidget(m_templates, 1);
    auto* scroll = new QScrollArea(this);
    scroll->setWidget(m_preview);
    scroll->setWidgetResizable(true);
    m_preview->setAlignment(Qt::AlignCenter);
    auto* previewColumn = new QVBoxLayout;
    previewColumn->addWidget(scroll, 1);
    previewColumn->addWidget(m_zoom);
    auto* body = new QHBoxLayout;
    body->addLayout(valueColumn);
    body->addLayout(previewColumn, 1);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_selector);
    layout->addWidget(m_selectorLabel);
    layout->addLayout(body, 1);
    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(insert);
    layout->addLayout(buttons);
    resize(689, 485);

    connect(m_selector, &QScrollBar::valueChanged, this, &PalFXDialog::updatePreview);
    connect(m_zoom, &QSlider::valueChanged, this, &PalFXDialog::updatePreview);
    for (QSpinBox* spin : {m_period, m_color})
        connect(spin, &QSpinBox::valueChanged, this, &PalFXDialog::updatePreview);
    connect(m_invert, &QCheckBox::toggled, this, &PalFXDialog::updatePreview);
    connect(defaults, &QPushButton::clicked, this, [this] { setValues(PalFX{}); });
    connect(m_templates, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        PalFX palfx;
        if (readPalFXTemplate(item->data(Qt::UserRole).toString(), &palfx))
            setValues(palfx);
    });
    connect(insert, &QPushButton::clicked, this, [this] {
        const QString text = values().controllerText();
        accept();
        if (m_insert)
            m_insert(text);
    });
    // sinadd's sine moves at 60 ticks per second.
    m_timer->setInterval(1000 / 60);
    connect(m_timer, &QTimer::timeout, this, [this] {
        ++m_tick;
        if (values().sin[0] || values().sin[1] || values().sin[2])
            updatePreview();
    });
    m_timer->start();
    updatePreview();
}

PalFX PalFXDialog::values() const
{
    PalFX palfx;
    for (int c = 0; c < 3; ++c) {
        palfx.add[c] = m_add[c]->value();
        palfx.mul[c] = m_mul[c]->value();
        palfx.sin[c] = m_sin[c]->value();
    }
    palfx.period = m_period->value();
    palfx.color = m_color->value();
    palfx.invert = m_invert->isChecked();
    return palfx;
}

void PalFXDialog::setValues(const PalFX& palfx)
{
    for (int c = 0; c < 3; ++c) {
        const QSignalBlocker a(m_add[c]);
        const QSignalBlocker m(m_mul[c]);
        const QSignalBlocker s(m_sin[c]);
        m_add[c]->setValue(palfx.add[c]);
        m_mul[c]->setValue(palfx.mul[c]);
        m_sin[c]->setValue(palfx.sin[c]);
    }
    {
        const QSignalBlocker p(m_period);
        const QSignalBlocker c(m_color);
        const QSignalBlocker i(m_invert);
        m_period->setValue(palfx.period);
        m_color->setValue(palfx.color);
        m_invert->setChecked(palfx.invert);
    }
    updatePreview();
}

QImage PalFXDialog::preview() const
{
    const int count = m_sprites ? m_sprites->document().spriteCount() : 0;
    if (count == 0)
        return {};
    const Formats::Image rgba = m_sprites->document().imageRgba(m_selector->value());
    if (rgba.pixelBuffer().empty())
        return {};
    QImage image = QImage(rgba.pixels().data(), rgba.width(), rgba.height(), rgba.width() * 4, QImage::Format_RGBA8888)
                       .convertToFormat(QImage::Format_ARGB32);
    const PalFX palfx = values();
    for (int y = 0; y < image.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(line[x]) > 0)
                line[x] = palfx.apply(line[x], m_tick);
        }
    }
    return image;
}

void PalFXDialog::updatePreview()
{
    const int count = m_sprites ? m_sprites->document().spriteCount() : 0;
    m_selectorLabel->setText(count > 0 ? QStringLiteral("%1 / %2").arg(m_selector->value() + 1).arg(count) : QString());
    const QImage image = preview();
    const int zoom = m_zoom->value() + 1;
    m_preview->setPixmap(image.isNull() ? QPixmap()
                                        : QPixmap::fromImage(image.scaled(image.size() * zoom, Qt::IgnoreAspectRatio,
                                                                          Qt::FastTransformation)));
}

} // namespace OFS::Mugen
