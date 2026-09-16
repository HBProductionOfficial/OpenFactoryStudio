#pragma once

#include "core/sounds/SoundDocument.h"
#include "core/sounds/SoundFile.h"

#include <QPointer>
#include <QWidget>

#include <memory>

namespace OFS {

class SoundPlayer;
class WaveformView;

// Sounds editor workspace for one sound file. Waveform edits change a pending copy until Save changes writes it as one
// undo step.
class SoundPage : public QWidget
{
    Q_OBJECT

public:
    explicit SoundPage(SoundFile* file, QWidget* parent = nullptr);
    ~SoundPage() override;

    SoundFile* file() const { return m_file; }
    WaveformView* view() const { return m_view; }
    SoundPlayer* player() const { return m_player; }

    int current() const { return m_current; }
    // Shows another sound. With pending changes it asks first and returns false when the user stays.
    bool setCurrent(int index);
    const Formats::SampleBuffer& samples() const;
    int bitsPerSample() const { return m_bits; }

    bool hasChanges() const { return m_pending; }
    bool confirmDiscard();
    void saveChanges();
    void discardChanges();
    void removeSelection();
    void fillSilence();
    void fadeIn();
    void fadeOut();
    void copySelection();
    void cutSelection();
    void paste();
    void selectAll();
    void selectNone();

    enum class State
    {
        Stopped,
        Playing,
        Paused,
    };
    State state() const { return m_state; }
    bool looping() const { return m_looping; }
    void play(bool loop);
    void pause();
    void moveCursor(qint64 sample);
    void stop();
    void setVolume(int percent);

Q_SIGNALS:
    void currentChanged(int index);
    void statusChanged();

private:
    void reload();
    void setPending(Formats::SampleBuffer samples, Formats::SampleRange selection);
    void setState(State state);

    QPointer<SoundFile> m_file;
    WaveformView* m_view;
    SoundPlayer* m_player;
    int m_current = 0;
    std::shared_ptr<const Formats::Bytes> m_shownData;
    bool m_pending = false;
    int m_bits = 16;
    State m_state = State::Stopped;
    bool m_looping = false;
    std::shared_ptr<Formats::SampleBuffer> m_playing;
};

} // namespace OFS
