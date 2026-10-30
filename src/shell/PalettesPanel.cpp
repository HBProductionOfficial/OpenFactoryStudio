#include "shell/PalettesPanel.h"

#include "core/actions/ActionRegistry.h"
#include "core/app/AppContext.h"
#include "core/app/Messages.h"
#include "core/project/Project.h"
#include "core/settings/Config.h"
#include "core/sprites/ImageDocument.h"
#include "core/sprites/PaletteEdits.h"
#include "core/sprites/SpriteFile.h"
#include "core/sprites/SpriteImages.h"
#include "core/widgets/PaletteWidget.h"
#include "shell/FileDialogs.h"
#include "shell/FileFilters.h"
#include "shell/ImageEditor.h"
#include "shell/SpritePage.h"
#include "shell/dialogs/SoundDialogs.h"
#include "shell/dialogs/SpriteDialogs.h"

#include <QAction>
#include <QCheckBox>
#include <QColorDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QScrollBar>
#include <QSpinBox>
#include <QUndoStack>

namespace OFS {

using namespace Formats;

namespace {

template <typename T>
T* control(QWidget* panel, const char* name)
{
    return panel ? panel->findChild<T*>(QLatin1String(name)) : nullptr;
}

QStringList paletteCaptions(const SpriteDocument& doc)
{
    QStringList captions;
    for (const SpritePalettePtr& palette : doc.palettes())
        captions << QStringLiteral("%1, %2").arg(palette->palette.group).arg(palette->palette.number);
    return captions;
}

// Qt's own colour dialog with the alpha channel.
std::optional<Rgba> askColor(QWidget* parent, Rgba initial)
{
    QColorDialog dialog(QColor(initial.r, initial.g, initial.b, initial.a), parent);
    dialog.setWindowTitle(QColorDialog::tr("Color"));
    dialog.setOptions(QColorDialog::ShowAlphaChannel | QColorDialog::DontUseNativeDialog);
    if (dialog.exec() != QDialog::Accepted)
        return std::nullopt;
    const QColor c = dialog.selectedColor();
    return Rgba{std::uint8_t(c.red()), std::uint8_t(c.green()), std::uint8_t(c.blue()), std::uint8_t(c.alpha())};
}

} // namespace

PalettesPanel::PalettesPanel(AppContext& context, QObject* parent)
    : QObject(parent)
    , m_context(context)
{}

void PalettesPanel::bindPanel(QWidget* panel)
{
    m_selector = control<QScrollBar>(panel, "pal_sel");
    m_selectorLabel = control<QLabel>(panel, "pal_sel_lb");
    m_switch = control<QCheckBox>(panel, "spr_switch_pal");
    m_group = control<QSpinBox>(panel, "pal_grp");
    m_number = control<QSpinBox>(panel, "pal_num");
    m_grid = control<PaletteWidget>(panel, "pal_view");
    m_pickerButton = control<QWidget>(panel, "pal_pick_color");
    if (m_selector)
        connect(m_selector, &QScrollBar::valueChanged, this, [this](int value) {
            if (!m_updating && m_page)
                m_page->setCurrentPalette(value);
        });
    if (m_switch) {
        m_switch->setChecked(m_context.config().boolValue(QStringLiteral("switch_to_imgpal"), true));
        connect(m_switch, &QCheckBox::toggled, this, [this](bool on) {
            m_context.config().setValue(QStringLiteral("switch_to_imgpal"), on);
            if (on)
                followSprite();
            updateDisplay();
        });
    }
    for (QSpinBox* box : {m_group.data(), m_number.data()}) {
        if (!box)
            continue;
        box->setKeyboardTracking(false);
        connect(box, &QSpinBox::valueChanged, this, &PalettesPanel::applyIdentifiers);
    }
    if (m_grid) {
        connect(m_grid, &PaletteWidget::slotDoubleClicked, this, &PalettesPanel::editSlot);
        connect(m_grid, &PaletteWidget::slotPressed, this, &PalettesPanel::slotPressed);
        connect(m_grid, &PaletteWidget::slotMoved, this, [this](int from, int to) {
            const int index = current();
            if (!m_page || index < 0)
                return;
            SpriteDocument doc = m_page->file()->document();
            PaletteEdits::moveColor(doc, index, from, to);
            apply(tr("Move color"), doc, true);
        });
    }
    setImageMode(m_imageMode);
    refresh();
}

void PalettesPanel::setPage(SpritePage* page)
{
    if (m_page == page)
        return;
    if (m_page) {
        disconnect(m_page, nullptr, this, nullptr);
        disconnect(m_page->file(), nullptr, this, nullptr);
        disconnect(m_page->file()->undoStack(), nullptr, this, nullptr);
        m_page->setColorPicker(false);
        m_page->setDisplayPalette(nullptr);
    }
    m_page = page;
    if (m_page) {
        connect(m_page, &SpritePage::currentPaletteChanged, this, [this] {
            refresh();
            updateDisplay();
        });
        connect(m_page, &SpritePage::currentChanged, this, [this] {
            followSprite();
            updateDisplay();
        });
        connect(m_page, &SpritePage::colorPicked, this, [this](int slot) {
            if (m_grid)
                m_grid->setSelection(m_grid->selection() + QList<int>{slot});
        });
        connect(m_page->file(), &SpriteFile::documentChanged, this, [this] {
            refresh();
            updateDisplay();
            Q_EMIT stateChanged();
        });
        // Undo and redo clear the slot selection.
        connect(m_page->file()->undoStack(), &QUndoStack::indexChanged, this, [this] {
            if (!m_applying && m_grid)
                m_grid->clearSelection();
        });
        if (QAction* picker = m_context.actions().action(QStringLiteral("pal_pick_color")))
            m_page->setColorPicker(picker->isChecked() && !m_imageMode);
        followSprite();
    }
    refresh();
    updateDisplay();
    Q_EMIT stateChanged();
}

void PalettesPanel::setImageMode(bool on)
{
    m_imageMode = on;
    for (QWidget* w : {m_pickerButton.data(), static_cast<QWidget*>(m_switch.data())}) {
        if (w)
            w->setVisible(!on);
    }
    if (m_page) {
        QAction* picker = m_context.actions().action(QStringLiteral("pal_pick_color"));
        m_page->setColorPicker(!on && picker && picker->isChecked());
        // Entering the image editor loads the sprite again, so the switch picks its palette.
        if (on) {
            followSprite();
            updateDisplay();
        }
    }
}

int PalettesPanel::current() const
{
    return m_page ? m_page->currentPalette() : -1;
}

QList<int> PalettesPanel::selection() const
{
    return m_grid ? m_grid->selection() : QList<int>();
}

void PalettesPanel::refresh()
{
    const bool updating = std::exchange(m_updating, true);
    const SpriteDocument* doc = m_page ? &m_page->file()->document() : nullptr;
    const int count = doc ? int(doc->palettes().size()) : 0;
    const int index = current();
    const bool has = index >= 0 && index < count;
    if (m_selector) {
        m_selector->setRange(count > 0 ? 0 : -1, count - 1);
        m_selector->setValue(has ? index : -1);
    }
    if (m_selectorLabel)
        m_selectorLabel->setText(QStringLiteral("%1 / %2").arg(has ? index + 1 : 0).arg(count));
    for (QSpinBox* box : {m_group.data(), m_number.data()}) {
        if (box)
            box->setEnabled(has);
    }
    if (has) {
        const SffPalette& palette = doc->palettes()[index]->palette;
        if (m_group)
            m_group->setValue(palette.group);
        if (m_number)
            m_number->setValue(palette.number);
    }
    if (m_grid)
        m_grid->setColors(has ? doc->palettes()[index]->palette.colors : Palette());
    m_updating = updating;
}

void PalettesPanel::followSprite()
{
    if (!m_page || (m_switch && !m_switch->isChecked()))
        return;
    // Only for 8-bit sprites. True-color sprites and sprites without a palette keep the panel's.
    const SpriteDocument& doc = m_page->file()->document();
    const int sprite = m_page->current();
    if (sprite < 0 || sprite >= doc.spriteCount() || !PaletteEdits::isIndexed(doc.sprites()[sprite]))
        return;
    const int palette = PaletteEdits::paletteOf(doc, sprite);
    if (palette >= 0)
        m_page->setCurrentPalette(palette);
}

void PalettesPanel::updateDisplay()
{
    if (!m_page)
        return;
    const SpriteDocument& doc = m_page->file()->document();
    const int index = current();
    // Same as Fighter Factory: 8-bit sprites are drawn with the palette chosen in the panel, in both modes. With the
    // switch on that is the sprite's own palette until another one is chosen. A Remap row whose source is that palette
    // draws with its destination instead.
    SpritePalettePtr display = index >= 0 && index < doc.palettes().size() ? doc.palettes()[index] : nullptr;
    if (display) {
        const QList<RemapRow> rows = m_remap ? m_remap->rows() : m_remapRows;
        for (const RemapRow& row : rows) {
            if (display->palette.group == row.sourceGroup && display->palette.number == row.sourceNumber) {
                if (const int target = doc.findPalette(row.targetGroup, row.targetNumber); target >= 0)
                    display = doc.palettes()[target];
                break;
            }
        }
    }
    m_page->setDisplayPalette(display);
}

void PalettesPanel::apply(const QString& text, const SpriteDocument& doc, bool colorEdit)
{
    const bool applying = std::exchange(m_applying, true);
    if (colorEdit)
        m_page->file()->applyColorEdit(text, doc);
    else
        m_page->file()->apply(text, doc);
    m_applying = applying;
}

void PalettesPanel::applyIdentifiers()
{
    const int index = current();
    if (m_updating || !m_page || !m_group || !m_number || index < 0)
        return;
    const SpriteDocument& doc = m_page->file()->document();
    const SffPalette& palette = doc.palettes()[index]->palette;
    if (palette.group == m_group->value() && palette.number == m_number->value())
        return;
    SpriteDocument edited = doc;
    PaletteEdits::setIdentifiers(edited, index, m_group->value(), m_number->value());
    apply(tr("Palette group and index"), edited);
}

void PalettesPanel::editSlot(int slot)
{
    const int index = current();
    if (!m_page || index < 0)
        return;
    const Palette& colors = m_page->file()->document().palettes()[index]->palette.colors;
    if (slot < 0 || slot >= int(colors.size()))
        return;
    const std::optional<Rgba> color = askColor(dialogParent(), colors[std::size_t(slot)]);
    if (!color)
        return;
    SpriteDocument doc = m_page->file()->document();
    PaletteEdits::fill(doc, index, {slot}, *color);
    apply(tr("Palette color"), doc, true);
}

void PalettesPanel::slotPressed(int slot, Qt::MouseButton button)
{
    // Image editor mode: left click picks the foreground color and right click the background. With the Remap pen they
    // pick the current slot's source and replacement.
    if (!m_imageMode || !m_page || !m_grid)
        return;
    const Palette& colors = m_grid->colors();
    const Rgba rgba = slot < int(colors.size()) ? colors[std::size_t(slot)] : Rgba{0, 0, 0, 255};
    const PaintColor color{slot, Rgba{rgba.r, rgba.g, rgba.b, std::uint8_t(slot == 0 ? 0 : 255)}};
    ImageEditor* editor = m_page->imageEditor();
    if (button != Qt::LeftButton && button != Qt::RightButton)
        return;
    if (editor->tool() == ImageEditor::Tool::RemapPen)
        editor->setSlotColor(button == Qt::RightButton, color);
    else if (button == Qt::LeftButton)
        editor->setForeground(color);
    else
        editor->setBackground(color);
}

QWidget* PalettesPanel::dialogParent() const
{
    return m_page ? m_page->window() : nullptr;
}

QString PalettesPanel::projectFolder() const
{
    // The palette dialogs start in the project folder every time.
    auto* project = m_page ? qobject_cast<Project*>(m_page->file()->parent()) : nullptr;
    return project ? project->directory() : QFileInfo(m_page ? m_page->file()->path() : QString()).absolutePath();
}

const QStringList& PalettesPanel::actionIds()
{
    static const QStringList ids = {QStringLiteral("pal_clear"),
                                    QStringLiteral("pal_clear_unused"),
                                    QStringLiteral("pal_add"),
                                    QStringLiteral("pal_delete"),
                                    QStringLiteral("pal_delete_duplicates"),
                                    QStringLiteral("pal_open"),
                                    QStringLiteral("pal_save"),
                                    QStringLiteral("pal_remap"),
                                    QStringLiteral("pal_undo"),
                                    QStringLiteral("pal_redo"),
                                    QStringLiteral("pal_switch_color"),
                                    QStringLiteral("pal_set_color"),
                                    QStringLiteral("pal_apply_sprite"),
                                    QStringLiteral("pal_prev"),
                                    QStringLiteral("pal_next"),
                                    QStringLiteral("pal_switch_count"),
                                    QStringLiteral("pal_prev_grp"),
                                    QStringLiteral("pal_next_grp"),
                                    QStringLiteral("pal_pick_color")};
    return ids;
}

bool PalettesPanel::perform(const QString& id)
{
    if (!m_page || !id.startsWith(QLatin1String("pal_")))
        return false;
    SpriteFile* file = m_page->file();
    const SpriteDocument& doc = file->document();
    const int count = int(doc.palettes().size());
    const int index = current();
    const bool has = index >= 0 && index < count;
    QWidget* parent = dialogParent();
    if (id == QLatin1String("pal_prev")) {
        if (index > 0)
            m_page->setCurrentPalette(index - 1);
    } else if (id == QLatin1String("pal_next")) {
        if (has && index + 1 < count)
            m_page->setCurrentPalette(index + 1);
    } else if (id == QLatin1String("pal_prev_grp") || id == QLatin1String("pal_next_grp")) {
        if (!has)
            return true;
        auto group = [&doc](int i) {
            return doc.palettes()[i]->palette.group;
        };
        int i = index;
        if (id == QLatin1String("pal_next_grp")) {
            while (i < count && group(i) == group(index))
                ++i;
            if (i < count)
                m_page->setCurrentPalette(i);
        } else {
            while (i > 0 && group(i - 1) == group(index))
                --i;
            if (i > 0) {
                int start = i - 1;
                while (start > 0 && group(start - 1) == group(i - 1))
                    --start;
                m_page->setCurrentPalette(start);
            }
        }
    } else if (id == QLatin1String("pal_add")) {
        addPalettes();
    } else if (id == QLatin1String("pal_delete")) {
        deletePalette();
    } else if (id == QLatin1String("pal_delete_duplicates")) {
        SpriteDocument edited = doc;
        if (PaletteEdits::removeDuplicates(edited) > 0)
            apply(tr("Remove all duplicates"), edited);
    } else if (id == QLatin1String("pal_clear_unused")) {
        SpriteDocument edited = doc;
        if (PaletteEdits::removeUnused(edited) > 0)
            apply(tr("Clear all unused"), edited);
    } else if (id == QLatin1String("pal_clear")) {
        if (count == 0)
            return true;
        if (m_context.config().boolValue(QStringLiteral("confirm_delete"), true) &&
            QMessageBox::question(parent, tr("Clear"), tr("Do you really want to remove all palettes?"),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) != QMessageBox::Yes)
            return true;
        SpriteDocument edited = doc;
        PaletteEdits::clear(edited);
        apply(tr("Clear palettes"), edited);
    } else if (id == QLatin1String("pal_open")) {
        if (!has)
            return true;
        const QString path =
            FileDialogs::getOpenFileName(parent, tr("Open palette"), projectFolder(), paletteFileFilters());
        if (path.isEmpty())
            return true;
        auto colors = PaletteEdits::readFile(path);
        if (!colors) {
            QMessageBox::warning(parent, tr("Change..."), QString::fromStdString(colors.error().message));
            return true;
        }
        SpriteDocument edited = doc;
        PaletteEdits::setColors(edited, index, std::move(colors.value()));
        apply(tr("Change palette"), edited);
    } else if (id == QLatin1String("pal_save")) {
        if (count == 0)
            return true;
        ItemsDialog items(paletteCaptions(doc), index, parent);
        if (items.exec() != QDialog::Accepted || items.selection().isEmpty())
            return true;
        const QString path =
            FileDialogs::getSaveFileName(parent, tr("Save palette"), projectFolder(), FileFilters::adobeColorTable());
        if (path.isEmpty())
            return true;
        QString error;
        if (PaletteEdits::saveAct(doc, items.selection(), path, &error).size() != items.selection().size())
            QMessageBox::warning(parent, tr("Save as..."), error);
    } else if (id == QLatin1String("pal_switch_count")) {
        if (!has)
            return true;
        SpriteDocument edited = doc;
        PaletteEdits::switchCount(edited, index);
        apply(tr("Switch color count"), edited);
    } else if (id == QLatin1String("pal_switch_color")) {
        if (!has || selection().size() < 2)
            return true;
        SpriteDocument edited = doc;
        PaletteEdits::exchange(edited, index, selection());
        const QList<int> kept = selection();
        apply(tr("Exchange colors"), edited, true);
        if (m_grid)
            m_grid->setSelection(kept);
    } else if (id == QLatin1String("pal_set_color")) {
        const QList<int> slots = selection();
        if (!has || slots.isEmpty())
            return true;
        const Palette& colors = doc.palettes()[index]->palette.colors;
        const std::optional<Rgba> color = askColor(parent, colors[std::size_t(slots.first())]);
        if (!color)
            return true;
        SpriteDocument edited = file->document();
        PaletteEdits::fill(edited, index, slots, *color);
        apply(tr("Palette color"), edited, true);
        if (m_grid)
            m_grid->setSelection(slots);
    } else if (id == QLatin1String("pal_pick_color")) {
        QAction* picker = m_context.actions().action(QStringLiteral("pal_pick_color"));
        m_page->setColorPicker(picker && picker->isChecked() && !m_imageMode);
    } else if (id == QLatin1String("pal_remap")) {
        if (!m_remap) {
            m_remap = new RemapDialog(parent);
            m_remap->setRows(m_remapRows);
            connect(m_remap, &RemapDialog::rowsChanged, this, &PalettesPanel::updateDisplay);
        }
        m_remap->show();
        m_remap->raise();
        m_remap->activateWindow();
    } else if (id == QLatin1String("pal_apply_sprite")) {
        if (!has || doc.spriteCount() == 0)
            return true;
        ItemsDialog items(doc, m_page->current(), parent);
        if (items.exec() != QDialog::Accepted || items.sprites().isEmpty())
            return true;
        SpriteDocument edited = doc;
        PaletteEdits::applyToSprites(edited, index, items.sprites());
        apply(tr("Apply to the sprites"), edited);
    } else if (id == QLatin1String("pal_undo")) {
        file->undoColorEdit();
    } else if (id == QLatin1String("pal_redo")) {
        file->redoColorEdit();
    } else {
        return false;
    }
    Q_EMIT stateChanged();
    return true;
}

void PalettesPanel::addPalettes()
{
    SpriteFile* file = m_page->file();
    const SpriteDocument& doc = file->document();
    QWidget* parent = dialogParent();
    const int index = current();
    SpriteDocument edited = doc;
    if (!askQuestion(parent, m_context.config(), QStringLiteral("showmsg_pal_file"), tr("Add palette"),
                     tr("Do you want to load palettes from files?"))) {
        // New palette 0,0 of 32 transparent colors after the current one.
        const int added = PaletteEdits::insert(edited, index, 0, 0, PaletteEdits::emptyColors());
        apply(tr("Add palette"), edited);
        m_page->setCurrentPalette(added);
        return;
    }
    QStringList files =
        FileDialogs::getOpenFileNames(parent, tr("Add palettes"), projectFolder(), paletteFileFilters());
    if (files.isEmpty())
        return;
    files.sort();
    bool detect = false;
    bool reversed = false;
    int group = 0;
    int number = 0;
    int step = 1;
    if (m_context.config().boolValue(QStringLiteral("show_addpalette_dialog"), true)) {
        AddItemsDialog dialog(AddItemsDialog::Kind::Palettes, parent);
        if (dialog.exec() != QDialog::Accepted)
            return;
        detect = dialog.detect();
        reversed = dialog.reversed();
        group = dialog.group();
        number = dialog.number();
        step = dialog.step();
    }
    QStringList errors;
    int after = index;
    for (const QString& path : std::as_const(files)) {
        auto colors = PaletteEdits::readFile(path, reversed);
        if (!colors) {
            errors << QStringLiteral("%1: %2").arg(QFileInfo(path).fileName(),
                                                   QString::fromStdString(colors.error().message));
            continue;
        }
        int g = group;
        int i = number;
        if (!detect || !identifiersFromFileName(path, &g, &i)) {
            g = group;
            i = number;
            number += step;
        }
        after = PaletteEdits::insert(edited, after, g, i, std::move(colors.value()));
    }
    if (!errors.isEmpty())
        QMessageBox::warning(parent, tr("Add"), errors.join(QLatin1Char('\n')));
    if (after == index)
        return;
    apply(tr("Add palettes"), edited);
    m_page->setCurrentPalette(after);
}

void PalettesPanel::deletePalette()
{
    SpriteFile* file = m_page->file();
    const SpriteDocument& doc = file->document();
    const int count = int(doc.palettes().size());
    const int index = current();
    if (index < 0 || index >= count)
        return;
    QWidget* parent = dialogParent();
    const int replacement = index + 1 < count ? index + 1 : index - 1;
    const int used = int(PaletteEdits::users(doc, index).size());
    if (used > 0) {
        const SffPalette& own = doc.palettes()[index]->palette;
        QString text = tr("The palette %1, %2 is used by %n sprite(s).", nullptr, used).arg(own.group).arg(own.number);
        if (replacement >= 0) {
            const SffPalette& other = doc.palettes()[replacement]->palette;
            text += QLatin1Char('\n') + tr("Do you want to delete it? Its sprites will use the palette %1, %2.")
                                            .arg(other.group)
                                            .arg(other.number);
        } else {
            text += QLatin1Char('\n') + tr("Do you want to delete it? Its sprites will have no palette.");
        }
        if (QMessageBox::question(parent, tr("Delete"), text, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) !=
            QMessageBox::Yes)
            return;
    } else if (!confirmDelete(parent, m_context.config(), tr("Delete"),
                              tr("Do you really want to delete the palette?"))) {
        return;
    }
    SpriteDocument edited = doc;
    PaletteEdits::remove(edited, index, replacement);
    apply(tr("Delete palette"), edited);
    m_page->setCurrentPalette(std::min(index, count - 2));
}

} // namespace OFS
