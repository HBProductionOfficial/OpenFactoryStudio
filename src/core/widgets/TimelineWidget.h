#pragma once

#include "core/CoreExport.h"

#include <QList>
#include <QWidget>

namespace OFS {

class OFS_CORE_EXPORT TimelineWidget : public QWidget
{
    Q_OBJECT

public:
    struct Frame
    {
        int time = 0; // ticks, -1 = endless
        bool loopStart = false;
    };

    explicit TimelineWidget(QWidget* parent = nullptr);

    void setAnimation(const QString& name, const QList<Frame>& frames);
    void setPlayhead(int tick);
    int playhead() const { return m_playhead; }
    int frameAt(int x) const;
    static constexpr int EndlessLength = 30;

    QSize sizeHint() const override { return {640, 60}; }
    QSize minimumSizeHint() const override { return {100, 60}; }

Q_SIGNALS:
    void frameClicked(int frame);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    double tickWidth() const;
    int trackLeft() const;

    QString m_name;
    QList<Frame> m_frames;
    int m_playhead = 0;
};

} // namespace OFS
