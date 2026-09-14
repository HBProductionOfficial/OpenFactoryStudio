#include "shell/dialogs/OptionsDialog.h"

#include "shell/FileDialogs.h"

#include "core/actions/ActionRegistry.h"
#include "core/app/AppContext.h"
#include "core/app/Languages.h"
#include "core/app/Paths.h"
#include "core/engine/Engine.h"
#include "core/engine/EngineManager.h"
#include "core/options/OptionsPage.h"
#include "core/settings/Config.h"
#include "core/settings/History.h"
#include "core/settings/SettingsBundle.h"
#include "core/settings/ShortcutSettings.h"
#include "core/text/Encodings.h"
#include "core/theme/ColorSet.h"
#include "core/theme/ThemeManager.h"
#include "shell/dialogs/ColorsPage.h"
#include "shell/dialogs/MugenOptionsPage.h"
#include "shell/dialogs/ShortcutEditor.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <tuple>

namespace OFS {

// Value encodings in config.xml, the same as Fighter Factory uses:
//   check boxes             "true" / "false" (an empty value counts as false)
//   numeric combo boxes     the item index (zoom_mode, zoom_center, onion_effect, texUploadMethod,
//                           default_codepage); cache_type is the index plus one (2 is "Hard disk")
//   radio groups            crosshair_type 0 Default, 1 Modern, 2 Custom;
//                           background_type 0 None, 1 Default, 2 Custom (0 and 2 are a guess)
//   sliders 0..100          stored as 0..1 (onion_opacity, box_opacity)
//   Ask/Always/Never        showmsg_* keys: "0" Ask, "1" Always, "2" Never (an empty value also
//                           reads as Ask, anything else as Never)
//   Can't save ... boxes    showmsg_sprs_empty, showmsg_spr_no_pal, showmsg_snds_empty: "true" / "false"
//   Renderer                opengl_mode -1 Automatic, 0 OpenGL (desktop), 1 DirectX (ANGLE);
//                           Native painter sets native_painting = true and keeps opengl_mode
struct OptionsDialog::Binding
{
    QStringList keys;
    QStringList fallbacks;
    std::function<void(const QStringList&)> read;
    std::function<QStringList()> write;
};

namespace {

const QString kCustomColorSet = QStringLiteral("Custom");

const QStringList kAnswerValues = {QStringLiteral("0"), QStringLiteral("1"), QStringLiteral("2")};

void registerBuiltinPages()
{
    if (!OptionsPageRegistry::hasPage(QStringLiteral("mugen"))) {
        OptionsPageRegistry::registerPage(QStringLiteral("mugen"), [](Config& config, QWidget* parent) {
            return new MugenOptionsPage(config, parent);
        });
    }
}

QFrame* separator(QWidget* parent)
{
    auto* line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}

QString percentText(double value)
{
    return QString::number(value, 'g', 6);
}

bool validColorSetName(const QString& name)
{
    static const QString forbidden = QStringLiteral("/\\:*?\"<>|");
    if (name.isEmpty() || name.startsWith(QLatin1Char('.')))
        return false;
    for (const QChar c : name) {
        if (forbidden.contains(c) || c.unicode() < 32)
            return false;
    }
    return true;
}

} // namespace

OptionsDialog::OptionsDialog(AppContext& context, QWidget* parent)
    : QDialog(parent)
    , m_context(context)
{
    registerBuiltinPages();
    setObjectName(QStringLiteral("Options"));
    setWindowTitle(tr("Options"));

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("tabWidget"));
    m_tabs->addTab(createGeneralTab(), tr("General"));
    m_tabs->addTab(createInterfaceTab(), tr("Interface"));
    m_tabs->addTab(createMessagesTab(), tr("Messages"));
    m_tabs->addTab(createColorsTab(), tr("Colors"));
    m_tabs->addTab(createGraphicsTab(), tr("Graphics"));
    m_tabs->addTab(createCodeTab(), tr("Code"));

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_tabs, 1);
    layout->addLayout(createButtonRow());

    createEnginePages();
    refreshThemeLists();
    load();
    resize(1020, 614);
}

OptionsDialog::~OptionsDialog() = default;

int OptionsDialog::showOptions(QWidget* parent, const std::function<void()>& onApplied, int tab)
{
    AppContext* context = AppContext::instance();
    if (!context)
        return QDialog::Rejected;
    OptionsDialog dialog(*context, parent);
    if (onApplied)
        connect(&dialog, &OptionsDialog::settingsApplied, &dialog, onApplied);
    if (tab >= 0 && tab < dialog.tabs()->count())
        dialog.tabs()->setCurrentIndex(tab);
    return dialog.exec();
}

QString OptionsDialog::customColorSetName()
{
    return kCustomColorSet;
}

void OptionsDialog::setConflictHandler(ConflictHandler handler)
{
    m_shortcuts->setConflictHandler(std::move(handler));
}

// ---------------------------------------------------------------------------------------------
// Tabs

