#pragma once

#include <QColor>
#include <QImage>
#include <QList>
#include <QPointF>
#include <QWidget>

namespace OFS::Mugen {

// Drawing area of the Throw creator. Positions are in screen units from the origin on the floor line.
class AnimationPreview : public QWidget
{
    Q_OBJECT

public:
    struct Figure
    {
        QImage image;
        QPoint axis;
        QPointF position;
        double scale = 1.0; // screen units per sprite pixel
        bool flipH = false;
        bool flipV = false;
        QColor cross = Qt::white;
    };

    explicit AnimationPreview(QWidget* parent = nullptr);

    void setFigures(const QList<Figure>& figures);
    const QList<Figure>& figures() const { return m_figures; }
    QPointF toWidget(const QPointF& units) const;

    QSize sizeHint() const override { return {782, 452}; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QRect viewport() const;
    QPointF origin() const;

    QList<Figure> m_figures;
};

} // namespace OFS::Mugen
