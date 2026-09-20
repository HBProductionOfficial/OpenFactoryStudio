#include "shell/SpritesEditor.h"

#include "core/actions/ActionRegistry.h"
#include "core/app/AppContext.h"
#include "core/app/Messages.h"
#include "core/editors/Organize.h"
#include "core/engine/Engine.h"
#include "core/project/Project.h"
#include "core/settings/Config.h"
#include "core/sprites/QtImages.h"
#include "core/sprites/SpriteEdits.h"
#include "core/sprites/SpriteExport.h"
#include "core/sprites/SpriteFile.h"
#include "core/sprites/SpriteImages.h"
#include "core/sprites/SpriteTransfer.h"
#include "core/widgets/GraphicsView.h"
#include "formats/image/ImageOps.h"
#include "shell/FileDialogs.h"
#include "shell/FileFilters.h"
#include "shell/ImageEditor.h"
#include "shell/OrganizerBinding.h"
#include "shell/PalettesPanel.h"
#include "shell/SaveChecks.h"
#include "shell/SpritePage.h"
#include "shell/dialogs/PaletteDialogs.h"
#include "shell/dialogs/SpriteDialogs.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressDialog>
#include <QScrollBar>
#include <QSpinBox>
#include <QStringListModel>
#include <QTimer>
#include <QToolButton>

#include <cstring>

namespace OFS {

namespace {

int bitDepth(const SpriteEntry& entry)
{
    const Formats::SffSprite& payload = entry.pixels->payload;
    if (payload.image)
        return payload.image->isIndexed() ? 8 : (payload.colorDepth == 24 ? 24 : 32);
    switch (payload.compression) {
    case Formats::SffCompression::Png24:
        return 24;
    case Formats::SffCompression::Png32:
        return 32;
    default:
        return 8;
    }
}

template <typename T>
T* control(QWidget* panel, const char* name)
{
    return panel ? panel->findChild<T*>(QLatin1String(name)) : nullptr;
}

} // namespace

SpritesEditor::SpritesEditor(AppContext& context, QObject* parent)
    : QObject(parent)
    , m_context(context)
    , m_palettes(new PalettesPanel(context, this))
{}

void SpritesEditor::bindPalettes(QWidget* panel)
{
    m_palettes->bindPanel(panel);
}

void SpritesEditor::bindPanel(QWidget* panel)
{
    m_selector = control<QScrollBar>(panel, "spr_sel");
    m_selectorLabel = control<QLabel>(panel, "spr_sel_lb");
    m_group = control<QSpinBox>(panel, "spr_grp");
    m_number = control<QSpinBox>(panel, "spr_num");
    m_axisX = control<QSpinBox>(panel, "spr_xaxis");
    m_axisY = control<QSpinBox>(panel, "spr_yaxis");
    m_info = control<QLabel>(panel, "spr_info_lb");
    m_decompress = control<QCheckBox>(panel, "spr_dec_load");
    m_showTransparent = control<QCheckBox>(panel, "spr_trans_color");
    m_onion = control<QCheckBox>(panel, "spr_onion_skin");
    m_onionSelector = control<QScrollBar>(panel, "spr_onion_sel");
    m_onionLabel = control<QLabel>(panel, "spr_onion_lb");
    m_name = control<QLineEdit>(panel, "spr_name");
    m_panel = panel;
    bindImageControls(panel);

    if (m_name) {
        m_name->setMaxLength(13);
        connect(m_name, &QLineEdit::editingFinished, this, [this] {
            if (m_updating || !m_page)
                return;
            const int index = m_page->current();
            const SpriteDocument& current = m_page->file()->document();
            if (index < 0 || index >= current.spriteCount() || current.sprites()[index].name == m_name->text())
                return;
            SpriteDocument doc = current;
            doc.sprites()[index].name = m_name->text();
            m_page->file()->apply(tr("Sprite name"), doc);
        });
    }
    if (m_selector)
        connect(m_selector, &QScrollBar::valueChanged, this, [this](int value) {
            if (!m_updating && m_page)
                m_page->setCurrent(value);
        });
    for (QSpinBox* box : {m_group.data(), m_number.data()}) {
        if (box)
            connect(box, &QSpinBox::editingFinished, this, &SpritesEditor::applyIdentifiers);
    }
    for (QSpinBox* box : {m_axisX.data(), m_axisY.data()}) {
        if (box)
            connect(box, &QSpinBox::valueChanged, this, &SpritesEditor::applyAxis);
    }
    if (m_decompress)
        connect(m_decompress, &QCheckBox::toggled, this, [this](bool on) {
            if (m_updating || !m_page)
                return;
            SpriteDocument doc = m_page->file()->document();
            SpriteEdits::setDecompressOnLoad(doc, m_page->current(), on);
            m_page->file()->apply(tr("Decompress on load"), doc);
        });
    if (m_showTransparent) {
        m_showTransparent->setChecked(m_context.config().boolValue(QStringLiteral("show_trans_color"), false));
        connect(m_showTransparent, &QCheckBox::toggled, this, [this](bool on) {
            m_context.config().setValue(QStringLiteral("show_trans_color"), on);
            if (m_page)
                m_page->setShowTransparent(on);
        });
    }
    if (m_onion)
        connect(m_onion, &QCheckBox::toggled, this, [this](bool on) {
            if (on && m_onionSelector && m_page) {
                const bool updating = std::exchange(m_updating, true);
                m_onionSelector->setValue(m_page->current());
                m_updating = updating;
            }
            updateOnion();
        });
    if (m_onionSelector)
        connect(m_onionSelector, &QScrollBar::valueChanged, this, [this] {
            if (!m_updating)
                updateOnion();
        });
    refresh();
}

void SpritesEditor::bindOrganizer(QWidget* panel)
{
    if (m_organizer)
        disconnect(m_organizer, nullptr, this, nullptr);
    if (m_organizePalettes)
        disconnect(m_organizePalettes, nullptr, this, nullptr);
    m_organizer = control<QListView>(panel, "org_list");
    m_organizePalettes = control<QToolButton>(panel, "org_palette");
    if (m_organizePalettes)
        m_organizePalettes->setCheckable(true);
    setOrganizerActive(m_organizerActive);
}

void SpritesEditor::setOrganizerActive(bool active)
{
    m_organizerActive = active;
    if (m_organizer)
        disconnect(m_organizer, nullptr, this, nullptr);
    if (m_organizePalettes)
        disconnect(m_organizePalettes, nullptr, this, nullptr);
    if (!active) {
        OrganizerDrops::disable(m_organizer, this);
        return;
    }
    if (m_organizer) {
        if (!m_organizerModel)
            m_organizerModel = new QStringListModel(this);
        m_organizer->setModel(m_organizerModel);
        m_organizer->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_organizer->setEditTriggers(QAbstractItemView::NoEditTriggers);
        connect(m_organizer, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
            if (m_page && !organizePalettes())
                m_page->setCurrent(index.row());
        });
        OrganizerDrops::enable(m_organizer, this, [this](int target) { dropOrganizerRows(target); });
    }
    if (m_organizePalettes)
        connect(m_organizePalettes, &QToolButton::toggled, this, &SpritesEditor::refreshOrganizer);
    refreshOrganizer();
}