QWidget* OptionsDialog::createGeneralTab()
{
    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("mainTab"));
    auto* grid = new QGridLayout(page);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);

    auto check = [this, page, grid](const QString& text, const QString& key, bool fallback, int row, int column) {
        auto* box = new QCheckBox(text, page);
        box->setObjectName(key);
        bindCheck(box, key, fallback);
        grid->addWidget(box, row, column);
    };
    check(tr("Enable closing last open file (closes editor)"), QStringLiteral("can_close_editor"), true, 0, 0);
    check(tr("Enable the file modification watcher"), QStringLiteral("enable_file_watcher"), true, 0, 1);
    check(tr("Use system native open dialog"), QStringLiteral("use_native_open"), true, 1, 0);
    check(tr("Sort file names from the open dialog"), QStringLiteral("sort_file_names"), true, 1, 1);
    check(tr("Make a backup before loading a project"), QStringLiteral("backup_onload"), false, 2, 0);
    check(tr("Automatically check updates on startup"), QStringLiteral("auto_update"), false, 2, 1);

    grid->addWidget(new QLabel(tr("Cache type:"), page), 3, 0);
    grid->addWidget(new QLabel(tr("Undo limit:"), page), 3, 1);
    auto* cache = new QComboBox(page);
    cache->setObjectName(QStringLiteral("cache_type"));
    cache->addItems({tr("Memory (maximum speed)"), tr("Hard disk (low memory usage)")});
    bindIndex(cache, QStringLiteral("cache_type"), 1, 1);
    grid->addWidget(cache, 4, 0);
    auto* undo = new QSpinBox(page);
    undo->setObjectName(QStringLiteral("undo_limit"));
    undo->setRange(0, 256);
    bindSpin(undo, QStringLiteral("undo_limit"), 64);
    grid->addWidget(undo, 4, 1);

    grid->addWidget(new QLabel(tr("Max history:"), page), 5, 0);
    grid->addWidget(new QLabel(tr("Thumbnail size:"), page), 5, 1);
    auto* history = new QSpinBox(page);
    history->setObjectName(QStringLiteral("maxHistory"));
    history->setRange(10, 32);
    bindSpin(history, QStringLiteral("max_history"), 32);
    grid->addWidget(history, 6, 0);
    auto* thumb = new QSpinBox(page);
    thumb->setObjectName(QStringLiteral("thumb_size"));
    thumb->setRange(32, 256);
    bindSpin(thumb, QStringLiteral("thumb_size"), 64);
    grid->addWidget(thumb, 6, 1);

    auto* autoSave = new QGroupBox(tr("Auto save:"), page);
    autoSave->setObjectName(QStringLiteral("autoSave"));
    auto* autoGrid = new QGridLayout(autoSave);
    autoGrid->setColumnStretch(0, 1);
    autoGrid->setColumnStretch(1, 1);
    auto* intervalLabel = new QLabel(tr("Interval:"), autoSave);
    intervalLabel->setObjectName(QStringLiteral("saveIntervalLb"));
    autoGrid->addWidget(intervalLabel, 0, 0, 1, 2);
    auto* interval = new QSpinBox(autoSave);
    interval->setObjectName(QStringLiteral("saveInterval"));
    interval->setRange(0, 99);
    bindSpin(interval, QStringLiteral("autosave_interval"), 0);
    autoGrid->addWidget(interval, 1, 0);
    auto* minutes = new QLabel(tr("minutes. 0 to disable."), autoSave);
    minutes->setObjectName(QStringLiteral("saveMinLb"));
    autoGrid->addWidget(minutes, 1, 1);
    auto* asyncOnly = new QCheckBox(tr("Only save if asynchronous mode is supported"), autoSave);
    asyncOnly->setObjectName(QStringLiteral("saveAsyncOnly"));
    bindCheck(asyncOnly, QStringLiteral("autosave_async"), true);
    autoGrid->addWidget(asyncOnly, 2, 0, 1, 2);
    grid->addWidget(autoSave, 7, 0);

    auto* language = new QGroupBox(tr("Language:"), page);
    language->setObjectName(QStringLiteral("language_group"));
    auto* languageLayout = new QVBoxLayout(language);
    m_language = new QComboBox(language);
    m_language->setObjectName(QStringLiteral("language"));
    for (const LanguageInfo& info : LanguageInfo::available({Paths::userDir(), Paths::dataDir()}))
        m_language->addItem(info.name, info.code);
    m_languageAuthor = new QLabel(language);
    m_languageAuthor->setObjectName(QStringLiteral("langAuthor"));
    m_languageContact = new QLabel(language);
    m_languageContact->setObjectName(QStringLiteral("langContact"));
    m_languageContact->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_languageContact->setOpenExternalLinks(true);
    languageLayout->addWidget(m_language);
    languageLayout->addWidget(m_languageAuthor);
    languageLayout->addWidget(m_languageContact);
    bind(
        QStringLiteral("language"), QStringLiteral("en-us"),
        [this](const QString& code) {
            int index = m_language->findData(code);
            if (index < 0) {
                m_language->addItem(code, code);
                index = m_language->count() - 1;
            }
            m_language->setCurrentIndex(index);
            refreshLanguageInfo();
        },
        [this] { return m_language->currentData().toString(); });
    connect(m_language, &QComboBox::currentIndexChanged, this, &OptionsDialog::refreshLanguageInfo);
    grid->addWidget(language, 7, 1);

    grid->setRowStretch(8, 1);
    return page;
}

QWidget* OptionsDialog::createInterfaceTab()
{
    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("intfTab"));

    auto* left = new QVBoxLayout;
    auto addCombo = [page, left](const QString& caption, const QString& key) {
        left->addWidget(new QLabel(caption, page));
        auto* combo = new QComboBox(page);
        combo->setObjectName(key);
        left->addWidget(combo);
        return combo;
    };
    m_preset = addCombo(tr("Preset:"), QStringLiteral("preset"));
    m_iconset = addCombo(tr("Iconset:"), QStringLiteral("iconset"));
    m_stylesheet = addCombo(tr("Stylesheet:"), QStringLiteral("stylesheet"));
    m_colorset = addCombo(tr("Colorset:"), QStringLiteral("colorset"));
    bindName(m_preset, QStringLiteral("preset"), QStringLiteral("Neo"));
    bindName(m_iconset, QStringLiteral("iconset"), QStringLiteral("Nuvola"));
    bindName(m_stylesheet, QStringLiteral("stylesheet"), QStringLiteral("Dark"));
    bindName(m_colorset, QStringLiteral("colorset"), QStringLiteral("Dark"));
    connect(m_colorset, &QComboBox::currentTextChanged, this, [this](const QString& name) {
        if (!m_syncingColors)
            showColorSet(name);
    });

    auto* dpi = new QGroupBox(tr("DPI Scaling:"), page);
    dpi->setObjectName(QStringLiteral("dpiBox"));
    auto* dpiLayout = new QVBoxLayout(dpi);
    auto* dpiAuto = new QCheckBox(tr("Automatic"), dpi);
    dpiAuto->setObjectName(QStringLiteral("dpi_auto"));
    m_dpiAuto = dpiAuto;
    bindCheck(dpiAuto, QStringLiteral("dpi_auto"), false);
    m_dpiScale = new QDoubleSpinBox(dpi);
    m_dpiScale->setObjectName(QStringLiteral("dpi_scale"));
    m_dpiScale->setRange(0.2, 10.0);
    m_dpiScale->setDecimals(2);
    m_dpiScale->setSingleStep(0.05);
    bindDoubleSpin(m_dpiScale, QStringLiteral("dpi_scale"), 1.0);
    dpiLayout->addWidget(dpiAuto);
    dpiLayout->addWidget(new QLabel(tr("Scale factor:"), dpi));
    dpiLayout->addWidget(m_dpiScale);
    connect(dpiAuto, &QCheckBox::toggled, this, &OptionsDialog::updateDependentControls);
    left->addWidget(dpi);
    left->addStretch(1);

    auto* leftWidget = new QWidget(page);
    leftWidget->setLayout(left);
    left->setContentsMargins(0, 0, 0, 0);
    leftWidget->setFixedWidth(150);

    auto* shortcuts = new QGroupBox(tr("Keyboard shortcuts:"), page);
    shortcuts->setObjectName(QStringLiteral("shortcut_lb"));
    auto* shortcutsLayout = new QVBoxLayout(shortcuts);
    m_shortcuts = new ShortcutEditor(m_context.actions(), shortcuts);
    shortcutsLayout->addWidget(m_shortcuts);

    auto* layout = new QHBoxLayout(page);
    layout->addWidget(leftWidget);
    layout->addWidget(shortcuts, 1);
    return page;
}

QComboBox* OptionsDialog::answerCombo(const QString& key)
{
    auto* combo = new QComboBox;
    combo->setObjectName(key);
    combo->addItems({tr("Ask"), tr("Always"), tr("Never")});
    bindAnswer(combo, key);
    return combo;
}

