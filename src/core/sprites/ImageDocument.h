#pragma once

#include "core/CoreExport.h"
#include "formats/image/Image.h"

#include <QList>
#include <QPair>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QStringList>

#include <vector>

namespace OFS {

struct OFS_CORE_EXPORT PaintColor
{
    int index = 0;
    Formats::Rgba rgba{};

    friend bool operator==(const PaintColor&, const PaintColor&) = default;
};

struct OFS_CORE_EXPORT ImageLayer
{
    QString name;
    Formats::Image image;
    bool visible = true;
    int blend = 0;
    int opacity = 100;
};

OFS_CORE_EXPORT QStringList blendModeNames();

struct OFS_CORE_EXPORT ImageDocument
{
    bool indexed = true;
    Formats::Palette palette;
    int width = 0;
    int height = 0;
    QList<ImageLayer> layers; // bottom first
    int current = 0;
    // Empty means no selection and every pixel is editable. Otherwise one byte per pixel, 1 when
    // selected. An all-zero mask makes nothing editable.
    std::vector<std::uint8_t> selection;
    QPoint origin;

    static ImageDocument fromImage(const Formats::Image& image, const QString& name);

    bool contains(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height; }
    bool editable(int x, int y) const;
    bool hasSelection() const { return !selection.empty(); }
    ImageLayer& layer() { return layers[current]; }
    const ImageLayer& layer() const { return layers[current]; }

    PaintColor pixel(int x, int y) const;
    void setPixel(int x, int y, const PaintColor& color);
};

namespace ImageEdits {

// Painting. Only editable pixels change.
OFS_CORE_EXPORT bool paint(ImageDocument& doc, QPoint from, QPoint to, int size, const PaintColor& color);
OFS_CORE_EXPORT bool remap(ImageDocument& doc, QPoint from, QPoint to, int size,
                           const QList<QPair<PaintColor, PaintColor>>& slots);
OFS_CORE_EXPORT bool fill(ImageDocument& doc, QPoint at, const PaintColor& color, bool contiguous);
OFS_CORE_EXPORT PaintColor pick(const ImageDocument& doc, QPoint at);

// Selection.
enum class SelectMode
{
    Replace,
    Add,
    Subtract,
    AllExcept,
};
OFS_CORE_EXPORT void selectColor(ImageDocument& doc, QPoint at, bool contiguous, SelectMode mode);
OFS_CORE_EXPORT void selectRect(ImageDocument& doc, const QRect& rect);
OFS_CORE_EXPORT void paintSelection(ImageDocument& doc, QPoint from, QPoint to, int size, bool add);
OFS_CORE_EXPORT void selectAll(ImageDocument& doc);
OFS_CORE_EXPORT void selectNone(ImageDocument& doc);
OFS_CORE_EXPORT void invertSelection(ImageDocument& doc);
OFS_CORE_EXPORT bool moveSelection(ImageDocument& doc, int dx, int dy, const PaintColor& background);

// Layers. Which layer becomes current is the same as in Fighter Factory.
OFS_CORE_EXPORT void addLayer(ImageDocument& doc);
OFS_CORE_EXPORT void deleteLayer(ImageDocument& doc);
OFS_CORE_EXPORT void duplicateLayer(ImageDocument& doc);
OFS_CORE_EXPORT void mergeVisible(ImageDocument& doc);
OFS_CORE_EXPORT void mergeAll(ImageDocument& doc);

OFS_CORE_EXPORT void resizeCanvas(ImageDocument& doc, int width, int height, const PaintColor& background);
// RGBA <-> indexed. Uses the nearest palette colour, and alpha below 128 becomes index 0.
OFS_CORE_EXPORT void setIndexed(ImageDocument& doc, bool indexed);
// EPX and HQ sample the layer magnified four times. HQ on 8-bit canvases works as EPX.
enum class RotateFilter
{
    Nearest,
    Bilinear,
    Epx,
    Hq,
};
OFS_CORE_EXPORT void rotate(ImageDocument& doc, double degrees, RotateFilter filter, const PaintColor& background);
OFS_CORE_EXPORT void flip(ImageDocument& doc, bool horizontal);

OFS_CORE_EXPORT Formats::Image flatten(const ImageDocument& doc, bool visibleOnly = true);
OFS_CORE_EXPORT Formats::Image layerRgba(const ImageDocument& doc, int layer);

struct OFS_CORE_EXPORT LayerSource
{
    QString name;
    Formats::Image image;
    QPoint axis;
};
OFS_CORE_EXPORT void addLayers(ImageDocument& doc, const QList<LayerSource>& sources, bool aligned, QPoint axis,
                               const PaintColor& background);

OFS_CORE_EXPORT Formats::Image copy(const ImageDocument& doc);
OFS_CORE_EXPORT void paste(ImageDocument& doc, const Formats::Image& image);

} // namespace ImageEdits

} // namespace OFS