void SpritesEditor::dropOrganizerRows(int target)
{
    if (!m_page)
        return;
    const QList<int> rows = organizerSelection();
    SpriteFile* file = m_page->file();
    const SpriteDocument& doc = file->document();
    const bool palettes = organizePalettes();
    const int count = palettes ? int(doc.palettes().size()) : doc.spriteCount();
    QList<int> moved;
    const QList<int> order = Organize::dropOrder(count, rows, target, &moved);
    if (rows.isEmpty() || moved == rows)
        return;
    SpriteDocument edited = doc;
    for (int i = 0; i < count; ++i) {
        if (palettes)
            edited.palettes()[i] = doc.palettes()[order[i]];
        else
            edited.sprites()[i] = doc.sprites()[order[i]];
    }
    file->apply(tr("Organize"), edited);
    if (m_organizer && m_organizerModel) {
        m_organizer->selectionModel()->clearSelection();
        for (int row : std::as_const(moved))
            m_organizer->selectionModel()->select(m_organizerModel->index(row), QItemSelectionModel::Select);
    }
}

bool SpritesEditor::organizePalettes() const
{
    return m_organizePalettes && m_organizePalettes->isChecked();
}

QList<int> SpritesEditor::organizerSelection() const
{
    QList<int> rows;
    if (!m_organizer || !m_organizer->selectionModel())
        return rows;
    for (const QModelIndex& index : m_organizer->selectionModel()->selectedRows())
        rows << index.row();
    return rows;
}

void SpritesEditor::refreshOrganizer()
{
    if (!m_organizer || !m_organizerModel || !m_organizerActive)
        return;
    QStringList rows;
    if (m_page) {
        const SpriteDocument& doc = m_page->file()->document();
        if (organizePalettes()) {
            for (const SpritePalettePtr& palette : doc.palettes())
                rows << QStringLiteral("%1, %2").arg(palette->palette.group).arg(palette->palette.number);
        } else {
            for (const SpriteEntry& entry : doc.sprites())
                rows << spriteCaption(entry);
        }
    }
    const QList<int> selected = organizerSelection();
    m_organizerModel->setStringList(rows);
    for (int row : selected) {
        if (row < rows.size())
            m_organizer->selectionModel()->select(m_organizerModel->index(row), QItemSelectionModel::Select);
    }
}

void SpritesEditor::setPage(SpritePage* page)
{
    if (m_page == page)
        return;
    if (m_page)
        disconnect(m_page, nullptr, this, nullptr);
    const bool wasImage = imageMode();
    m_page = page;
    m_palettes->setPage(page);
    m_palettes->setImageMode(imageMode());
    if (m_page) {
        connect(m_page, &SpritePage::currentChanged, this, &SpritesEditor::refresh);
        connect(m_page->file(), &SpriteFile::documentChanged, this, &SpritesEditor::refresh);
        connect(m_page, &SpritePage::imageModeChanged, this, &SpritesEditor::imageModeSwitched);
        connect(m_page->imageEditor(), &ImageEditor::documentChanged, this, &SpritesEditor::refreshLayers);
        connect(m_page->file(), &SpriteFile::documentChanged, this, &SpritesEditor::refreshExplorer);
        connect(m_page->imageEditor(), &ImageEditor::toolChanged, this, &SpritesEditor::syncTool);
        connect(m_page->imageEditor(), &ImageEditor::colorsChanged, this, [this] {
            for (QWidget* swatch : {m_foreground.data(), m_background.data()}) {
                if (swatch)
                    swatch->update();
            }
        });
        if (m_showTransparent)
            m_page->setShowTransparent(m_showTransparent->isChecked());
    }
    refresh();
    updateMode();
    refreshLayers();
    if (imageMode())
        syncTool();
    refreshExplorer();
    if (wasImage != imageMode()) {
        Q_EMIT imageModeChanged(imageMode());
        Q_EMIT explorerChanged();
    }
}

