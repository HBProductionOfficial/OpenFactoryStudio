#include "engines/mugen/dialogs/ExportHtmlDialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>

namespace OFS::Mugen {

namespace {

QLabel* label(const QString& text, const char* name, QWidget* parent)
{
    auto* l = new QLabel(text, parent);
    l->setObjectName(QLatin1String(name));
    return l;
}

QSpinBox* spin(const char* name, int minimum, int maximum, int value, QWidget* parent)
{
    auto* s = new QSpinBox(parent);
    s->setObjectName(QLatin1String(name));
    s->setRange(minimum, maximum);
    s->setValue(value);
    return s;
}

QGroupBox* sizeGroup(const QString& title, const char* name, const char* widthName, const char* heightName,
                     QSpinBox** width, QSpinBox** height, int maxWidth, int maxHeight, QWidget* parent)
{
    auto* group = new QGroupBox(title, parent);
    group->setObjectName(QLatin1String(name));
    *width = spin(widthName, 320, maxWidth, 320, group);
    *height = spin(heightName, 240, maxHeight, 240, group);
    auto* grid = new QGridLayout(group);
    grid->addWidget(label(QObject::tr("Width:"), "label_6", group), 0, 0);
    grid->addWidget(*width, 0, 1);
    grid->addWidget(label(QObject::tr("Height:"), "label_8", group), 1, 0);
    grid->addWidget(*height, 1, 1);
    return group;
}

} // namespace

ExportHtmlDialog::ExportHtmlDialog(const QList<QPair<QString, QString>>& layouts, bool stage,
                                   const QList<HtmlCommand>& commands, QWidget* parent)
    : QDialog(parent)
    , m_stage(stage)
    , m_layouts(layouts)
{
    setObjectName(QStringLiteral("ExpHtmlDlg"));
    setWindowTitle(tr("Export to HTML"));

    // Left column: layout, contact details and comments.
    m_layout = new QComboBox(this);
    m_layout->setObjectName(QStringLiteral("lay"));
    for (const auto& layout : layouts)
        m_layout->addItem(layout.first);
    m_mail = new QLineEdit(this);
    m_mail->setObjectName(QStringLiteral("mail"));
    m_site = new QLineEdit(this);
    m_site->setObjectName(QStringLiteral("site"));
    auto* comments = new QGroupBox(tr("Acknowledgements, comments:"), this);
    comments->setObjectName(QStringLiteral("comment_lb"));
    m_title = new QLineEdit(comments);
    m_title->setObjectName(QStringLiteral("title"));
    m_text = new QPlainTextEdit(comments);
    m_text->setObjectName(QStringLiteral("text"));
    auto* commentLayout = new QVBoxLayout(comments);
    commentLayout->addWidget(label(tr("Title:"), "title_lb", comments));
    commentLayout->addWidget(m_title);
    commentLayout->addWidget(label(tr("Text:"), "text_lb", comments));
    commentLayout->addWidget(m_text, 1);
    m_hline = new QCheckBox(tr("Horizontal line between commands"), this);
    m_hline->setObjectName(QStringLiteral("hline"));
    m_bold = new QCheckBox(tr("Command names in bold"), this);
    m_bold->setObjectName(QStringLiteral("bold"));
    auto* left = new QVBoxLayout;
    left->addWidget(label(tr("Layout:"), "layout_lb", this));
    left->addWidget(m_layout);
    left->addWidget(label(tr("E-Mail:"), "mail_lb", this));
    left->addWidget(m_mail);
    left->addWidget(label(tr("Home page:"), "site_lb", this));
    left->addWidget(m_site);
    left->addWidget(comments, 1);
    left->addWidget(m_hline);
    left->addWidget(m_bold);

    // Middle column: the commands, ticked unless they are for the AI.
    m_commands = new QStandardItemModel(0, 2, this);
    m_commands->setHorizontalHeaderLabels({tr("Name"), tr("Command")});
    for (const HtmlCommand& command : commands) {
        auto* name = new QStandardItem(command.name);
        name->setCheckable(true);
        name->setCheckState(command.aiOnly ? Qt::Unchecked : Qt::Checked);
        name->setEditable(false);
        auto* sequence = new QStandardItem(command.command);
        sequence->setEditable(false);
        m_commands->appendRow({name, sequence});
    }
    auto* table = new QTableView(this);
    table->setObjectName(QStringLiteral("cmds"));
    table->setModel(m_commands);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    auto* middle = new QVBoxLayout;
    QLabel* commandsLabel = label(tr("Commands:"), "cmd_lb", this);
    middle->addWidget(commandsLabel);
    middle->addWidget(table, 1);

    // Right column: how the previews are rendered.
    auto* rendering = new QGroupBox(tr("Rendering options:"), this);
    rendering->setObjectName(QStringLiteral("render_opt"));
    m_emulator = new QCheckBox(tr("Use Emulator to render"), rendering);
    m_emulator->setObjectName(QStringLiteral("emulator"));
    auto* explanation = label(tr("Renders the previews as the game shows them, with the options below. Stages are "
                                 "always rendered this way; characters will be once the Emulator can play them."),
                              "label", rendering);
    explanation->setWordWrap(true);
    auto* line = new QFrame(rendering);
    line->setObjectName(QStringLiteral("line"));
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    m_backgroundButton = new QPushButton(tr("Background color"), rendering);
    m_backgroundButton->setObjectName(QStringLiteral("bkgrnd"));
    m_backgroundSwatch = new QWidget(rendering);
    m_backgroundSwatch->setObjectName(QStringLiteral("bkgrnd_color"));
    m_backgroundSwatch->setFixedSize(32, 24);
    m_backgroundSwatch->setAutoFillBackground(true);
    m_frameStart = spin("frame_start", 0, 99999, 0, rendering);
    m_duration = spin("duration", 1, 99999, 5, rendering);
    m_fps = spin("fps", 15, 60, stage ? 30 : 60, rendering);
    m_sharedPalette = new QCheckBox(tr("One palette for all frames"), rendering);
    m_sharedPalette->setObjectName(QStringLiteral("shared_pal"));
    m_sharedPalette->setChecked(true);
    if (stage)
        m_staticSize = sizeGroup(tr("Static image resolution:"), "static_res", "static_w", "static_h", &m_staticWidth,
                                 &m_staticHeight, 1920, 1080, rendering);
    m_gifSize = sizeGroup(tr("GIF image resolution:"), "gif_res", "gif_w", "gif_h", &m_gifWidth, &m_gifHeight, 1920,
                          1080, rendering);
    auto* renderLayout = new QVBoxLayout(rendering);
    renderLayout->addWidget(m_emulator);
    renderLayout->addWidget(explanation);
    renderLayout->addWidget(line);
    auto* background = new QHBoxLayout;
    background->addWidget(m_backgroundButton, 1);
    background->addWidget(m_backgroundSwatch);
    renderLayout->addLayout(background);
    renderLayout->addWidget(label(tr("Start on frame number:"), "label_2", rendering));
    renderLayout->addWidget(m_frameStart);
    renderLayout->addWidget(label(tr("Duration (sec):"), "label_3", rendering));
    renderLayout->addWidget(m_duration);
    renderLayout->addWidget(label(tr("Frame rate:"), "label_4", rendering));
    renderLayout->addWidget(m_fps);
    renderLayout->addWidget(m_sharedPalette);
    if (m_staticSize)
        renderLayout->addWidget(m_staticSize);
    renderLayout->addWidget(m_gifSize);
    renderLayout->addStretch(1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setObjectName(QStringLiteral("buttonBox"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* columns = new QHBoxLayout;
    columns->addLayout(left, stage ? 3 : 2);
    columns->addLayout(middle, 1);
    columns->addWidget(rendering);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(columns, 1);
    layout->addWidget(buttons);

    if (stage) {
        commandsLabel->hide();
        table->hide();
        m_hline->hide();
        m_bold->hide();
        m_emulator->setChecked(true);
        m_emulator->setEnabled(false);
    } else {
        // There is no Emulator to render characters with yet.
        m_emulator->setEnabled(false);
    }
    connect(m_emulator, &QCheckBox::toggled, this, &ExportHtmlDialog::updateRendering);
    connect(m_backgroundButton, &QPushButton::clicked, this, [this] {
        const QColor color = QColorDialog::getColor(m_background, this, tr("Background color"));
        if (color.isValid())
            setBackground(color);
    });
    setBackground(Qt::black);
    updateRendering();
    resize(676, stage ? 591 : 575);
}

void ExportHtmlDialog::setBackground(const QColor& color)
{
    m_background = color;
    QPalette palette = m_backgroundSwatch->palette();
    palette.setColor(QPalette::Window, color);
    m_backgroundSwatch->setPalette(palette);
}

void ExportHtmlDialog::updateRendering()
{
    const bool on = m_emulator->isChecked();
    for (QWidget* w : std::initializer_list<QWidget*>{m_backgroundButton, m_frameStart, m_duration, m_fps,
                                                      m_sharedPalette, m_gifSize, m_staticSize}) {
        if (w)
            w->setEnabled(on);
    }
}

HtmlExportOptions ExportHtmlDialog::options() const
{
    HtmlExportOptions options;
    options.layout = m_layouts.value(m_layout->currentIndex()).second;
    options.email = m_mail->text();
    options.homepage = m_site->text();
    options.title = m_title->text();
    options.text = m_text->toPlainText();
    options.horizontalLines = m_hline->isChecked();
    options.boldNames = m_bold->isChecked();
    for (int row = 0; row < m_commands->rowCount(); ++row) {
        if (m_commands->item(row, 0)->checkState() == Qt::Checked)
            options.commands << m_commands->item(row, 0)->text();
    }
    options.frameStart = m_frameStart->value();
    options.duration = m_duration->value();
    options.frameRate = m_fps->value();
    options.sharedPalette = m_sharedPalette->isChecked();
    options.gifSize = QSize(m_gifWidth->value(), m_gifHeight->value());
    if (m_staticSize)
        options.staticSize = QSize(m_staticWidth->value(), m_staticHeight->value());
    options.background = m_background;
    return options;
}

} // namespace OFS::Mugen