QWidget* OptionsDialog::createMessagesTab()
{
    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("tab"));
    auto* grid = new QGridLayout(page);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);

    auto check = [this, page](const QString& text, const QString& key, bool flag) {
        auto* box = new QCheckBox(text, page);
        box->setObjectName(key);
        if (flag)
            bindFlag(box, key);
        else
            bindCheck(box, key, true);
        return box;
    };
    auto label = [page](const QString& text) {
        return new QLabel(text, page);
    };

    grid->addWidget(check(tr("Request confirmation on exit"), QStringLiteral("confirm_close"), false), 0, 0);
    grid->addWidget(check(tr("Request confirmation on delete"), QStringLiteral("confirm_delete"), false), 0, 1);
    grid->addWidget(label(tr("Discard file modifications:")), 1, 0);
    grid->addWidget(label(tr("Reload watched files on modification:")), 1, 1);
    grid->addWidget(answerCombo(QStringLiteral("showmsg_discard")), 2, 0);
    grid->addWidget(answerCombo(QStringLiteral("showmsg_reload_file")), 2, 1);
    grid->addWidget(separator(page), 3, 0, 1, 2);
    grid->addWidget(label(tr("Sprite Sharing:")), 4, 0);
    grid->addWidget(label(tr("Palette sharing:")), 4, 1);
    grid->addWidget(answerCombo(QStringLiteral("showmsg_share_spr")), 5, 0);
    grid->addWidget(answerCombo(QStringLiteral("showmsg_share_pal")), 5, 1);
    grid->addWidget(label(tr("Crop new sprites:")), 6, 0);
    grid->addWidget(label(tr("Add palette from file:")), 6, 1);
    grid->addWidget(answerCombo(QStringLiteral("showmsg_crop_spr")), 7, 0);
    grid->addWidget(answerCombo(QStringLiteral("showmsg_pal_file")), 7, 1);
    grid->addWidget(check(tr("Can't save empty sprite list"), QStringLiteral("showmsg_sprs_empty"), true), 8, 0);
    grid->addWidget(check(tr("Can't save 8-bit sprites without palette"), QStringLiteral("showmsg_spr_no_pal"), true),
                    8, 1);
    grid->addWidget(separator(page), 9, 0, 1, 2);
    grid->addWidget(check(tr("Can't save empty sound list"), QStringLiteral("showmsg_snds_empty"), true), 10, 0);
    grid->addWidget(separator(page), 11, 0, 1, 2);
    grid->addWidget(label(tr("New identifier for animation duplication:")), 12, 0);
    grid->addWidget(label(tr("Contact box replication:")), 12, 1);
    grid->addWidget(answerCombo(QStringLiteral("showmsg_anim_clone")), 13, 0);
    grid->addWidget(answerCombo(QStringLiteral("showmsg_cbox_replicate")), 13, 1);
    grid->addWidget(separator(page), 14, 0, 1, 2);
    grid->addWidget(label(tr("Reload document after encoding changes:")), 15, 0);
    grid->addWidget(answerCombo(QStringLiteral("showmsg_enc_reload")), 16, 0);
    grid->setRowStretch(17, 1);
    return page;
}

QWidget* OptionsDialog::createColorsTab()
{
    m_colors = new ColorsPage;
    m_colors->setObjectName(QStringLiteral("colorsTab"));
    connect(m_colors, &ColorsPage::edited, this, &OptionsDialog::colorsEdited);
    connect(m_colors, &ColorsPage::saveRequested, this, &OptionsDialog::colorSaveRequested);
    return m_colors;
}

QPushButton* OptionsDialog::browseButton(QLineEdit* target, const QString& title)
{
    auto* button = new QPushButton(tr("..."), target->parentWidget());
    button->setAutoDefault(false);
    button->setFixedWidth(40);
    connect(button, &QPushButton::clicked, this, [this, target, title] {
        const bool native =
            Config::decodeBool(currentValue(QStringLiteral("use_native_open"), QStringLiteral("true")), true);
        const QString file =
            FileDialogs::getOpenFileName(this, title, QFileInfo(target->text()).absolutePath(),
                                         tr("Images (*.png *.bmp *.gif *.jpg *.jpeg);;All files (*)"),
                                         native ? QFileDialog::Options() : QFileDialog::DontUseNativeDialog);
        if (!file.isEmpty())
            target->setText(QDir::fromNativeSeparators(file));
    });
    return button;
}