void SpritesEditor::refresh()
{
    const bool updating = std::exchange(m_updating, true);
    const SpriteDocument* doc = m_page ? &m_page->file()->document() : nullptr;
    const int count = doc ? doc->spriteCount() : 0;
    const int current = m_page ? m_page->current() : 0;
    const bool has = count > 0 && current >= 0 && current < count;
    if (m_empty != (count == 0)) {
        m_empty = count == 0;
        QTimer::singleShot(0, this, &SpritesEditor::emptyChanged);
    }
    if (m_selector) {
        m_selector->setRange(0, qMax(0, count - 1));
        m_selector->setValue(current);
    }
    if (m_onionSelector)
        m_onionSelector->setRange(0, qMax(0, count - 1));
    if (m_selectorLabel)
        m_selectorLabel->setText(count > 0 ? QStringLiteral("%1 / %2").arg(current + 1).arg(count) : QString());
    for (QWidget* w : std::initializer_list<QWidget*>{m_group, m_number, m_axisX, m_axisY, m_decompress}) {
        if (w)
            w->setEnabled(has);
    }
    if (has) {
        const SpriteEntry& entry = doc->sprites()[current];
        if (m_group)
            m_group->setValue(entry.group());
        if (m_number)
            m_number->setValue(entry.number());
        // Options > Graphics > Invert sprite axis: the spins show the image's offset from the axis (the negated axis).
        // The axis itself and the drawing do not change.
        const int sign = axisSign();
        if (m_axisX)
            m_axisX->setValue(sign * entry.header.axisX);
        if (m_axisY)
            m_axisY->setValue(sign * entry.header.axisY);
        // Also enabled for SFF v1 files because the flag is kept for a later save as v2.
        if (m_decompress)
            m_decompress->setChecked(SpriteEdits::decompressesOnLoad(*doc, current));
        if (m_info) {
            const bool shared = doc->linkedSprites(current).size() > 1;
            m_info->setText(QStringLiteral("%1 x %2 px (%3 bit) - %4")
                                .arg(entry.width())
                                .arg(entry.height())
                                .arg(bitDepth(entry))
                                .arg(shared ? tr("shared") : tr("unique")));
        }
        if (m_name) {
            // The name is the SFF v1 comment: editable in v1 files only, kept for the others.
            m_name->setEnabled(doc->version() == Formats::SffVersion::V1);
            m_name->setText(entry.name);
        }
    } else {
        if (m_info)
            m_info->clear();
        if (m_name) {
            m_name->clear();
            m_name->setEnabled(false);
        }
    }
    m_updating = updating;
    updateOnionLabel();
    refreshOrganizer();
}

void SpritesEditor::applyIdentifiers()
{
    if (m_updating || !m_page || !m_group || !m_number)
        return;
    const SpriteDocument& current = m_page->file()->document();
    const int index = m_page->current();
    if (index < 0 || index >= current.spriteCount())
        return;
    const SpriteEntry& entry = current.sprites()[index];
    if (entry.group() == m_group->value() && entry.number() == m_number->value())
        return;
    SpriteDocument doc = current;
    SpriteEdits::setIdentifiers(doc, index, m_group->value(), m_number->value());
    m_page->file()->apply(tr("Sprite group and index"), doc);
}

void SpritesEditor::applyAxis()
{
    if (m_updating || !m_page || !m_axisX || !m_axisY)
        return;
    const int index = m_page->current();
    if (index < 0 || index >= m_page->file()->document().spriteCount())
        return;
    SpriteDocument doc = m_page->file()->document();
    const int sign = axisSign();
    SpriteEdits::setAxis(doc, index, sign * m_axisX->value(), sign * m_axisY->value());
    m_page->file()->applyAxisChange(index, doc);
}

int SpritesEditor::axisSign() const
{
    return m_context.config().boolValue(QStringLiteral("reverse_spr_axis"), false) ? -1 : 1;
}

void SpritesEditor::updateOnion()
{
    if (!m_page)
        return;
    const bool on = m_onion && m_onion->isChecked();
    const int index = on && m_onionSelector ? m_onionSelector->value() : -1;
    m_page->setOnionSkin(index,
                         m_context.config().value(QStringLiteral("onion_opacity"), QStringLiteral("0.74")).toDouble());
    updateOnionLabel();
}

void SpritesEditor::updateOnionLabel()
{
    if (!m_onionLabel || !m_onionSelector)
        return;
    // "1 / 281 (9000, 1)": the onion-skin sprite's position and identifiers.
    const SpriteDocument* doc = m_page ? &m_page->file()->document() : nullptr;
    const int index = m_onionSelector->value();
    if (!doc || index < 0 || index >= doc->spriteCount()) {
        m_onionLabel->clear();
        return;
    }
    const SpriteEntry& entry = doc->sprites()[index];
    m_onionLabel->setText(QStringLiteral("%1 / %2 (%3, %4)")
                              .arg(index + 1)
                              .arg(doc->spriteCount())
                              .arg(entry.group())
                              .arg(entry.number()));
}

