#include "shell/dialogs/FirstRunDialog.h"

#include "core/Version.h"
#include "core/app/AppContext.h"
#include "core/settings/Config.h"
#include "core/settings/SettingsImporter.h"
#include "core/theme/ColorSet.h"
#include "core/theme/ThemeManager.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace OFS {

namespace {

// Small sketch of the main window in the colors of a theme, used as the button picture.
QPixmap previewFor(const FirstRunDialog::Theme& theme, const ThemeManager& themes, qreal dpr)
{
    ColorSet colors;
    const QString path = themes.colorSetPath(theme.colorset);
    if (!path.isEmpty())
        colors.load(path);
    const QPalette style = themes.paletteOf(theme.stylesheet);
    const QColor window = style.color(QPalette::Window);
    const bool dark = window.lightness() < 128;
    const QColor panel = style.color(QPalette::Base);
    const QColor code = colors.color(QStringLiteral("background"));
    const QColor accent = colors.color(QStringLiteral("block"));
    const QColor text = colors.color(QStringLiteral("foreground"));

    const QSize size = FirstRunDialog::previewSize();
    QPixmap pixmap(size * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(window);
    QPainter p(&pixmap);
    p.fillRect(QRect(0, 0, size.width(), 14), window.darker(dark ? 130 : 110));
    for (int i = 0; i < 6; ++i)
        p.fillRect(QRect(6 + i * 14, 3, 10, 8), theme.iconset.startsWith(QLatin1String("Flat")) ? text : accent);
    p.fillRect(QRect(4, 18, 40, size.height() - 22), panel);
    p.fillRect(QRect(48, 18, size.width() - 52, size.height() - 22), code);
    for (int i = 0; i < 7; ++i) {
        const int y = 24 + i * 10;
        p.fillRect(QRect(54, y, i % 3 == 0 ? 30 : 60 + (i * 7) % 30, 4), i % 3 == 0 ? accent : text);
    }
    return pixmap;
}

} // namespace

QList<FirstRunDialog::Theme> FirstRunDialog::themes()
{
    return {
        {tr("Neo Dark Flat"), QStringLiteral("Neo"), QStringLiteral("Flat-Dark"), QStringLiteral("Dark"),
         QStringLiteral("Dark")},
        {tr("Neo Light Flat"), QStringLiteral("Neo"), QStringLiteral("Flat-Light"), QStringLiteral("Light"),
         QStringLiteral("Light")},
        {tr("Neo Dark"), QStringLiteral("Neo"), QStringLiteral("Nuvola"), QStringLiteral("Dark"),
         QStringLiteral("Dark")},
        {tr("Neo Light"), QStringLiteral("Neo"), QStringLiteral("Nuvola"), QStringLiteral("Light"),
         QStringLiteral("Light")},
        {tr("Ultimate"), QStringLiteral("Ultimate"), QStringLiteral("Ultimate"), QStringLiteral("Ultimate"),
         QStringLiteral("Ultimate")},
        {tr("Classic"), QStringLiteral("Classic"), QStringLiteral("Classic"), QStringLiteral("Classic"),
         QStringLiteral("Classic")},
    };
}

FirstRunDialog::FirstRunDialog(AppContext& context, QWidget* parent)
    : QDialog(parent)
    , m_context(context)
    , m_group(new QButtonGroup(this))
{
    setObjectName(QStringLiteral("firstRunDialog"));
    setWindowTitle(tr("Welcome to %1").arg(QStringLiteral(OFS_DISPLAY_NAME)));

    auto* intro = new QLabel(tr("Choose how the program looks. You can change the interface preset, icons, style "
                                "and colors at any time in Tools > Options."),
                             this);
    intro->setWordWrap(true);

    auto* grid = new QGridLayout;
    grid->setSpacing(10);
    const QList<Theme> list = themes();
    for (int i = 0; i < list.size(); ++i) {
        auto* button = new QToolButton(this);
        button->setObjectName(QStringLiteral("theme%1").arg(i));
        button->setCheckable(true);
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIconSize(FirstRunDialog::previewSize());
        button->setIcon(QIcon(previewFor(list[i], context.theme(), devicePixelRatioF())));
        button->setText(list[i].name);
        button->setAutoRaise(true);
        m_group->addButton(button, i);
        grid->addWidget(button, i / 3, i % 3);
    }
    m_group->setExclusive(true);
    selectTheme(0);

    auto* buttons = new QDialogButtonBox(this);
    QPushButton* start = buttons->addButton(tr("Start"), QDialogButtonBox::AcceptRole);
    start->setDefault(true);
    QPushButton* import = buttons->addButton(tr("Import from previous installation..."), QDialogButtonBox::ActionRole);
    import->setObjectName(QStringLiteral("importButton"));
    import->setEnabled(SettingsImporter().hasSettings());
    connect(start, &QPushButton::clicked, this, &QDialog::accept);
    connect(import, &QPushButton::clicked, this, &FirstRunDialog::importPrevious);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addLayout(grid);
    layout->addWidget(buttons);
}

int FirstRunDialog::selectedTheme() const
{
    return qMax(0, m_group->checkedId());
}

void FirstRunDialog::selectTheme(int index)
{
    if (QAbstractButton* b = m_group->button(index))
        b->setChecked(true);
}

void FirstRunDialog::applySelection()
{
    Config& config = m_context.config();
    if (!m_imported) {
        const Theme theme = themes().value(selectedTheme());
        config.setValue(QStringLiteral("preset"), theme.preset);
        config.setValue(QStringLiteral("iconset"), theme.iconset);
        config.setValue(QStringLiteral("stylesheet"), theme.stylesheet);
        config.setValue(QStringLiteral("colorset"), theme.colorset);
    }
    config.setValue(QStringLiteral("initialized"), true);
    config.setValue(QStringLiteral("config_wizard"), false);
    m_context.saveSettings();
}

bool FirstRunDialog::isNeeded(const AppContext& context)
{
    const Config& config = context.config();
    if (QFileInfo::exists(config.userPath()) && config.boolValue(QStringLiteral("initialized"), false))
        return false;
    return !QFileInfo::exists(config.userPath()) || config.boolValue(QStringLiteral("config_wizard"), true);
}

void FirstRunDialog::importPrevious()
{
    SettingsImporter importer;
    const SettingsImporter::Report report = importer.import(m_context);
    if (!report.ok()) {
        QMessageBox::warning(this, windowTitle(), report.errors.join(QLatin1Char('\n')));
        return;
    }
    m_imported = true;
    QMessageBox::information(
        this, windowTitle(),
        tr("%n setting(s) were imported from the previous installation.", nullptr, report.total()));
    accept();
}

} // namespace OFS