QWidget* OptionsDialog::createGraphicsTab()
{
    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("gfxTab"));

    auto* left = new QVBoxLayout;
    auto* palFit = new QCheckBox(tr("Colors fit available space on Advanced Palette Editor"), page);
    palFit->setObjectName(QStringLiteral("pal_fit_space"));
    bindCheck(palFit, QStringLiteral("pal_fit_space"), false);
    left->addWidget(palFit);

    auto* renderer = new QGroupBox(tr("Renderer:"), page);
    renderer->setObjectName(QStringLiteral("glMode"));
    auto* rendererLayout = new QVBoxLayout(renderer);
    m_renderer = new QComboBox(renderer);
    m_renderer->setObjectName(QStringLiteral("opengl_mode"));
    m_renderer->addItems({tr("Automatic"), tr("OpenGL (Desktop)"), tr("DirectX (ANGLE)"), tr("Native painter")});
    rendererLayout->addWidget(m_renderer);
    rendererLayout->addWidget(new QLabel(tr("This setting requires a program restart."), renderer));
    bind(
        {QStringLiteral("opengl_mode"), QStringLiteral("native_painting")},
        {QStringLiteral("-1"), QStringLiteral("false")},
        [this](const QStringList& values) {
            m_loadedOpenGlMode = values[0];
            const int mode = values[0].trimmed().toInt();
            int index = mode == 0 ? 1 : mode == 1 ? 2 : 0;
            if (Config::decodeBool(values[1], false))
                index = 3;
            m_renderer->setCurrentIndex(index);
        },
        [this]() -> QStringList {
            switch (m_renderer->currentIndex()) {
            case 1:
                return {QStringLiteral("0"), QStringLiteral("false")};
            case 2:
                return {QStringLiteral("1"), QStringLiteral("false")};
            case 3:
                return {m_loadedOpenGlMode.isEmpty() ? QStringLiteral("-1") : m_loadedOpenGlMode,
                        QStringLiteral("true")};
            default:
                return {QStringLiteral("-1"), QStringLiteral("false")};
            }
        });
    left->addWidget(renderer);

    left->addWidget(new QLabel(tr("Texture upload method:"), page));
    auto* texture = new QComboBox(page);
    texture->setObjectName(QStringLiteral("texUploadMethod"));
    texture->addItems(
        {tr("Immediate (may slow down loading)"), tr("Delayed (on-demand)"), tr("Multi-threaded (in the background)")});
    bindIndex(texture, QStringLiteral("texUploadMethod"), 1);
    left->addWidget(texture);

    auto* cross = new QGroupBox(tr("Crosshair:"), page);
    cross->setObjectName(QStringLiteral("crossGrp"));
    auto* crossGrid = new QGridLayout(cross);
    crossGrid->setColumnStretch(0, 1);
    crossGrid->setColumnStretch(1, 1);
    m_crosshair = new QButtonGroup(this);
    auto* crossDefault = new QRadioButton(tr("Default"), cross);
    auto* crossModern = new QRadioButton(tr("Modern"), cross);
    auto* crossCustom = new QRadioButton(tr("Custom"), cross);
    crossDefault->setObjectName(QStringLiteral("crosshair_default"));
    crossModern->setObjectName(QStringLiteral("crosshair_modern"));
    crossCustom->setObjectName(QStringLiteral("crosshair_custom"));
    m_crosshair->addButton(crossDefault, 0);
    m_crosshair->addButton(crossModern, 1);
    m_crosshair->addButton(crossCustom, 2);
    bindRadio(m_crosshair, QStringLiteral("crosshair_type"), 0);
    crossGrid->addWidget(crossDefault, 0, 0);
    crossGrid->addWidget(crossModern, 0, 1);
    crossGrid->addWidget(crossCustom, 1, 0);
    int row = 2;
    const QList<QPair<QString, QString>> crossFiles = {{tr("Sprite:"), QStringLiteral("crosshair_main")},
                                                       {tr("Animation:"), QStringLiteral("crosshair_secondary")},
                                                       {tr("Extra:"), QStringLiteral("crosshair_extra")}};
    for (const auto& [caption, key] : crossFiles) {
        crossGrid->addWidget(new QLabel(caption, cross), row++, 0, 1, 3);
        auto* edit = new QLineEdit(cross);
        edit->setObjectName(key);
        bindLine(edit, key);
        QPushButton* browse = browseButton(edit, caption);
        crossGrid->addWidget(edit, row, 0, 1, 2);
        crossGrid->addWidget(browse, row++, 2);
        m_crosshairFiles << edit << browse;
    }
    connect(m_crosshair, &QButtonGroup::idToggled, this, &OptionsDialog::updateDependentControls);
    left->addWidget(cross);
    left->addStretch(1);

    auto* right = new QVBoxLayout;
    auto* onion = new QGridLayout;
    onion->setColumnStretch(0, 1);
    onion->setColumnStretch(1, 1);
    onion->addWidget(new QLabel(tr("Onion Skin effect:"), page), 0, 0);
    onion->addWidget(new QLabel(tr("Opacity:"), page), 0, 1);
    auto* effect = new QComboBox(page);
    effect->setObjectName(QStringLiteral("onion_effect"));
    effect->addItems({tr("None"), tr("Monochrome"), tr("Lighter"), tr("Darker")});
    bindIndex(effect, QStringLiteral("onion_effect"), 2);
    onion->addWidget(effect, 1, 0);
    auto* opacity = new QSlider(Qt::Horizontal, page);
    opacity->setObjectName(QStringLiteral("onion_opacity"));
    bindPercent(opacity, QStringLiteral("onion_opacity"), 0.74);
    onion->addWidget(opacity, 1, 1);
    right->addLayout(onion);

    auto* back = new QGroupBox(tr("Background:"), page);
    back->setObjectName(QStringLiteral("backGrp"));
    auto* backGrid = new QGridLayout(back);
    m_background = new QButtonGroup(this);
    auto* backDefault = new QRadioButton(tr("Default"), back);
    auto* backNone = new QRadioButton(tr("None"), back);
    auto* backCustom = new QRadioButton(tr("Custom"), back);
    backDefault->setObjectName(QStringLiteral("background_default"));
    backNone->setObjectName(QStringLiteral("background_none"));
    backCustom->setObjectName(QStringLiteral("background_custom"));
    m_background->addButton(backNone, 0);
    m_background->addButton(backDefault, 1);
    m_background->addButton(backCustom, 2);
    bindRadio(m_background, QStringLiteral("background_type"), 1);
    backGrid->addWidget(backDefault, 0, 0, 1, 2);
    backGrid->addWidget(backNone, 0, 2, 1, 3);
    backGrid->addWidget(backCustom, 1, 0, 1, 2);
    auto* backFile = new QLineEdit(back);
    backFile->setObjectName(QStringLiteral("background_file"));
    bindLine(backFile, QStringLiteral("background_file"));
    QPushButton* backBrowse = browseButton(backFile, tr("Background:"));
    backGrid->addWidget(backFile, 2, 0, 1, 4);
    backGrid->addWidget(backBrowse, 2, 4);
    m_backgroundFiles << backFile << backBrowse;
    auto* backX = new QSpinBox(back);
    auto* backY = new QSpinBox(back);
    backX->setObjectName(QStringLiteral("background_x"));
    backY->setObjectName(QStringLiteral("background_y"));
    for (QSpinBox* spin : {backX, backY})
        spin->setRange(-2000, 2000);
    bindSpin(backX, QStringLiteral("background_x"), -426);
    bindSpin(backY, QStringLiteral("background_y"), -416);
    backGrid->addWidget(new QLabel(tr("X:"), back), 3, 0);
    backGrid->addWidget(backX, 3, 1);
    backGrid->addWidget(new QLabel(tr("Y:"), back), 3, 2);
    backGrid->addWidget(backY, 3, 3, 1, 2);
    backGrid->setColumnStretch(1, 1);
    backGrid->setColumnStretch(3, 1);
    backGrid->addWidget(new QLabel(tr("The position above is based on the sprite axis."), back), 4, 0, 1, 5);
    connect(m_background, &QButtonGroup::idToggled, this, &OptionsDialog::updateDependentControls);
    right->addWidget(back);

    auto* zoom = new QGridLayout;
    zoom->setColumnMinimumWidth(0, 145);
    zoom->setColumnStretch(1, 1);
    zoom->addWidget(new QLabel(tr("Zoom mode:"), page), 0, 0);
    auto* zoomMode = new QComboBox(page);
    zoomMode->setObjectName(QStringLiteral("zoom_mode"));
    zoomMode->addItems({tr("Small range (up to 4X)"), tr("Large range (up to 8X)")});
    bindIndex(zoomMode, QStringLiteral("zoom_mode"), 1);
    zoom->addWidget(zoomMode, 0, 1);
    zoom->addWidget(new QLabel(tr("Center of zoom:"), page), 1, 0);
    auto* zoomCenter = new QComboBox(page);
    zoomCenter->setObjectName(QStringLiteral("zoom_center"));
    zoomCenter->addItems({tr("Sprite's axis"), tr("Center of workspace")});
    bindIndex(zoomCenter, QStringLiteral("zoom_center"), 0);
    zoom->addWidget(zoomCenter, 1, 1);
    zoom->addWidget(new QLabel(tr("Contact box opacity:"), page), 2, 0);
    auto* boxOpacity = new QSlider(Qt::Horizontal, page);
    boxOpacity->setObjectName(QStringLiteral("box_opacity"));
    bindPercent(boxOpacity, QStringLiteral("box_opacity"), 0.5);
    zoom->addWidget(boxOpacity, 2, 1);
    right->addLayout(zoom);

    auto* draw = new QGridLayout;
    draw->setColumnStretch(0, 1);
    draw->setColumnStretch(1, 1);
    auto check = [this, page, draw](const QString& text, const QString& key, int r, int c) {
        auto* box = new QCheckBox(text, page);
        box->setObjectName(key);
        bindCheck(box, key, false);
        draw->addWidget(box, r, c);
    };
    check(tr("Sprite over guide lines"), QStringLiteral("sprite_over_guides"), 0, 0);
    check(tr("Contact box over axis"), QStringLiteral("contact_over_cross"), 0, 1);
    check(tr("Invert sprite axis"), QStringLiteral("reverse_spr_axis"), 1, 0);
    right->addLayout(draw);
    right->addStretch(1);

    auto* layout = new QHBoxLayout(page);
    layout->addLayout(left, 1);
    layout->addLayout(right, 1);
    return page;
}

