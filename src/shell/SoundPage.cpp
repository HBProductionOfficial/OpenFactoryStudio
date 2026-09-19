#include "shell/SoundPage.h"

#include "core/audio/SoundPlayer.h"
#include "core/sounds/SoundFile.h"
#include "core/widgets/WaveformView.h"
#include "formats/audio/AudioEdit.h"
#include "formats/audio/Resampler.h"

#include <QMessageBox>
#include <QVBoxLayout>

namespace OFS {

using namespace Formats;

namespace {

SampleBuffer& copiedSamples()
{
    static SampleBuffer samples;
    return samples;
}
bool& hasCopiedSamples()
{
    static bool copied = false;
    return copied;
}

} // namespace

SoundPage::SoundPage(SoundFile* file, QWidget* parent)
    : QWidget(parent)
    , m_file(file)
    , m_view(new WaveformView(this))
    , m_player(new SoundPlayer(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);
    m_player->setVolume(0.55f);
    connect(m_view, &WaveformView::cursorChanged, this, &SoundPage::statusChanged);
    connect(m_view, &WaveformView::cursorStepped, this, &SoundPage::moveCursor);
    connect(m_view, &WaveformView::selectionChanged, this, &SoundPage::statusChanged);
    connect(m_view, &WaveformView::moveRequested, this, [this](SampleRange range, qint64 start) {
        setPending(moveRange(samples(), range, std::size_t(start)), SampleRange{std::size_t(start), range.count});
    });
    connect(m_view, &WaveformView::stretchRequested, this, [this](SampleRange range, qint64 frames) {
        auto stretched = stretchRange(samples(), range, std::size_t(frames));
        if (stretched)
            setPending(std::move(stretched.value()), SampleRange{range.start, std::size_t(frames)});
    });
    connect(m_view, &WaveformView::amplitudeRequested, this, [this](SampleRange range, double factor) {
        setPending(changeVolume(samples(), range, float(factor)), range);
    });
    connect(m_player, &SoundPlayer::positionChanged, this, [this](qint64 frame) {
        if (m_state == State::Playing)
            m_view->setPlayPosition(frame);
    });
    connect(m_player, &SoundPlayer::finished, this, [this] {
        // At the end, stop and put the cursor back to 0. With loop on the player goes on by itself.
        m_view->setPlayPosition(-1);
        m_view->setCursor(0);
        setState(State::Stopped);
    });
    connect(m_file, &SoundFile::documentChanged, this, [this] {
        const int count = m_file->document().count();
        if (m_current >= count)
            m_current = std::max(0, count - 1);
        const auto data = count > 0 ? m_file->document().sounds().at(m_current).data : nullptr;
        if (!m_pending && data != m_shownData)
            reload();
        Q_EMIT statusChanged();
    });
    reload();
}

SoundPage::~SoundPage()
{
    m_player->stop();
}

const SampleBuffer& SoundPage::samples() const
{
    return m_view->buffer();
}

void SoundPage::reload()
{
    stop();
    m_pending = false;
    const SoundDocument& doc = m_file->document();
    SampleBuffer buffer;
    m_bits = 16;
    m_shownData = nullptr;
    if (m_current >= 0 && m_current < doc.count()) {
        m_shownData = doc.sounds().at(m_current).data;
        auto decoded = SoundEdits::decode(doc.sounds().at(m_current));
        if (decoded) {
            buffer = std::move(decoded.value().buffer);
            m_bits = decoded.value().bitsPerSample == 8 ? 8 : 16;
        }
    }
    m_view->setBuffer(buffer);
    m_view->setSelection(SampleRange{});
    m_view->setCursor(0);
    Q_EMIT statusChanged();
}

bool SoundPage::setCurrent(int index)
{
    const int count = m_file->document().count();
    index = count > 0 ? std::clamp(index, 0, count - 1) : 0;
    if (index == m_current && m_shownData)
        return true;
    if (!confirmDiscard()) {
        Q_EMIT currentChanged(m_current);
        return false;
    }
    m_current = index;
    reload();
    Q_EMIT currentChanged(m_current);
    return true;
}

bool SoundPage::confirmDiscard()
{
    if (!m_pending)
        return true;
    QMessageBox box(QMessageBox::Question, tr("Discard changes?"),
                    tr("Do you want to discard the changes made in the current sound?"),
                    QMessageBox::Yes | QMessageBox::No, window());
    box.setDefaultButton(QMessageBox::Yes);
    if (box.exec() != QMessageBox::Yes)
        return false;
    m_pending = false;
    return true;
}

void SoundPage::setPending(SampleBuffer buffer, SampleRange selection)
{
    stop();
    m_pending = true;
    const double first = m_view->firstFrame();
    m_view->setBuffer(buffer);
    m_view->setFirstFrame(first);
    m_view->setSelection(selection);
    Q_EMIT statusChanged();
}

void SoundPage::saveChanges()
{
    if (!m_pending || m_current < 0 || m_current >= m_file->document().count())
        return;
    SoundDocument doc = m_file->document();
    QString error;
    if (!SoundEdits::setSamples(doc, m_current, samples(), m_bits, &error)) {
        QMessageBox::warning(window(), tr("Save changes"), error);
        return;
    }
    m_pending = false;
    m_shownData = doc.sounds().at(m_current).data;
    m_file->apply(tr("Save changes"), doc);
    Q_EMIT statusChanged();
}

void SoundPage::discardChanges()
{
    // Drop pending changes without asking.
    if (m_pending)
        reload();
}

void SoundPage::removeSelection()
{
    const SampleRange range = m_view->selection();
    if (range.empty())
        return;
    setPending(removeRange(samples(), range), SampleRange{range.start, 0});
}

void SoundPage::fillSilence()
{
    const SampleRange range = m_view->selection();
    if (!range.empty())
        setPending(Formats::fillSilence(samples(), range), range);
}

void SoundPage::fadeIn()
{
    const SampleRange range = m_view->selection();
    if (!range.empty())
        setPending(Formats::fadeIn(samples(), range), range);
}

void SoundPage::fadeOut()
{
    const SampleRange range = m_view->selection();
    if (!range.empty())
        setPending(Formats::fadeOut(samples(), range), range);
}

void SoundPage::copySelection()
{
    const SampleRange range = m_view->selection();
    if (range.empty())
        return;
    copiedSamples() = copyRange(samples(), range);
    hasCopiedSamples() = true;
}

void SoundPage::cutSelection()
{
    copySelection();
    removeSelection();
}

void SoundPage::paste()
{
    if (!hasCopiedSamples())
        return;
    const SampleRange range = m_view->selection();
    auto clip = conformTo(copiedSamples(), samples());
    if (!clip)
        return;
    auto result = range.empty() ? insertBuffer(samples(), range.start, clip.value())
                                : replaceRange(samples(), range, clip.value());
    if (result)
        setPending(std::move(result.value()), SampleRange{range.start, clip.value().frameCount()});
}

void SoundPage::selectAll()
{
    m_view->setSelection(SampleRange{0, samples().frameCount()});
}

void SoundPage::selectNone()
{
    m_view->setSelection(SampleRange{});
}

void SoundPage::setState(State state)
{
    if (state == m_state)
        return;
    m_state = state;
    Q_EMIT statusChanged();
}

void SoundPage::play(bool loop)
{
    if (m_state == State::Paused && m_playing) {
        if (m_player->resume())
            setState(State::Playing);
        return;
    }
    if (samples().empty())
        return;
    m_player->stop();
    m_playing = std::make_shared<SampleBuffer>(samples());
    const std::size_t start = std::size_t(m_view->cursor());
    const bool started = loop ? m_player->playLoop(m_playing, kAllFrames, start) : m_player->play(m_playing, start);
    if (!started) {
        QMessageBox::warning(window(), tr("Play"), m_player->errorString());
        return;
    }
    m_looping = loop;
    setState(State::Playing);
}

void SoundPage::moveCursor(qint64 sample)
{
    // Moving the cursor stops playback and sets the status to Paused. Play then starts there.
    m_player->stop();
    m_playing.reset();
    m_view->setPlayPosition(-1);
    m_view->setCursor(sample);
    setState(State::Paused);
}

void SoundPage::pause()
{
    if (m_state != State::Playing)
        return;
    m_player->pause();
    setState(State::Paused);
}

void SoundPage::stop()
{
    if (m_state == State::Stopped)
        return;
    m_player->stop();
    m_view->setPlayPosition(-1);
    m_view->setCursor(0);
    setState(State::Stopped);
}

void SoundPage::setVolume(int percent)
{
    m_player->setVolume(float(std::clamp(percent, 0, 100)) / 100.0f);
}

} // namespace OFS