bool SpritesEditor::perform(const QString& id)
{
    if (!m_page)
        return false;
    SpriteFile* file = m_page->file();
    const SpriteDocument& doc = file->document();
    const int current = m_page->current();
    const int count = doc.spriteCount();
    auto runStart = [&doc](int index) {
        int i = index;
        while (i > 0 && doc.sprites()[i - 1].group() == doc.sprites()[index].group())
            --i;
        return i;
    };
    if (id == QLatin1String("spr_prev")) {
        m_page->setCurrent(current - 1);
    } else if (id == QLatin1String("spr_next")) {
        m_page->setCurrent(current + 1);
    } else if (id == QLatin1String("spr_prev_grp")) {
        if (count > 0) {
            const int start = runStart(current);
            if (start > 0)
                m_page->setCurrent(runStart(start - 1));
        }
    } else if (id == QLatin1String("spr_next_grp")) {
        if (count > 0) {
            int i = current;
            while (i < count - 1 && doc.sprites()[i + 1].group() == doc.sprites()[current].group())
                ++i;
            m_page->setCurrent(i + 1 < count ? i + 1 : 0);
        }
    } else if (id.startsWith(QLatin1String("spr_pos_"))) {
        // Moving the sprite one pixel on screen moves its axis the other way.
        const QString direction = id.mid(8);
        const int dx = direction == QLatin1String("left") ? 1 : direction == QLatin1String("right") ? -1 : 0;
        const int dy = direction == QLatin1String("up") ? 1 : direction == QLatin1String("down") ? -1 : 0;
        if (current < count && (dx || dy)) {
            SpriteDocument edited = doc;
            SpriteEdits::moveAxes(edited, {current}, dx, dy);
            file->applyAxisChange(current, edited);
        }
    } else if (id == QLatin1String("spr_delete")) {
        if (current >= count)
            return true;
        if (!confirmDelete(m_page, m_context.config(), tr("Delete"), tr("Do you really want to delete the sprite?")))
            return true;
        SpriteDocument edited = doc;
        SpriteEdits::remove(edited, {current});
        file->apply(tr("Delete sprite"), edited);
    } else if (id == QLatin1String("spr_duplicate")) {
        if (current >= count)
            return true;
        SpriteDocument edited = doc;
        const int copy = SpriteEdits::duplicate(edited, current);
        file->apply(tr("Duplicate sprite"), edited);
        m_page->setCurrent(copy);
    } else if (id.startsWith(QLatin1String("spr_align_"))) {
        const QString where = id.mid(10); // "topleft", "centercenter", "bottomright"...
        const int vertical = where.startsWith(QLatin1String("top"))      ? 0
                             : where.startsWith(QLatin1String("center")) ? 1
                                                                         : 2;
        const int horizontal = where.endsWith(QLatin1String("left"))    ? 0
                               : where.endsWith(QLatin1String("right")) ? 2
                                                                        : 1;
        if (current < count) {
            SpriteDocument edited = doc;
            SpriteEdits::alignToGuides(edited, current, horizontal, vertical, m_page->view()->guides());
            file->applyAxisChange(current, edited);
        }
    } else if (id.startsWith(QLatin1String("org_"))) {
        const bool palettes = organizePalettes();
        SpriteDocument edited = doc;
        QList<int> moved;
        if (id == QLatin1String("org_auto")) {
            SpriteEdits::arrange(edited, palettes);
        } else {
            const int size = palettes ? int(doc.palettes().size()) : count;
            const int delta = id == QLatin1String("org_up")        ? -1
                              : id == QLatin1String("org_down")    ? 1
                              : id == QLatin1String("org_up_10")   ? -10
                              : id == QLatin1String("org_down_10") ? 10
                              : id == QLatin1String("org_top")     ? -size
                                                                   : size;
            const QList<int> rows = organizerSelection();
            if (rows.isEmpty())
                return true;
            moved = SpriteEdits::moveRows(edited, rows, delta, palettes);
            if (moved == rows)
                return true;
        }
        file->apply(tr("Organize"), edited);
        if (m_organizer) {
            m_organizer->selectionModel()->clearSelection();
            for (int row : std::as_const(moved))
                m_organizer->selectionModel()->select(m_organizerModel->index(row), QItemSelectionModel::Select);
        }
    } else if (!performDialog(id) && !performImage(id)) {
        return false;
    }
    return true;
}

namespace {

bool samePixels(const SpriteDocument& a, const SpriteDocument& b)
{
    if (a.spriteCount() != b.spriteCount())
        return false;
    for (int i = 0; i < a.spriteCount(); ++i) {
        if (a.sprites()[i].pixels != b.sprites()[i].pixels ||
            a.sprites()[i].header.axisX != b.sprites()[i].header.axisX ||
            a.sprites()[i].header.axisY != b.sprites()[i].header.axisY)
            return false;
    }
    return true;
}

} // namespace