QWidget* OptionsDialog::createCodeTab()
{
    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("codeTab"));

    auto* display = new QGroupBox(tr("Display:"), page);
    display->setObjectName(QStringLiteral("display"));
    display->setFixedWidth(202);
    auto* displayLayout = new QVBoxLayout(display);
    const QList<std::tuple<QString, QString, bool>> checks = {
        {tr("Line numbers"), QStringLiteral("code_line_numbers"), true},
        {tr("Folding markers"), QStringLiteral("code_folding_markers"), true},
        {tr("Highlight current line"), QStringLiteral("code_highlight_line"), true},
        {tr("Highlight blocks"), QStringLiteral("code_highlight_blocks"), false},
        {tr("Parenthesis matching"), QStringLiteral("code_paren_matching"), true},
        {tr("Current line navigation"), QStringLiteral("auto_expand_tree"), false},
    };
    for (const auto& [text, key, fallback] : checks) {
        auto* box = new QCheckBox(text, display);
        box->setObjectName(key);
        bindCheck(box, key, fallback);
        displayLayout->addWidget(box);
    }
    displayLayout->addStretch(1);

    auto* right = new QVBoxLayout;
    auto* font = new QGridLayout;
    font->setColumnStretch(0, 1);
    font->addWidget(new QLabel(tr("Editor font:"), page), 0, 0);
    font->addWidget(new QLabel(tr("Font size:"), page), 0, 1);
    m_fontName = new QFontComboBox(page);
    m_fontName->setObjectName(QStringLiteral("font_name"));
    // Keep the stored family unless the user picks another, so a font missing here is not replaced on Apply.
    bind(
        QStringLiteral("font_name"), QStringLiteral("Courier New"),
        [this](const QString& family) {
            m_loadedFont = family;
            m_fontTouched = false;
            const QSignalBlocker blocker(m_fontName);
            m_fontName->setCurrentFont(QFont(family));
        },
        [this] { return m_fontTouched ? m_fontName->currentFont().family() : m_loadedFont; });
    connect(m_fontName, &QFontComboBox::currentFontChanged, this, [this] { m_fontTouched = true; });
    font->addWidget(m_fontName, 1, 0);
    auto* size = new QSpinBox(page);
    size->setObjectName(QStringLiteral("font_size"));
    size->setRange(0, 99);
    size->setFixedWidth(100);
    bindSpin(size, QStringLiteral("font_size"), 10);
    font->addWidget(size, 1, 1);
    right->addLayout(font);

    auto* charset = new QGroupBox(tr("Default charset:"), page);
    charset->setObjectName(QStringLiteral("charset"));
    auto* charsetLayout = new QVBoxLayout(charset);
    auto* codePage = new QComboBox(charset);
    codePage->setObjectName(QStringLiteral("default_codepage"));
    for (const TextEncoding& encoding : TextEncoding::all())
        codePage->addItem(encoding.displayText());
    bindIndex(codePage, QStringLiteral("default_codepage"), 0);
    charsetLayout->addWidget(codePage);
    charsetLayout->addWidget(new QLabel(tr("Used when the charset of a file can't be detected."), charset));
    right->addWidget(charset);

    auto* openTab = new QCheckBox(tr("Convert Tab to Spaces while opening"), page);
    openTab->setObjectName(QStringLiteral("open_tabToSpace"));
    bindCheck(openTab, QStringLiteral("open_tabToSpace"), false);
    right->addWidget(openTab);
    auto* saveTab = new QCheckBox(tr("Convert Spaces to Tab while saving"), page);
    saveTab->setObjectName(QStringLiteral("save_spaceToTab"));
    bindCheck(saveTab, QStringLiteral("save_spaceToTab"), false);
    right->addWidget(saveTab);
    right->addStretch(1);

    auto* layout = new QHBoxLayout(page);
    layout->addWidget(display);
    layout->addLayout(right, 1);
    return page;
}

void OptionsDialog::createEnginePages()
{
    QList<QPair<QString, QString>> engines;
    for (const Engine* engine : m_context.engines().engines()) {
        if (OptionsPageRegistry::hasPage(engine->name()))
            engines.append({engine->name(), engine->displayName()});
    }
    for (const QString& name : OptionsPageRegistry::engines()) {
        const bool listed =
            std::any_of(engines.cbegin(), engines.cend(), [&name](const auto& e) { return e.first == name; });
        if (!listed)
            engines.append({name, QString()});
    }

    m_engineStack = new QStackedWidget;
    for (const auto& [name, displayName] : std::as_const(engines)) {
        OptionsPage* page = OptionsPageRegistry::create(name, m_context.engineConfig(name), m_engineStack);
        if (!page)
            continue;
        m_enginePages.insert(name, page);
        m_engineOrder << name;
        m_engineStack->addWidget(page);
        m_engines->addItem(displayName.isEmpty() ? page->title() : displayName, name);
    }
    m_engines->setEnabled(!m_engineOrder.isEmpty());
    if (m_engineOrder.isEmpty()) {
        delete m_engineStack;
        m_engineStack = nullptr;
        return;
    }
    m_engineTab = m_engineStack;
    m_tabs->addTab(m_engineTab, m_enginePages.value(m_engineOrder.first())->title());
    connect(m_engines, &QComboBox::currentIndexChanged, this, &OptionsDialog::selectEngine);
    selectEngine(0);
}

void OptionsDialog::selectEngine(int index)
{
    if (!m_engineStack || index < 0 || index >= int(m_engineOrder.size()))
        return;
    OptionsPage* page = m_enginePages.value(m_engineOrder[index]);
    m_engineStack->setCurrentWidget(page);
    m_tabs->setTabText(m_tabs->indexOf(m_engineTab), page->title());
}

