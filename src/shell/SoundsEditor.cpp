#include "shell/SoundsEditor.h"

#include "core/GameTime.h"
#include "core/actions/ActionRegistry.h"
#include "core/app/AppContext.h"
#include "core/project/Project.h"
#include "core/settings/Config.h"
#include "core/sounds/SoundFile.h"
#include "core/sounds/SoundTransfer.h"
#include "core/widgets/WaveformView.h"
#include "formats/audio/Resampler.h"
#include "shell/FileDialogs.h"
#include "shell/OrganizerBinding.h"
#include "shell/SoundPage.h"
#include "shell/dialogs/ProjectDialogs.h"
#include "shell/dialogs/SoundDialogs.h"
#include "shell/dialogs/SpriteDialogs.h"

#include <QAction>
#include <QCheckBox>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>

#include <cmath>

namespace OFS {

using namespace Formats;

namespace {

template <typename T>
T* control(QWidget* panel, const char* name)
{
    return panel ? panel->findChild<T*>(QLatin1String(name)) : nullptr;
}

const char* const AudioFilters = "All supported (*.wav *.ogg *.flac *.mp3);;WAVEform audio format (*.wav);;"
                                 "Ogg Vorbis (*.ogg);;Free Lossless Audio Codec (*.flac);;"
                                 "MPEG-1/2 Audio Layer 3 (*.mp3)";

QStringList soundCaptions(const SoundDocument& doc)
{
    QStringList captions;
    for (const SoundEntry& sound : doc.sounds())
        captions << QStringLiteral("%1, %2").arg(sound.group).arg(sound.number);
    return captions;
}

// "1", "0.1", "0.24375", "0.499955": up to 6 significant digits.
QString seconds(double value)
{
    return QString::number(value, 'g', 6);
}

} // namespace

SoundsEditor::SoundsEditor(AppContext& context, QObject* parent)
    : QObject(parent)
    , m_context(context)
{
    // Organizer rows read "<group>, <number>". Moves change the order the sounds are stored in.
    OrganizerBinding::Source source;
    source.captions = [this] {
        QStringList rows;
        if (m_page) {
            for (const SoundEntry& sound : m_page->file()->document().sounds())
                rows << QStringLiteral("%1, %2").arg(sound.group).arg(sound.number);
        }
        return rows;
    };
    source.reorder = [this](const QList<int>& order) {
        if (!m_page || !m_page->confirmDiscard())
            return;
        const SoundDocument& doc = m_page->file()->document();
        if (order.size() != doc.count())
            return;
        SoundDocument edited = doc;
        for (int i = 0; i < order.size(); ++i)
            edited.sounds()[i] = doc.sounds()[order[i]];
        m_page->file()->apply(tr("Organize"), edited);
    };
    source.sortedOrder = [this] {
        QList<int> order;
        if (!m_page)
            return order;
        const QList<SoundEntry>& sounds = m_page->file()->document().sounds();
        for (int i = 0; i < sounds.size(); ++i)
            order << i;
        std::stable_sort(order.begin(), order.end(), [&sounds](int a, int b) {
            return std::pair(sounds[a].group, sounds[a].number) < std::pair(sounds[b].group, sounds[b].number);
        });
        return order;
    };
    source.activate = [this](int row) {
        if (m_page)
            m_page->setCurrent(row);
    };
    m_organizer = new OrganizerBinding(source, this);
}

void SoundsEditor::bindPanel(QWidget* panel)
{
    m_selector = control<QScrollBar>(panel, "snd_sel");
    m_selectorLabel = control<QLabel>(panel, "snd_sel_lb");
    m_group = control<QSpinBox>(panel, "snd_grp");
    m_number = control<QSpinBox>(panel, "snd_num");
    m_info = control<QLabel>(panel, "snd_info_lb");
    m_volume = control<QSlider>(panel, "snd_volume");
    m_cursor = control<QSpinBox>(panel, "snd_curr_sample");
    m_start = control<QSpinBox>(panel, "snd_sel_start");
    m_end = control<QSpinBox>(panel, "snd_sel_end");
    if (m_selector)
        connect(m_selector, &QScrollBar::valueChanged, this, [this](int value) {
            if (!m_updating && m_page)
                m_page->setCurrent(value);
        });
    for (QSpinBox* box : {m_group.data(), m_number.data()}) {
        if (!box)
            continue;
        box->setKeyboardTracking(false);
        connect(box, &QSpinBox::valueChanged, this, [this] {
            if (m_updating || !m_page || !m_group || !m_number)
                return;
            SoundFile* file = m_page->file();
            const int index = m_page->current();
            if (index < 0 || index >= file->document().count())
                return;
            const SoundEntry& sound = file->document().sounds().at(index);
            if (sound.group == m_group->value() && sound.number == m_number->value())
                return;
            SoundDocument doc = file->document();
            SoundEdits::setIdentifiers(doc, index, m_group->value(), m_number->value());
            file->apply(tr("Sound group and index"), doc);
        });
    }
    if (m_volume) {
        m_volume->setRange(0, 100);
        m_volume->setValue(std::clamp(m_context.config().intValue(QStringLiteral("snd_volume"), 55), 0, 100));
        connect(m_volume, &QSlider::valueChanged, this, [this](int value) {
            m_context.config().setValue(QStringLiteral("snd_volume"), value);
            if (m_page)
                m_page->setVolume(value);
        });
    }
    if (m_cursor)
        connect(m_cursor, &QSpinBox::valueChanged, this, [this](int value) {
            if (!m_updating && m_page)
                m_page->moveCursor(value);
        });
    for (QSpinBox* box : {m_start.data(), m_end.data()}) {
        if (!box)
            continue;
        connect(box, &QSpinBox::valueChanged, this, [this] {
            if (m_updating || !m_page || !m_start || !m_end)
                return;
            const int start = std::min(m_start->value(), m_end->value());
            const int end = std::max(m_start->value(), m_end->value());
            m_page->view()->setSelection(SampleRange{std::size_t(start), std::size_t(end - start)});
        });
    }
    refresh();
}

void SoundsEditor::importFromProject(const QList<QPair<QString, const SoundDocument*>>& sources)
{
    if (!m_page)
        return;
    QWidget* parent = m_page->window();
    if (sources.isEmpty()) {
        QMessageBox::warning(parent, tr("Warning"), tr("You can't import from the destination project!"));
        return;
    }
    QList<ItemImportDialog::Source> projects;
    for (const auto& source : sources) {
        ItemImportDialog::Source project{source.first, {}};
        for (const SoundEntry& sound : source.second->sounds())
            project.items << ItemImportDialog::Item{QStringLiteral("%1, %2").arg(sound.group).arg(sound.number),
                                                    QStringLiteral("%1,%2").arg(sound.group).arg(sound.number),
                                                    sound.group * 65536 + sound.number};
        projects << project;
    }
    ItemImportDialog dialog(ItemImportDialog::Kind::Sounds, projects, parent);
    if (dialog.exec() != QDialog::Accepted || dialog.project() < 0 || dialog.project() >= sources.size() ||
        !m_page->confirmDiscard())
        return;
    const SoundDocument& from = *sources[dialog.project()].second;
    SoundFile* file = m_page->file();
    SoundDocument edited = file->document();
    QList<int> added;
    int replaced = 0;
    int after = m_page->current();
    for (const ItemImportDialog::Chosen& chosen : dialog.items()) {
        const QStringList parts = chosen.identifier.split(QLatin1Char(','));
        bool okGroup = false;
        bool okNumber = false;
        const int group = parts.value(0).trimmed().toInt(&okGroup);
        const int number = parts.value(1).trimmed().toInt(&okNumber);
        if (parts.size() != 2 || !okGroup || !okNumber || chosen.source < 0 || chosen.source >= from.count())
            continue;
        const SoundEntry& sound = from.sounds()[chosen.source];
        if (const int existing = edited.find(group, number); existing >= 0) {
            bool replace = dialog.conflict() == ItemImportDialog::Conflict::Replace;
            if (dialog.conflict() == ItemImportDialog::Conflict::Ask)
                replace = QMessageBox::question(
                              parent, tr("Conflict resolution!"),
                              tr("Sound '%1,%2' already exists, do you want to replace it?").arg(group).arg(number),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
            if (replace) {
                edited.sounds()[existing].data = sound.data;
                ++replaced;
                continue;
            }
            if (dialog.conflict() != ItemImportDialog::Conflict::AsIs)
                continue;
        }
        after = SoundEdits::insert(edited, after, group, number, sound.data ? *sound.data : Formats::Bytes());
        added << after;
    }
    if (!added.isEmpty() || replaced > 0) {
        file->apply(tr("Import from another project"), edited);
        if (!added.isEmpty())
            m_page->setCurrent(added.first());
    }
    const int count = int(added.size()) + replaced;
    QMessageBox::information(parent, tr("Import from another project..."),
                             count > 0 ? tr("%1 sound(s) were imported!").arg(count) : tr("No sound imported!"));
}

void SoundsEditor::setPage(SoundPage* page)
{
    if (m_page == page)
        return;
    if (m_page)
        disconnect(m_page, nullptr, this, nullptr);
    m_page = page;
    if (m_page) {
        connect(m_page, &SoundPage::statusChanged, this, [this] {
            refresh();
            Q_EMIT stateChanged();
        });
        connect(m_page, &SoundPage::currentChanged, this, &SoundsEditor::refresh);
        connect(m_page->file(), &SoundFile::documentChanged, m_organizer, &OrganizerBinding::refresh);
        if (m_volume)
            m_page->setVolume(m_volume->value());
    }
    refresh();
    m_organizer->refresh();
    Q_EMIT stateChanged();
}

void SoundsEditor::refresh()
{
    const bool updating = std::exchange(m_updating, true);
    const SoundDocument* doc = m_page ? &m_page->file()->document() : nullptr;
    const int count = doc ? doc->count() : 0;
    const int current = m_page ? m_page->current() : 0;
    const bool has = count > 0 && current < count;
    if (m_empty != (count == 0)) {
        m_empty = count == 0;
        QTimer::singleShot(0, this, &SoundsEditor::stateChanged);
    }
    if (m_selector) {
        m_selector->setRange(0, std::max(0, count - 1));
        m_selector->setValue(current);
    }
    if (m_selectorLabel)
        m_selectorLabel->setText(count > 0 ? QStringLiteral("%1 / %2").arg(current + 1).arg(count) : QString());
    for (QWidget* w : std::initializer_list<QWidget*>{m_group, m_number, m_cursor, m_start, m_end}) {
        if (w)
            w->setEnabled(has);
    }
    if (has) {
        const SoundEntry& sound = doc->sounds().at(current);
        if (m_group)
            m_group->setValue(sound.group);
        if (m_number)
            m_number->setValue(sound.number);
        const SampleBuffer& samples = m_page->samples();
        if (m_info) {
            const double duration = samples.durationSeconds();
            m_info->setText(tr("Channels: %1 (%2)\nSample rate: %3 Hz\nBits per sample: %4 bits\n"
                               "Duration: %5 seconds\nTick count: %6 at 60Hz")
                                .arg(samples.channels())
                                .arg(samples.channels() >= 2 ? tr("Stereo") : tr("Mono"))
                                .arg(samples.sampleRate())
                                .arg(m_page->bitsPerSample())
                                .arg(seconds(duration))
                                .arg(qint64(std::floor(duration * TicksPerSecond + 1e-9))));
        }
        const int frames = int(std::min<std::size_t>(samples.frameCount(), 2147483647));
        if (m_cursor) {
            m_cursor->setRange(0, frames);
            m_cursor->setValue(int(m_page->view()->cursor()));
        }
        const SampleRange selection = m_page->view()->selection();
        for (QSpinBox* box : {m_start.data(), m_end.data()}) {
            if (box)
                box->setRange(0, frames);
        }
        if (m_start)
            m_start->setValue(int(selection.start));
        if (m_end)
            m_end->setValue(int(selection.end()));
    } else if (m_info) {
        m_info->clear();
    }
    m_updating = updating;
}

bool SoundsEditor::confirmLeave()
{
    return !m_page || m_page->confirmDiscard();
}

bool SoundsEditor::perform(const QString& id)
{
    if (!m_page)
        return false;
    SoundPage* page = m_page;
    SoundFile* file = page->file();
    const SoundDocument& doc = file->document();
    const int current = page->current();
    const int count = doc.count();
    auto runStart = [&doc](int index) {
        int i = index;
        while (i > 0 && doc.sounds().at(i - 1).group == doc.sounds().at(index).group)
            --i;
        return i;
    };
    if (id == QLatin1String("snd_prev")) {
        page->setCurrent(current - 1);
    } else if (id == QLatin1String("snd_next")) {
        page->setCurrent(std::min(current + 1, count - 1));
    } else if (id == QLatin1String("snd_prev_grp")) {
        if (count > 0) {
            const int start = runStart(current);
            if (start > 0)
                page->setCurrent(runStart(start - 1));
        }
    } else if (id == QLatin1String("snd_next_grp")) {
        if (count > 0) {
            int i = current;
            while (i < count - 1 && doc.sounds().at(i + 1).group == doc.sounds().at(current).group)
                ++i;
            page->setCurrent(i + 1 < count ? i + 1 : 0);
        }
    } else if (id == QLatin1String("snd_play")) {
        page->play(false);
    } else if (id == QLatin1String("snd_play_loop")) {
        page->play(true);
    } else if (id == QLatin1String("snd_pause")) {
        page->pause();
    } else if (id == QLatin1String("snd_stop")) {
        page->stop();
    } else if (id == QLatin1String("snd_select_all")) {
        page->selectAll();
    } else if (id == QLatin1String("snd_select_none")) {
        page->selectNone();
    } else if (id == QLatin1String("snd_remove_selected")) {
        page->removeSelection();
    } else if (id == QLatin1String("snd_fill_silence")) {
        page->fillSilence();
    } else if (id == QLatin1String("snd_fade_in")) {
        page->fadeIn();
    } else if (id == QLatin1String("snd_fade_out")) {
        page->fadeOut();
    } else if (id == QLatin1String("snd_save_changes")) {
        page->saveChanges();
    } else if (id == QLatin1String("snd_discard_changes")) {
        page->discardChanges();
    } else if (id.startsWith(QLatin1String("snd_tool_"))) {
        const QString tool = id.mid(9);
        page->view()->setTool(tool == QLatin1String("offset")    ? WaveformView::Tool::MoveView
                              : tool == QLatin1String("zoom")    ? WaveformView::Tool::Zoom
                              : tool == QLatin1String("move")    ? WaveformView::Tool::MoveSelection
                              : tool == QLatin1String("hresize") ? WaveformView::Tool::HorizontalResize
                              : tool == QLatin1String("vresize") ? WaveformView::Tool::VerticalResize
                                                                 : WaveformView::Tool::Select);
    } else if (!performFiles(id)) {
        return false;
    }
    Q_EMIT stateChanged();
    return true;
}

bool SoundsEditor::performFiles(const QString& id)
{
    SoundPage* page = m_page;
    SoundFile* file = page->file();
    const SoundDocument& doc = file->document();
    const int current = page->current();
    const int count = doc.count();
    QWidget* parent = page->window();
    Config& config = m_context.config();
    const QString folder = config.value(QStringLiteral("snd_path"), QFileInfo(file->path()).absolutePath());
    if (id == QLatin1String("snd_add")) {
        QStringList files = FileDialogs::getOpenFileNames(parent, tr("Add"), folder, QString::fromLatin1(AudioFilters));
        if (files.isEmpty())
            return true;
        config.setValue(QStringLiteral("snd_path"), QFileInfo(files.first()).absolutePath());
        files.sort();
        SoundAddOptions options;
        if (config.boolValue(QStringLiteral("show_addsound_dialog"), true)) {
            AddItemsDialog dialog(AddItemsDialog::Kind::Sounds, parent);
            if (dialog.exec() != QDialog::Accepted)
                return true;
            options.detect = dialog.detect();
            options.group = dialog.group();
            options.number = dialog.number();
            options.step = dialog.step();
        }
        SoundDocument edited = doc;
        QStringList errors;
        const QList<int> added = addSoundFiles(edited, count > 0 ? current : -1, files, options, &errors);
        if (!errors.isEmpty())
            QMessageBox::warning(parent, tr("Add"), errors.join(QLatin1Char('\n')));
        if (added.isEmpty() || !page->confirmDiscard())
            return true;
        file->apply(tr("Add sounds"), edited);
        page->setCurrent(added.last());
    } else if (id == QLatin1String("snd_delete")) {
        if (current >= count)
            return true;
        if (config.boolValue(QStringLiteral("confirm_delete"), true)) {
            QMessageBox box(QMessageBox::Question, tr("Delete"), tr("Do you really want to delete the sound?"),
                            QMessageBox::Yes | QMessageBox::No, parent);
            box.setDefaultButton(QMessageBox::Yes);
            auto* dontAsk = new QCheckBox(tr("Don't show this again"), &box);
            box.setCheckBox(dontAsk);
            const bool yes = box.exec() == QMessageBox::Yes;
            if (dontAsk->isChecked())
                config.setValue(QStringLiteral("confirm_delete"), false);
            if (!yes)
                return true;
        }
        SoundDocument edited = doc;
        SoundEdits::remove(edited, {current});
        file->apply(tr("Delete sound"), edited);
    } else if (id == QLatin1String("snd_duplicate")) {
        if (current >= count || !page->confirmDiscard())
            return true;
        SoundDocument edited = doc;
        const int copy = SoundEdits::duplicate(edited, current);
        file->apply(tr("Duplicate sound"), edited);
        page->setCurrent(copy);
    } else if (id == QLatin1String("snd_open_sample")) {
        if (current >= count)
            return true;
        const QString path =
            FileDialogs::getOpenFileName(parent, tr("Change sound..."), folder, QString::fromLatin1(AudioFilters));
        if (path.isEmpty())
            return true;
        config.setValue(QStringLiteral("snd_path"), QFileInfo(path).absolutePath());
        QFile in(path);
        if (!in.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(parent, tr("Change sound..."), in.errorString());
            return true;
        }
        const QByteArray bytes = in.readAll();
        const auto* begin = reinterpret_cast<const std::byte*>(bytes.constData());
        QString error;
        Bytes data = SoundEdits::importData(Bytes(begin, begin + bytes.size()), &error);
        if (data.empty()) {
            QMessageBox::warning(parent, tr("Change sound..."), error);
            return true;
        }
        SoundDocument edited = doc;
        edited.sounds()[current].data = std::make_shared<const Bytes>(std::move(data));
        file->apply(tr("Change sound"), edited);
    } else if (id == QLatin1String("snd_save_sample")) {
        if (current >= count)
            return true;
        const QString path = FileDialogs::getSaveFileName(
            parent, tr("Save sound as..."), folder,
            tr("WAVEform audio format (*.wav);;Ogg Vorbis (*.ogg);;Free Lossless Audio Codec (*.flac)"));
        if (path.isEmpty())
            return true;
        ItemsDialog items(soundCaptions(doc), current, parent);
        if (items.exec() != QDialog::Accepted || items.selection().isEmpty())
            return true;
        config.setValue(QStringLiteral("snd_path"), QFileInfo(path).absolutePath());
        QString error;
        if (saveSounds(doc, items.selection(), path, &error).size() != items.selection().size())
            QMessageBox::warning(parent, tr("Save sound as..."), error);
    } else if (id == QLatin1String("snd_resample")) {
        if (current >= count)
            return true;
        const SampleBuffer& samples = page->samples();
        ResampleDialog dialog(soundCaptions(doc), current, samples.channels(), int(samples.sampleRate()),
                              page->bitsPerSample(), parent);
        if (dialog.exec() != QDialog::Accepted || dialog.sounds().isEmpty() || !page->confirmDiscard())
            return true;
        SoundDocument edited = doc;
        for (int index : dialog.sounds()) {
            auto decoded = SoundEdits::decode(edited.sounds().at(index));
            if (!decoded)
                continue;
            SampleBuffer converted = convertChannels(decoded.value().buffer, std::uint16_t(dialog.channels()));
            auto resampled = resample(converted, std::uint32_t(dialog.sampleRate()));
            if (!resampled)
                continue;
            SoundEdits::setSamples(edited, index, resampled.value(), dialog.bitsPerSample());
        }
        file->apply(tr("Resample"), edited);
    } else if (id == QLatin1String("snd_delete_grp") || id == QLatin1String("snd_rename_grp")) {
        if (current >= count)
            return true;
        const bool rename = id == QLatin1String("snd_rename_grp");
        GroupDialog dialog(rename ? GroupDialog::Kind::Rename : GroupDialog::Kind::Delete,
                           doc.sounds().at(current).group, parent);
        if (dialog.exec() != QDialog::Accepted || !page->confirmDiscard())
            return true;
        SoundDocument edited = doc;
        QList<int> removed;
        for (int i = 0; i < edited.count(); ++i) {
            SoundEntry& sound = edited.sounds()[i];
            const bool inRange =
                dialog.first() < 0 || (sound.number >= dialog.first() && sound.number <= dialog.last());
            if (sound.group != dialog.group() || !inRange)
                continue;
            if (rename)
                sound.group = dialog.newGroup();
            else
                removed << i;
        }
        SoundEdits::remove(edited, removed);
        file->apply(rename ? tr("Rename group") : tr("Delete group"), edited);
    } else if (id == QLatin1String("snd_export")) {
        SpriteExportDialog dialog(parent);
        if (dialog.exec() != QDialog::Accepted)
            return true;
        QList<int> sounds;
        for (int i = 0; i < count; ++i) {
            if (dialog.group() < 0 || doc.sounds().at(i).group == dialog.group())
                sounds << i;
        }
        QString path = FileDialogs::getSaveFileName(parent, tr("Export..."), folder,
                                                    tr("Fighter Factory Export (*.ffe);;Elecbyte SndMaker (*.txt)"));
        if (path.isEmpty())
            return true;
        if (QFileInfo(path).suffix().isEmpty())
            path += QStringLiteral(".ffe");
        config.setValue(QStringLiteral("snd_path"), QFileInfo(path).absolutePath());
        const SoundListFormat format = QFileInfo(path).suffix().compare(QLatin1String("txt"), Qt::CaseInsensitive) == 0
                                           ? SoundListFormat::SndMaker
                                           : SoundListFormat::Ffe;
        QString error;
        if (!exportSounds(doc, sounds, path, format, &error))
            QMessageBox::warning(parent, tr("Export..."), error);
    } else if (id == QLatin1String("snd_import")) {
        const QString path =
            FileDialogs::getOpenFileName(parent, tr("Import..."), folder, tr("Fighter Factory Export (*.ffe)"));
        if (path.isEmpty() || !page->confirmDiscard())
            return true;
        config.setValue(QStringLiteral("snd_path"), QFileInfo(path).absolutePath());
        SoundImportOptions options;
        options.askConflict = [parent](int group, int number) {
            QMessageBox box(QMessageBox::Question, tr("Conflict!"),
                            tr("The sound group '%1' number '%2' already exists. Do you want to replace it?\n"
                               "Choose 'No' to skip it, or 'Ignore' to insert as is (remember to rename later).")
                                .arg(group)
                                .arg(number),
                            QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::No | QMessageBox::NoToAll |
                                QMessageBox::Ignore,
                            parent);
            box.setDefaultButton(QMessageBox::No);
            switch (box.exec()) {
            case QMessageBox::Yes:
                return SoundConflict::Replace;
            case QMessageBox::YesToAll:
                return SoundConflict::ReplaceAll;
            case QMessageBox::NoToAll:
                return SoundConflict::SkipAll;
            case QMessageBox::Ignore:
                return SoundConflict::Insert;
            default:
                return SoundConflict::Skip;
            }
        };
        SoundDocument edited = doc;
        QString error;
        const QList<int> added = importFfe(edited, count > 0 ? current : -1, path, options, &error);
        if (!error.isEmpty()) {
            QMessageBox::warning(parent, tr("Import..."), error);
            return true;
        }
        file->apply(tr("Import"), edited);
        if (!added.isEmpty())
            page->setCurrent(added.first());
    } else {
        return false;
    }
    return true;
}

} // namespace OFS
