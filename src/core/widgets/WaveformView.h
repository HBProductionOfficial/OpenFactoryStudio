#pragma once

#include "core/CoreExport.h"
#include "formats/audio/SampleBuffer.h"
#include "formats/audio/WaveformPeaks.h"

#include <QWidget>

class QScrollBar;

namespace OFS {

class OFS_CORE_EXPORT WaveformView : public QWidget
{
    Q_OBJECT

public:
    explicit WaveformView(QWidget* parent = nullptr);

    void setBuffer(const Formats::SampleBuffer& buffer);
    const Formats::SampleBuffer& buffer() const { return m_buffer; }

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
    qint64 playPosition() const { return m_play; }

    qint64 frameAt(int x) const;
    double xAt(double frame) const;

    QSize sizeHint() const override { return {640, 320}; }

Q_SIGNALS:
    void cursorChanged(qint64 frame);
    void selectionChanged(Formats::SampleRange range);
    void zoomChanged(double framesPerPixel);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    QRect waveArea() const;
    void updateScrollBar();
    double maximumFramesPerPixel() const;

    Formats::SampleBuffer m_buffer;
    Formats::WaveformPeaks m_peaks;
    QScrollBar* m_scroll;
    double m_framesPerPixel = 1.0;
    double m_firstFrame = 0.0;
    qint64 m_cursor = 0;
    Formats::SampleRange m_selection;
    qint64 m_play = -1;
    bool m_selecting = false;
    qint64 m_anchor = 0;
    bool m_fitted = false;
};

} // namespace OFS