QLayout* OptionsDialog::createButtonRow()
{
    m_engines = new QComboBox(this);
    m_engines->setObjectName(QStringLiteral("engines"));
    m_engines->setFixedWidth(150);
    auto* defaults = new QPushButton(tr("Load defaults"), this);
    auto* open = new QPushButton(tr("Open..."), this);
    auto* save = new QPushButton(tr("Save..."), this);
    m_applyButton = new QPushButton(tr("Apply"), this);
    auto* close = new QPushButton(tr("Close"), this);
    defaults->setObjectName(QStringLiteral("defaultsBut"));
    open->setObjectName(QStringLiteral("openBut"));
    save->setObjectName(QStringLiteral("saveBut"));
    m_applyButton->setObjectName(QStringLiteral("applyBut"));
    close->setObjectName(QStringLiteral("closeBut"));
    for (QPushButton* button : {defaults, open, save, m_applyButton, close})
        button->setAutoDefault(false);
    defaults->setMinimumWidth(120);

    connect(defaults, &QPushButton::clicked, this, &OptionsDialog::loadDefaults);
    connect(open, &QPushButton::clicked, this, &OptionsDialog::openClicked);
    connect(save, &QPushButton::clicked, this, &OptionsDialog::saveClicked);
    connect(m_applyButton, &QPushButton::clicked, this, &OptionsDialog::applyClicked);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);

    auto* row = new QHBoxLayout;
    row->addWidget(m_engines);
    row->addWidget(defaults);
    row->addStretch(1);
    row->addWidget(open);
    row->addWidget(save);
    row->addWidget(m_applyButton);
    row->addWidget(close);
    return row;
}

// ---------------------------------------------------------------------------------------------
// Bindings

void OptionsDialog::bind(const QStringList& keys, const QStringList& fallbacks,
                         std::function<void(const QStringList&)> read, std::function<QStringList()> write)
{
    Q_ASSERT(keys.size() == fallbacks.size());
    m_bindings.push_back({keys, fallbacks, std::move(read), std::move(write)});
}

void OptionsDialog::bind(const QString& key, const QString& fallback, std::function<void(const QString&)> read,
                         std::function<QString()> write)
{
    bind(
        QStringList{key}, QStringList{fallback}, [read](const QStringList& values) { read(values.first()); },
        [write] { return QStringList{write()}; });
}

void OptionsDialog::bindCheck(QAbstractButton* box, const QString& key, bool fallback)
{
    bind(
        key, Config::encodeBool(fallback),
        [box, fallback](const QString& value) {
            // An empty value counts as false, an unreadable one as the built-in default.
            const QString v = value.trimmed();
            box->setChecked(!v.isEmpty() && Config::decodeBool(v, fallback));
        },
        [box] { return Config::encodeBool(box->isChecked()); });
}

void OptionsDialog::bindSpin(QSpinBox* spin, const QString& key, int fallback)
{
    bind(
        key, QString::number(fallback),
        [spin, fallback](const QString& value) {
            bool ok = false;
            const int v = value.trimmed().toInt(&ok);
            spin->setValue(ok ? v : fallback);
        },
        [spin] { return QString::number(spin->value()); });
}

void OptionsDialog::bindDoubleSpin(QDoubleSpinBox* spin, const QString& key, double fallback)
{
    bind(
        key, QString::number(fallback),
        [spin, fallback](const QString& value) {
            bool ok = false;
            const double v = value.trimmed().toDouble(&ok);
            spin->setValue(ok ? v : fallback);
        },
        [spin] { return QString::number(spin->value(), 'g', 6); });
}

void OptionsDialog::bindLine(QLineEdit* edit, const QString& key)
{
    bind(
        key, QString(), [edit](const QString& value) { edit->setText(value); },
        [edit] { return edit->text().trimmed(); });
}

void OptionsDialog::bindIndex(QComboBox* combo, const QString& key, int fallbackValue, int offset)
{
    bind(
        key, QString::number(fallbackValue),
        [combo, fallbackValue, offset](const QString& value) {
            bool ok = false;
            int index = value.trimmed().toInt(&ok) - offset;
            if (!ok || index < 0 || index >= combo->count())
                index = fallbackValue - offset;
            combo->setCurrentIndex(qBound(0, index, combo->count() - 1));
        },
        [combo, offset] { return QString::number(combo->currentIndex() + offset); });
}

void OptionsDialog::bindAnswer(QComboBox* combo, const QString& key)
{
    bind(
        key, QString(),
        [combo](const QString& value) {
            const QString v = value.trimmed();
            combo->setCurrentIndex(v.isEmpty() || v == kAnswerValues[0] ? 0 : v == kAnswerValues[1] ? 1 : 2);
        },
        [combo] { return kAnswerValues.value(combo->currentIndex()); });
}

void OptionsDialog::bindFlag(QAbstractButton* box, const QString& key)
{
    bind(
        key, QString(), [box](const QString& value) { box->setChecked(Config::decodeBool(value, false)); },
        [box] { return Config::encodeBool(box->isChecked()); });
}

void OptionsDialog::bindRadio(QButtonGroup* group, const QString& key, int fallback)
{
    bind(
        key, QString::number(fallback),
        [group, fallback](const QString& value) {
            bool ok = false;
            const int id = value.trimmed().toInt(&ok);
            QAbstractButton* button = ok ? group->button(id) : nullptr;
            if (!button)
                button = group->button(fallback);
            if (button)
                button->setChecked(true);
        },
        [group, fallback] { return QString::number(group->checkedId() < 0 ? fallback : group->checkedId()); });
}

void OptionsDialog::bindPercent(QSlider* slider, const QString& key, double fallback)
{
    slider->setRange(0, 100);
    bind(
        key, percentText(fallback),
        [slider, fallback](const QString& value) {
            bool ok = false;
            const double v = value.trimmed().toDouble(&ok);
            slider->setValue(int(std::lround((ok ? v : fallback) * 100.0)));
        },
        [slider] { return percentText(slider->value() / 100.0); });
}

void OptionsDialog::bindName(QComboBox* combo, const QString& key, const QString& fallback)
{
    bind(
        key, fallback,
        [combo, fallback](const QString& value) {
            const QString name = value.trimmed().isEmpty() ? fallback : value.trimmed();
            if (combo->findText(name) < 0)
                combo->addItem(name);
            combo->setCurrentIndex(combo->findText(name));
        },
        [combo] { return combo->currentText(); });
}

QString OptionsDialog::currentValue(const QString& key, const QString& fallback) const
{
    const Config& config = m_context.config();
    return config.contains(key) ? config.value(key) : fallback;
}

QString OptionsDialog::shippedValue(const QString& key, const QString& fallback) const
{
    const Config& config = m_context.config();
    return config.hasDefault(key) ? config.defaultValue(key) : fallback;
}

void OptionsDialog::readBindings(const ValueSource& source)
{
    // Theme combo boxes must not reload the colors while they are filled.
    const QSignalBlocker colorBlocker(m_colorset);
    for (const Binding& binding : m_bindings) {
        QStringList values;
        for (qsizetype i = 0; i < binding.keys.size(); ++i)
            values << source(binding.keys[i], binding.fallbacks[i]);
        binding.read(values);
    }
    updateDependentControls();
}