std::function<AddSpritesOptions::Conflict(int, int)> SpritesEditor::conflictPrompt(QWidget* parent)
{
    return [parent](int group, int index) {
        QMessageBox box(QMessageBox::Question, tr("Conflict"),
                        tr("The index %1 of the group %2 is already in use. Do you want to use the next available "
                           "index?\nOtherwise, choose Discard to skip this sprite or No to insert as is.")
                            .arg(index)
                            .arg(group),
                        QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::Discard | QMessageBox::No |
                            QMessageBox::NoToAll,
                        parent);
        box.setDefaultButton(QMessageBox::Yes);
        switch (box.exec()) {
        case QMessageBox::YesToAll:
            return AddSpritesOptions::Conflict::NextFreeForAll;
        case QMessageBox::Discard:
            return AddSpritesOptions::Conflict::Skip;
        case QMessageBox::No:
            return AddSpritesOptions::Conflict::AsIs;
        case QMessageBox::NoToAll:
            return AddSpritesOptions::Conflict::AsIsForAll;
        default:
            return AddSpritesOptions::Conflict::NextFree;
        }
    };
}

bool SpritesEditor::shareImageChanges(int index, QWidget* parent)
{
    if (!m_page || m_page->file()->document().linkedSprites(index).size() < 2)
        return false;
    return askQuestion(parent, m_context.config(), QStringLiteral("showmsg_share_spr"), tr("Sprite changes"),
                       tr("Do you want to use this image in all other instances of this sprite?"));
}

