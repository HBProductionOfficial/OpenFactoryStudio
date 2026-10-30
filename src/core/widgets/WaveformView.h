#pragma once

#include "core/CoreExport.h"
#include "core/widgets/WheelSteps.h"
#include "formats/audio/SampleBuffer.h"
#include "formats/audio/WaveformPeaks.h"

#include <QWidget>

#include <optional>

class QScrollBar;

namespace OFS {

// Sounds editor workspace. x maps to frame floor(x * framesPerPixel + firstFrame).
class OFS_CORE_EXPORT WaveformView : public QWidget
{
    Q_OBJECT

public:
    enum class Tool
    {
        MoveView,
        Zoom,
        Select,
        MoveSelection,
        HorizontalResize,
        VerticalResize,
    };

    explicit WaveformView(QWidget* parent = nullptr);

    void setBuffer(const Formats::SampleBuffer& buffer);
    const Formats::SampleBuffer& buffer() const { return m_buffer; }

    Tool tool() const { return m_tool; }
    void setTool(Tool tool);

    double framesPerPixel() const { return m_framesPerPixel; }
    void setFramesPerPixel(double frames);
    void zoomIn();
    void zoomOut();
    void zoomFit();
    void zoomReset();
    double firstFrame() const { return m_firstFrame; }
    void setFirstFrame(double frame);

    // The cursor is a frame from 0 to the frame count. The selection is [start, end).
    qint64 cursor() const { return m_cursor; }
    void setCursor(qint64 frame);
    Formats::SampleRange selection() const { return m_selection; }
    void setSelection(Formats::SampleRange range);
    void setPlayPosition(qint64 frame);
    // The colour set's sound_wave. An invalid colour restores the default.
    void setWaveColor(const QColor& color);
    QColor waveColor() const { return m_waveColor; }
    qint64 playPosition() const { return m_play; }

    qint64 frameAt(int x) const;
    double xAt(double frame) const;

    QSize sizeHint() const override { return {640, 320}; }

Q_SIGNALS:
    void cursorChanged(qint64 frame);
    void cursorStepped(qint64 frame);
    void selectionChanged(Formats::SampleRange range);
    void zoomChanged(double framesPerPixel);
    // Tool results, sent on release. `start` counts in the sound without the moved frames.
    void moveRequested(Formats::SampleRange range, qint64 start);
    void stretchRequested(Formats::SampleRange range, qint64 frames);
    void amplitudeRequested(Formats::SampleRange range, double factor);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    WheelSteps m_wheel;
    QRect waveArea() const;
    void updateScrollBar();
    double fitFramesPerPixel() const;
    // The mouse pointer for the tool, and for where the pointer is or the drag started.
    void updatePointer(int pointerX);

    Formats::SampleBuffer m_buffer;
    Formats::WaveformPeaks m_peaks;
    QScrollBar* m_scroll;
    Tool m_tool = Tool::Select;
    double m_framesPerPixel = 1.0;
    double m_firstFrame = 0.0;
    qint64 m_cursor = 0;
    Formats::SampleRange m_selection;
    std::optional<Tool> m_toolBeforeCtrl; // tool to restore when Ctrl is released
    qint64 m_play = -1;
    QColor m_waveColor{230, 200, 40};
    bool m_dragging = false;
    Qt::MouseButton m_dragButton = Qt::NoButton;
    QPoint m_pressPos;
    qint64 m_anchor = 0;
    double m_firstAtPress = 0;
    bool m_fitted = false;
};

} // namespace OFS