QMap<QString, QString> OptionsDialog::boundValues() const
{
    QMap<QString, QString> values;
    for (const Binding& binding : m_bindings) {
        const QStringList written = binding.write();
        for (qsizetype i = 0; i < binding.keys.size() && i < written.size(); ++i)
            values.insert(binding.keys[i], written[i]);
    }
    return values;
}

QStringList OptionsDialog::boundKeys() const
{
    QStringList keys;
    for (const Binding& binding : m_bindings)
        keys << binding.keys;
    return keys;
}

void OptionsDialog::updateDependentControls()
{
    const bool customCross = m_crosshair && m_crosshair->checkedId() == 2;
    for (QWidget* widget : std::as_const(m_crosshairFiles))
        widget->setEnabled(customCross);
    const bool customBack = m_background && m_background->checkedId() == 2;
    for (QWidget* widget : std::as_const(m_backgroundFiles))
        widget->setEnabled(customBack);
    if (m_dpiScale && m_dpiAuto)
        m_dpiScale->setEnabled(!m_dpiAuto->isChecked());
}

// ---------------------------------------------------------------------------------------------
// Load, defaults, apply

void OptionsDialog::load()
{
    m_extraConfig.clear();
    m_extraShortcuts.clear();
    readBindings([this](const QString& key, const QString& fallback) { return currentValue(key, fallback); });
    showColorSet(m_colorset->currentText());
    loadShortcutTables(false);
    for (OptionsPage* page : std::as_const(m_enginePages))
        page->load();
}

void OptionsDialog::loadDefaults()
{
    m_extraConfig.clear();
    m_extraShortcuts.clear();
    readBindings([this](const QString& key, const QString& fallback) { return shippedValue(key, fallback); });
    showColorSet(m_colorset->currentText());
    loadShortcutTables(true);
    for (OptionsPage* page : std::as_const(m_enginePages))
        page->loadDefaults();
}

void OptionsDialog::loadShortcutTables(bool defaults)
{
    const ShortcutSettings& settings = m_context.shortcuts();
    m_shortcuts->refreshActions();
    QHash<QString, QKeySequence> current;
    QHash<QString, QKeySequence> shipped;
    for (const QString& id : m_shortcuts->actionIds()) {
        shipped.insert(id, settings.defaultKeys(id));
        current.insert(id, defaults ? settings.defaultKeys(id) : settings.keys(id));
    }
    m_shortcuts->setDefaultKeys(shipped);
    m_shortcuts->setKeysTable(current);
    m_shortcutsReset = defaults;
}

bool OptionsDialog::apply()
{
    if (m_applying)
        return false;
    m_applying = true;
    Config& config = m_context.config();
    ShortcutSettings& shortcuts = m_context.shortcuts();
    QStringList errors;

    for (auto it = m_extraConfig.cbegin(); it != m_extraConfig.cend(); ++it)
        config.setValue(it.key(), it.value());
    m_extraConfig.clear();
    const QMap<QString, QString> values = boundValues();
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (!config.contains(it.key()) || config.value(it.key()) != it.value())
            config.setValue(it.key(), it.value());
    }

    if (m_colorset->currentText() == kCustomColorSet) {
        QString error;
        ColorSet set = m_colors->colorSet();
        set.setName(kCustomColorSet);
        if (set.save(customColorSetPath(), &error))
            m_colors->setModified(false);
        else
            errors << error;
    }

    if (m_shortcutsReset)
        shortcuts.resetAll();
    m_shortcutsReset = false;
    for (auto it = m_extraShortcuts.cbegin(); it != m_extraShortcuts.cend(); ++it)
        shortcuts.setKeys(it.key(), ShortcutSettings::fromText(it.value()));
    m_extraShortcuts.clear();
    const QHash<QString, QKeySequence> table = m_shortcuts->keysTable();
    for (auto it = table.cbegin(); it != table.cend(); ++it) {
        if (!shortcuts.hasEntry(it.key()) ||
            ShortcutSettings::toText(shortcuts.keys(it.key())) != ShortcutSettings::toText(it.value()))
            shortcuts.setKeys(it.key(), it.value());
    }
    m_context.actions().applyShortcuts(shortcuts);

    for (OptionsPage* page : std::as_const(m_enginePages))
        page->apply();

    m_context.history().setMaxEntries(config.intValue(QStringLiteral("max_history"), m_context.history().maxEntries()));

    QString error;
    if (!config.userPath().isEmpty() && !config.save(&error))
        errors << error;
    if (!shortcuts.userPath().isEmpty() && !shortcuts.save(&error))
        errors << error;

    // Receivers rebuild the interface. An Apply they trigger themselves is ignored.
    Q_EMIT settingsApplied();
    m_applying = false;

    if (m_shortcuts->actionIds() != m_context.actions().ids())
        loadShortcutTables(false);

    if (!errors.isEmpty() && isVisible())
        QMessageBox::warning(this, tr("Options"), tr("The settings could not be saved:\n%1").arg(errors.join('\n')));
    return errors.isEmpty();
}

void OptionsDialog::applyClicked()
{
    m_applyButton->setEnabled(false);
    apply();
    m_applyButton->setEnabled(true);
}

// ---------------------------------------------------------------------------------------------
// Themes and colors

void OptionsDialog::refreshThemeLists()
{
    const ThemeManager& theme = m_context.theme();
    auto fill = [](QComboBox* combo, QStringList names, const QString& first) {
        const QString current = combo->currentText();
        const QSignalBlocker blocker(combo);
        combo->clear();
        if (!first.isEmpty()) {
            names.removeAll(first);
            names.prepend(first);
        }
        combo->addItems(names);
        if (!current.isEmpty()) {
            if (combo->findText(current) < 0)
                combo->addItem(current);
            combo->setCurrentIndex(combo->findText(current));
        }
    };
    fill(m_preset, theme.availablePresets(), {});
    fill(m_iconset, theme.availableIconSets(), {});
    fill(m_stylesheet, theme.availableStyles(), {});
    fill(m_colorset, theme.availableColorSets(), kCustomColorSet);
}

QString OptionsDialog::customColorSetPath() const
{
    return Paths::userDir() + QStringLiteral("/colorsets/") + kCustomColorSet + QStringLiteral(".xml");
}

void OptionsDialog::showColorSet(const QString& name)
{
    ColorSet set;
    if (name == kCustomColorSet) {
        // Keep the colors on screen unless a saved Custom set exists and nothing was edited.
        if (m_colors->isModified() || !set.load(customColorSetPath()))
            set = m_colors->colorSet();
    } else {
        const QString path = m_context.theme().colorSetPath(name);
        if (path.isEmpty() || !set.load(path))
            set = ColorSet();
    }
    const bool modified = name == kCustomColorSet && m_colors->isModified();
    set.setName(name);
    m_colors->setColorSet(set);
    m_colors->setModified(modified);
}