bool SpritesEditor::saveAs(SpriteFile* file, QWidget* parent)
{
    SpriteFormatDialog format(file->document().version(), parent);
    if (format.exec() != QDialog::Accepted)
        return false;
    const Formats::SffVersion version = format.version();
    if (!confirmSave(file, m_context.config(), parent))
        return false;
    if (version != Formats::SffVersion::V201 && SpriteEdits::hasTrueColor(file->document())) {
        if (QMessageBox::question(parent, tr("Convert sprites to 8-bit"),
                                  tr("This version doesn't support 24/32-bit sprites. Do you want to convert these "
                                     "sprites to 8-bit automatically using an optimized palette?"),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) != QMessageBox::Yes)
            return false;
        SpriteDocument converted = file->document();
        SpriteEdits::convertToIndexed(converted);
        file->apply(tr("Convert sprites to 8-bit"), converted);
    }
    auto* project = qobject_cast<Project*>(file->parent());
    const QString path = FileDialogs::getSaveFileName(parent, tr("Save sprites as..."),
                                                      project ? project->suggestedPath(file) : file->path(),
                                                      tr("Sprites (*.sff);;All files (*.*)"));
    if (path.isEmpty())
        return false;
    const QString folder = project ? QDir(project->directory()).absolutePath() + QLatin1Char('/') : QString();
    // An archive's documents have no folder on disk, so any place counts as inside.
    const bool inside = project && (!project->archivePath().isEmpty() ||
                                    QFileInfo(path).absoluteFilePath().startsWith(folder, Qt::CaseInsensitive));
    QString error;
    bool written = false;
    if (inside) {
        written = file->saveAs(path, version, &error);
    } else {
        if (QMessageBox::question(parent, tr("External file!"),
                                  tr("This file isn't a child of the project directory, so will be ignored on 'Run in "
                                     "engine', 'Backup' and 'Save as' functions. Do you want to save it as copy?"),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return false;
        written = file->writeCopy(path, version, &error);
    }
    if (!written)
        QMessageBox::warning(parent, tr("Save sprites as..."), error);
    return written;
}

void SpritesEditor::addSprites(QWidget* parent, const QString& imageFolder)
{
    SpriteFile* file = m_page->file();
    Config& config = m_context.config();
    QStringList files = FileDialogs::getOpenFileNames(parent, tr("Add"), imageFolder, FileFilters::openImages());
    if (files.isEmpty())
        return;
    config.setValue(QStringLiteral("spr_path"), QFileInfo(files.first()).absolutePath());
    if (config.boolValue(QStringLiteral("sort_file_names"), true))
        files.sort();

    const SpriteDocument& doc = file->document();
    const int current = m_page->current();
    const QPoint axis = current < doc.spriteCount()
                            ? QPoint(doc.sprites()[current].header.axisX, doc.sprites()[current].header.axisY)
                            : QPoint();
    AddSpritesOptions options;
    options.axisX = axis.x();
    options.axisY = axis.y();
    if (config.boolValue(QStringLiteral("show_addsprite_dialog"), true)) {
        AddSpritesDialog dialog(axis, parent);
        dialog.setWindowIcon(
            m_context.actions().action(QStringLiteral("spr_add"))->icon());
        if (dialog.exec() != QDialog::Accepted)
            return;
        options = dialog.options();
    }
    options.askGroup = [parent](const QString& fileName) {
        bool ok = false;
        const int group =
            QInputDialog::getInt(parent, tr("Group"),
                                 tr("Can't detect the group on filename: '%1'.\nChoose a group number to use:")
                                     .arg(QFileInfo(fileName).fileName()),
                                 7777, 0, 65535, 1, &ok);
        return ok ? group : 7777;
    };
    options.askConflict = conflictPrompt(parent);
    // Same as Fighter Factory, "Ask to share identical sprites" shares them without asking.
    options.askShare = [](const QString&, int) {
        return true;
    };

    QList<ImageToAdd> images;
    for (const QString& path : std::as_const(files)) {
        QString error;
        Formats::Image image = loadImageFile(path, &error);
        if (image.pixelBuffer().empty()) {
            QMessageBox::warning(parent, tr("Add"), error);
            continue;
        }
        images << ImageToAdd{path, std::move(image)};
    }
    SpriteDocument edited = doc;
    const QList<int> added = OFS::addSprites(edited, current, images, options);
    if (added.isEmpty())
        return;
    file->apply(tr("Add sprites"), edited);
    m_page->setCurrent(added.last());
}

bool SpritesEditor::performDialog(const QString& id)
{
    SpriteFile* file = m_page->file();
    const SpriteDocument& doc = file->document();
    const int current = m_page->current();
    const int count = doc.spriteCount();
    const int currentGroup = current < count ? doc.sprites()[current].group() : 0;
    QWidget* parent = m_page->window();
    Config& config = m_context.config();
    const QString imageFolder = config.value(QStringLiteral("spr_path"), QFileInfo(file->path()).absolutePath());

    if (id == QLatin1String("spr_crop")) {
        RangeDialog range(currentGroup, parent);
        range.setWindowIcon(m_context.actions().action(id)->icon());
        if (range.exec() != QDialog::Accepted)
            return true;
        SpriteDocument edited = doc;
        SpriteEdits::crop(edited, range.sprites(doc, current));
        if (!samePixels(edited, doc)) // a crop that changes nothing is no step
            file->apply(tr("Crop image"), edited);
    } else if (id == QLatin1String("spr_resize")) {
        ResizeDialog dialog(currentGroup, parent);
        if (dialog.exec() != QDialog::Accepted)
            return true;
        const QList<int> sprites = dialog.sprites(doc, current);
        SpriteDocument edited = doc;
        SpriteEdits::resize(edited, sprites, dialog.widthScale(), dialog.heightScale(), dialog.filter());
        if (samePixels(edited, doc))
            return true;
        file->apply(tr("Resize"), edited);
        // The frames showing these sprites can have their collision boxes scaled too.
        auto* project = qobject_cast<Project*>(file->parent());
        if (project && project->engine() && project->engine()->hasContactBoxes() &&
            QMessageBox::question(parent, tr("Scale contact boxes"),
                                  tr("Do you want to scale contact boxes of frames that use these sprites?"),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
            QList<QPair<int, int>> identifiers;
            for (int index : sprites)
                identifiers << qMakePair(doc.sprites()[index].group(), doc.sprites()[index].number());
            project->engine()->scaleContactBoxes(*project, identifiers, dialog.widthScale(), dialog.heightScale());
        }
    } else if (id == QLatin1String("spr_delete_grp") || id == QLatin1String("spr_rename_grp")) {
        const bool rename = id == QLatin1String("spr_rename_grp");
        GroupDialog dialog(rename ? GroupDialog::Kind::Rename : GroupDialog::Kind::Delete, currentGroup, parent);
        if (dialog.exec() != QDialog::Accepted)
            return true;
        SpriteDocument edited = doc;
        const int changed =
            rename ? SpriteEdits::renameGroup(edited, dialog.group(), dialog.newGroup(), dialog.first(), dialog.last())
                   : SpriteEdits::removeGroup(edited, dialog.group(), dialog.first(), dialog.last());
        if (changed > 0)
            file->apply(rename ? tr("Rename group") : tr("Delete group"), edited);
    } else if (id == QLatin1String("spr_align")) {
        AlignmentDialog dialog(doc, current, parent);
        connect(&dialog, &AlignmentDialog::applyRequested, this, [file](const QList<int>& sprites, int dx, int dy) {
            if (sprites.isEmpty() || (dx == 0 && dy == 0))
                return;
            SpriteDocument edited = file->document();
            SpriteEdits::moveAxes(edited, sprites, dx, dy);
            file->apply(tr("Alignment tool"), edited);
        });
        dialog.exec();
    } else if (id == QLatin1String("spr_openimg")) {
        if (current >= count)
            return true;
        const QString path =
            FileDialogs::getOpenFileName(parent, tr("Change image..."), imageFolder, FileFilters::openImages());
        if (path.isEmpty())
            return true;
        config.setValue(QStringLiteral("spr_path"), QFileInfo(path).absolutePath());
        QString error;
        Formats::Image image = loadImageFile(path, &error);
        if (image.pixelBuffer().empty()) {
            QMessageBox::warning(parent, tr("Change image..."), error);
            return true;
        }
        // Questions in order: Crop image (Options > Messages > Crop new sprites), the Palette dialog for an indexed
        // image, then Sprite changes for a linked sprite.
        QPoint shift;
        if (askQuestion(parent, config, QStringLiteral("showmsg_crop_spr"), tr("Crop image"),
                        tr("Do you want to crop the image?"))) {
            Formats::Rect bounds;
            Formats::Image cropped = Formats::cropToContent(image, &bounds);
            if (!cropped.pixelBuffer().empty()) {
                image = std::move(cropped);
                shift = QPoint(-bounds.x, -bounds.y);
            }
        }
        CommitSpriteDialog palette(CommitSpriteDialog::captions(doc), parent);
        int result = CommitSpriteDialog::Ignored;
        if (image.isIndexed()) {
            result = palette.exec();
            if (result != QDialog::Accepted && result != CommitSpriteDialog::Ignored)
                return true;
        }
        const bool allInstances = shareImageChanges(current, parent);
        SpriteDocument edited = doc;
        const Formats::Palette colors = image.palette();
        palette.commit(edited, current, std::move(image), colors, result, allInstances);
        if (!shift.isNull()) {
            for (int i : allInstances ? edited.linkedSprites(current) : QList<int>{current}) {
                Formats::SffSprite& header = edited.sprites()[i].header;
                header.axisX = std::int16_t(header.axisX + shift.x());
                header.axisY = std::int16_t(header.axisY + shift.y());
            }
        }
        file->apply(tr("Change image"), edited);
    } else if (id == QLatin1String("spr_saveimgas")) {
        const QString path =
            FileDialogs::getSaveFileName(parent, tr("Save image..."), imageFolder, FileFilters::saveImages());
        if (path.isEmpty())
            return true;
        ItemsDialog items(doc, current, parent);
        if (items.exec() != QDialog::Accepted || items.sprites().isEmpty())
            return true;
        config.setValue(QStringLiteral("spr_path"), QFileInfo(path).absolutePath());
        const QString suffix = QFileInfo(path).suffix().isEmpty() ? QStringLiteral("png") : QFileInfo(path).suffix();
        QString error;
        if (saveSpriteImages(doc, items.sprites(), path, suffix, &error).size() != items.sprites().size())
            QMessageBox::warning(parent, tr("Save image..."), error);
    } else if (id == QLatin1String("spr_saveimg_aligned")) {
        RangeDialog range(currentGroup, parent);
        range.setWindowIcon(m_context.actions().action(id)->icon());
        if (range.exec() != QDialog::Accepted)
            return true;
        const QList<int> sprites = range.sprites(doc, current);
        if (sprites.isEmpty())
            return true;
        const QString path =
            FileDialogs::getSaveFileName(parent, tr("Save image (aligned)..."), imageFolder, FileFilters::saveImages());
        if (path.isEmpty())
            return true;
        config.setValue(QStringLiteral("spr_path"), QFileInfo(path).absolutePath());
        const QString suffix = QFileInfo(path).suffix().isEmpty() ? QStringLiteral("png") : QFileInfo(path).suffix();
        const AlignedImages aligned = alignSprites(doc, sprites);
        QString error;
        QProgressDialog progress(tr("Saving images..."), tr("Cancel"), 0, int(sprites.size()), parent);
        progress.setWindowTitle(tr("Saving images..."));
        progress.setWindowModality(Qt::WindowModal);
        progress.setMinimumDuration(500);
        const auto step = [&progress](int written) {
            progress.setValue(written);
            return !progress.wasCanceled();
        };
        const qsizetype saved = saveAlignedImages(doc, sprites, aligned, path, suffix, &error, step).size();
        progress.reset();
        if (progress.wasCanceled())
            return true;
        if (saved != sprites.size()) {
            QMessageBox::warning(parent, tr("Save image (aligned)..."), error);
            return true;
        }
        if (QMessageBox::question(parent, tr("Perfect axis"),
                                  tr("The axis is X: %1, Y: %2. Do you want to save this information to a file?")
                                      .arg(aligned.axis.x())
                                      .arg(aligned.axis.y()),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
            QFile axis(QFileInfo(path).dir().filePath(QStringLiteral("axis.txt")));
            if (axis.open(QIODevice::WriteOnly))
                axis.write(axisFileText(aligned.axis));
        }
    } else if (id == QLatin1String("spr_add")) {
        addSprites(parent, imageFolder);
    } else if (id == QLatin1String("spr_export")) {
        SpriteExportDialog dialog(parent);
        if (dialog.exec() != QDialog::Accepted)
            return true;
        QList<int> sprites;
        for (int i = 0; i < count; ++i) {
            if (dialog.group() < 0 || doc.sprites()[i].group() == dialog.group())
                sprites << i;
        }
        QString path = FileDialogs::getSaveFileName(parent, tr("Export..."), imageFolder,
                                                    FileFilters::fighterFactoryExport() + QStringLiteral(";;") +
                                                        tr("Elecbyte SprMaker 2 (*.def)"));
        if (path.isEmpty())
            return true;
        if (QFileInfo(path).suffix().isEmpty())
            path += QStringLiteral(".ffe");
        config.setValue(QStringLiteral("spr_path"), QFileInfo(path).absolutePath());
        const SpriteListFormat format = QFileInfo(path).suffix().compare(QLatin1String("def"), Qt::CaseInsensitive) == 0
                                            ? SpriteListFormat::SprMaker2
                                            : SpriteListFormat::Ffe;
        QString error;
        if (!exportSprites(doc, sprites, path, format, &error))
            QMessageBox::warning(parent, tr("Export..."), error);
    } else if (id == QLatin1String("spr_import")) {
        const QString path =
            FileDialogs::getOpenFileName(parent, tr("Import..."), imageFolder, FileFilters::fighterFactoryExport());
        if (path.isEmpty())
            return true;
        config.setValue(QStringLiteral("spr_path"), QFileInfo(path).absolutePath());
        AddSpritesOptions options;
        options.askConflict = conflictPrompt(parent);
        SpriteDocument edited = doc;
        QString error;
        const QList<int> added = importFfe(edited, current, path, options, &error);
        if (!error.isEmpty()) {
            QMessageBox::warning(parent, tr("Import..."), error);
            return true;
        }
        if (!added.isEmpty()) {
            file->apply(tr("Import"), edited);
            m_page->setCurrent(added.last());
        }
    } else if (id == QLatin1String("spr_split_channels")) {
        if (current >= count)
            return true;
        SpriteDocument edited = doc;
        SpriteEdits::splitChannels(edited, current);
        file->apply(tr("Split channels"), edited);
    } else {
        return false;
    }
    return true;
}
void SpritesEditor::importFromProject(const QList<QPair<QString, const SpriteDocument*>>& sources)
{
    if (!m_page)
        return;
    QWidget* parent = m_page->window();
    QList<ProjectImportDialog::Source> projects;
    for (const auto& source : sources)
        projects << ProjectImportDialog::Source{source.first, source.second};
    SpriteFile* file = m_page->file();
    ProjectImportDialog dialog(projects, file->document(), parent);
    if (dialog.exec() != QDialog::Accepted || dialog.project() < 0 || dialog.project() >= projects.size())
        return;
    const QList<ProjectImportItem> items = dialog.items();
    if (items.isEmpty())
        return;
    ProjectImportOptions options = dialog.options();
    options.askReplace = [parent](int group, int number) {
        return QMessageBox::question(
                   parent, tr("Conflict resolution!"),
                   tr("Sprite '%1,%2' already exists, do you want to replace it?").arg(group).arg(number),
                   QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
    };
    SpriteDocument edited = file->document();
    const ProjectImportResult result =
        OFS::importFromProject(edited, m_page->current(), *projects[dialog.project()].sprites, items, options);
    if (result.added.isEmpty() && result.replaced == 0)
        return;
    file->apply(tr("Import from another project"), edited);
    if (!result.added.isEmpty())
        m_page->setCurrent(result.added.first());
    QMessageBox::information(parent, tr("Import from another project..."),
                             tr("%1 new sprite(s) were imported!").arg(result.added.size()));
}
namespace {

const QString SpriteMimeType = QStringLiteral("application/x-openfactorystudio-sprite");
struct CopiedSprite
{
    Formats::Image image;
    int group = 0;
    int number = 0;
    QPoint axis;
    QString name;
};
CopiedSprite& copiedSprite()
{
    static CopiedSprite sprite;
    return sprite;
}

} // namespace

void SpritesEditor::copySprite()
{
    if (!m_page)
        return;
    const SpriteDocument& doc = m_page->file()->document();
    const int current = m_page->current();
    if (current < 0 || current >= doc.spriteCount())
        return;
    const SpriteEntry& entry = doc.sprites()[current];
    CopiedSprite& copied = copiedSprite();
    copied.image = doc.image(current);
    copied.group = entry.group();
    copied.number = entry.number();
    copied.axis = QPoint(entry.header.axisX, entry.header.axisY);
    copied.name = entry.name;
    const Formats::Image rgba = doc.imageRgba(current);
    auto* data = new QMimeData;
    if (!rgba.pixelBuffer().empty())
        data->setImageData(toQImage(rgba));
    data->setData(SpriteMimeType, QByteArray("1"));
    QApplication::clipboard()->setMimeData(data);
}

void SpritesEditor::cutSprite()
{
    if (!m_page || m_page->current() >= m_page->file()->document().spriteCount())
        return;
    copySprite();
    perform(QStringLiteral("spr_delete"));
}

void SpritesEditor::pasteSprite()
{
    if (!m_page)
        return;
    SpriteFile* file = m_page->file();
    const SpriteDocument& doc = file->document();
    const int current = m_page->current();
    const bool has = current >= 0 && current < doc.spriteCount();
    const QMimeData* data = QApplication::clipboard()->mimeData();
    CopiedSprite sprite;
    if (data && data->hasFormat(SpriteMimeType) && !copiedSprite().image.pixelBuffer().empty()) {
        sprite = copiedSprite();
    } else if (data && data->hasImage()) {
        const QImage image = qvariant_cast<QImage>(data->imageData());
        if (image.isNull())
            return;
        const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
        std::vector<std::uint8_t> pixels(std::size_t(rgba.width()) * std::size_t(rgba.height()) * 4);
        for (int y = 0; y < rgba.height(); ++y)
            std::memcpy(pixels.data() + std::size_t(y) * std::size_t(rgba.width()) * 4, rgba.constScanLine(y),
                        std::size_t(rgba.width()) * 4);
        sprite.image = Formats::Image::rgba(rgba.width(), rgba.height(), std::move(pixels));
        sprite.group = has ? doc.sprites()[current].group() : 0;
        sprite.number = SpriteEdits::freeNumber(doc, sprite.group);
        if (has)
            sprite.axis = QPoint(doc.sprites()[current].header.axisX, doc.sprites()[current].header.axisY);
    } else {
        return;
    }
    SpriteDocument edited = doc;
    const int position = has ? current + 1 : edited.spriteCount();
    SpriteEntry entry;
    entry.header.group = std::uint16_t(std::clamp(sprite.group, 0, 0xFFFF));
    entry.header.number = std::uint16_t(std::clamp(sprite.number, 0, 0xFFFF));
    entry.header.axisX = std::int16_t(std::clamp(sprite.axis.x(), -32768, 32767));
    entry.header.axisY = std::int16_t(std::clamp(sprite.axis.y(), -32768, 32767));
    entry.name = sprite.name;
    edited.sprites().insert(position, entry);
    SpriteEdits::setImage(edited, position, std::move(sprite.image));
    file->apply(tr("Paste"), edited);
    m_page->setCurrent(position);
}

} // namespace OFS
