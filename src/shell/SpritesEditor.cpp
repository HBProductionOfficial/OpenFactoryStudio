#include "shell/SpritesEditor.h"

#include "core/app/AppContext.h"
#include "core/settings/Config.h"
#include "core/sprites/SpriteEdits.h"
#include "core/sprites/SpriteFile.h"
#include "shell/SpritePage.h"

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QScrollBar>
#include <QSpinBox>

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
{}

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

void SpritesEditor::setPage(SpritePage* page)
{
    if (m_page == page)
        return;
    if (m_page)
        disconnect(m_page, nullptr, this, nullptr);
    m_page = page;
    if (m_page) {
        connect(m_page, &SpritePage::currentChanged, this, &SpritesEditor::refresh);
        connect(m_page->file(), &SpriteFile::documentChanged, this, &SpritesEditor::refresh);
        if (m_showTransparent)
            m_page->setShowTransparent(m_showTransparent->isChecked());
    }
    refresh();
}

void SpritesEditor::refresh()
{
    const bool updating = std::exchange(m_updating, true);
    const SpriteDocument* doc = m_page ? &m_page->file()->document() : nullptr;
    const int count = doc ? doc->spriteCount() : 0;
    const int current = m_page ? m_page->current() : 0;
    const bool has = count > 0 && current >= 0 && current < count;
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
        if (m_axisX)
            m_axisX->setValue(entry.header.axisX);
        if (m_axisY)
            m_axisY->setValue(entry.header.axisY);
        if (m_decompress) {
            m_decompress->setEnabled(doc->version() != Formats::SffVersion::V1);
            m_decompress->setChecked(SpriteEdits::decompressesOnLoad(*doc, current));
        }
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
    SpriteEdits::setAxis(doc, index, m_axisX->value(), m_axisY->value());
    m_page->file()->applyAxisChange(index, doc);
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
        if (m_context.config().boolValue(QStringLiteral("confirm_delete"), true)) {
            QMessageBox box(QMessageBox::Question, tr("Delete"), tr("Do you really want to delete the sprite?"),
                            QMessageBox::Yes | QMessageBox::No, m_page);
            box.setDefaultButton(QMessageBox::Yes);
            auto* dontAsk = new QCheckBox(tr("Don't show this again"), &box);
            box.setCheckBox(dontAsk);
            const bool yes = box.exec() == QMessageBox::Yes;
            if (dontAsk->isChecked())
                m_context.config().setValue(QStringLiteral("confirm_delete"), false);
            if (!yes)
                return true;
        }
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
    } else if (id == QLatin1String("spr_crop")) {
        if (current >= count)
            return true;
        SpriteDocument edited = doc;
        SpriteEdits::crop(edited, {current});
        file->apply(tr("Crop image"), edited);
    } else {
        return false;
    }
    return true;
}

} // namespace OFS