void OptionsDialog::colorsEdited()
{
    if (m_colorset->currentText() == kCustomColorSet)
        return;
    m_syncingColors = true;
    m_colorset->setCurrentIndex(m_colorset->findText(kCustomColorSet));
    m_syncingColors = false;
}

void OptionsDialog::colorSaveRequested()
{
    bool ok = false;
    const QString current = m_colorset->currentText();
    const QString name = QInputDialog::getText(this, tr("Save colorset"), tr("Name:"), QLineEdit::Normal,
                                               current == kCustomColorSet ? QString() : current, &ok)
                             .trimmed();
    if (!ok || name.isEmpty())
        return;
    const QString path = Paths::userDir() + QStringLiteral("/colorsets/") + name + QStringLiteral(".xml");
    if (QFileInfo::exists(path) &&
        QMessageBox::question(this, tr("Save colorset"), tr("The colorset \"%1\" exists. Replace it?").arg(name)) !=
            QMessageBox::Yes)
        return;
    QString error;
    if (!saveColorSet(name, &error))
        QMessageBox::warning(this, tr("Save colorset"), error);
}

bool OptionsDialog::saveColorSet(const QString& name, QString* error)
{
    const QString trimmed = name.trimmed();
    if (!validColorSetName(trimmed)) {
        if (error)
            *error = tr("\"%1\" is not a valid colorset name.").arg(trimmed);
        return false;
    }
    ColorSet set = m_colors->colorSet();
    set.setName(trimmed);
    const QString path = Paths::userDir() + QStringLiteral("/colorsets/") + trimmed + QStringLiteral(".xml");
    if (!set.save(path, error))
        return false;
    m_syncingColors = true;
    refreshThemeLists();
    if (m_colorset->findText(trimmed) < 0)
        m_colorset->addItem(trimmed);
    m_colorset->setCurrentIndex(m_colorset->findText(trimmed));
    m_syncingColors = false;
    m_colors->setColorSet(set);
    return true;
}

void OptionsDialog::refreshLanguageInfo()
{
    const QString code = m_language->currentData().toString();
    LanguageInfo info = LanguageInfo::read(Paths::userDir() + QStringLiteral("/languages/") + code);
    if (info.code.isEmpty())
        info = LanguageInfo::read(Paths::dataDir() + QStringLiteral("/languages/") + code);
    if (info.code.isEmpty() && code == LanguageInfo::builtin().code)
        info = LanguageInfo::builtin();
    m_languageAuthor->setText(tr("Author: %1").arg(info.author));
    m_languageContact->setText(tr("Contact: %1").arg(info.contact));
}

// ---------------------------------------------------------------------------------------------
// Settings file

bool OptionsDialog::exportSettings(const QString& path, QString* error) const
{
    SettingsBundle bundle;
    bundle.config = m_context.config().allValues();
    for (auto it = m_extraConfig.cbegin(); it != m_extraConfig.cend(); ++it)
        bundle.config.insert(it.key(), it.value());
    const QMap<QString, QString> values = boundValues();
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        bundle.config.insert(it.key(), it.value());

    const ShortcutSettings& shortcuts = m_context.shortcuts();
    for (const QString& id : shortcuts.actions())
        bundle.shortcuts.insert(id, ShortcutSettings::toText(shortcuts.keys(id)));
    for (auto it = m_extraShortcuts.cbegin(); it != m_extraShortcuts.cend(); ++it)
        bundle.shortcuts.insert(it.key(), it.value());
    const QHash<QString, QKeySequence> table = m_shortcuts->keysTable();
    for (auto it = table.cbegin(); it != table.cend(); ++it)
        bundle.shortcuts.insert(it.key(), ShortcutSettings::toText(it.value()));

    for (auto it = m_enginePages.cbegin(); it != m_enginePages.cend(); ++it)
        bundle.engines.insert(it.key(), it.value()->values());

    if (m_colorset->currentText() == kCustomColorSet) {
        bundle.colorSetName = kCustomColorSet;
        bundle.colors = m_colors->colorSet().toRecords();
    }
    return bundle.save(path, error);
}

bool OptionsDialog::importSettings(const QString& path, QString* error)
{
    SettingsBundle bundle;
    if (!bundle.load(path, error))
        return false;

    readBindings([this, &bundle](const QString& key, const QString& fallback) {
        return bundle.config.contains(key) ? bundle.config.value(key) : currentValue(key, fallback);
    });
    const QStringList bound = boundKeys();
    m_extraConfig.clear();
    for (auto it = bundle.config.cbegin(); it != bundle.config.cend(); ++it) {
        if (!bound.contains(it.key()))
            m_extraConfig.insert(it.key(), it.value());
    }

    QHash<QString, QKeySequence> table = m_shortcuts->keysTable();
    m_extraShortcuts.clear();
    for (auto it = bundle.shortcuts.cbegin(); it != bundle.shortcuts.cend(); ++it) {
        if (table.contains(it.key()))
            table.insert(it.key(), ShortcutSettings::fromText(it.value()));
        else
            m_extraShortcuts.insert(it.key(), it.value());
    }
    m_shortcuts->setKeysTable(table);

    if (!bundle.colors.isEmpty()) {
        ColorSet set;
        set.setRecords(bundle.colors);
        set.setName(kCustomColorSet);
        m_syncingColors = true;
        m_colorset->setCurrentIndex(m_colorset->findText(kCustomColorSet));
        m_syncingColors = false;
        m_colors->setColorSet(set);
        m_colors->setModified(true);
    } else {
        showColorSet(m_colorset->currentText());
    }

    for (auto it = bundle.engines.cbegin(); it != bundle.engines.cend(); ++it) {
        if (OptionsPage* page = m_enginePages.value(it.key()))
            page->setValues(it.value());
    }
    return true;
}

void OptionsDialog::openClicked()
{
    const QString filter = tr("Open Factory Studio settings (*.%1);;All files (*)").arg(SettingsBundle::fileSuffix());
    const QString path = FileDialogs::getOpenFileName(this, tr("Open settings"), QDir::homePath(), filter);
    if (path.isEmpty())
        return;
    QString error;
    if (!importSettings(path, &error))
        QMessageBox::warning(this, tr("Open settings"),
                             tr("Cannot read %1:\n%2").arg(QDir::toNativeSeparators(path), error));
}

void OptionsDialog::saveClicked()
{
    const QString filter = tr("Open Factory Studio settings (*.%1)").arg(SettingsBundle::fileSuffix());
    QString path = FileDialogs::getSaveFileName(this, tr("Save settings"), QDir::homePath(), filter);
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().isEmpty())
        path += QLatin1Char('.') + SettingsBundle::fileSuffix();
    QString error;
    if (!exportSettings(path, &error))
        QMessageBox::warning(this, tr("Save settings"),
                             tr("Cannot write %1:\n%2").arg(QDir::toNativeSeparators(path), error));
}

} // namespace OFS
