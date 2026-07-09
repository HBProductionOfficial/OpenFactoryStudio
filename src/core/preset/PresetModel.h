#pragma once

#include "core/CoreExport.h"

#include <QList>
#include <QMap>
#include <QSize>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

namespace OFS::Preset {

// The interface a preset script describes. It holds only names, so it can be built and tested
// without a GUI. PresetBuilder turns it into widgets.

struct Layout;

struct Control
{
    QString name;
    int iconSize = 0;
    bool flat = false;
    bool wordWrap = false;
    std::optional<Qt::Orientation> orientation;
    Qt::Alignment alignment;
    QSize size;
    std::shared_ptr<Layout> content;
};

struct LayoutItem
{
    enum class Kind
    {
        Control,
        Layout,
        Spacing,
        HorizontalSpacer,
        VerticalSpacer,
        Separator,
    };

    Kind kind = Kind::Control;
    // Grid layouts only. A span of -1 extends to the last row or column.
    int row = 0;
    int column = 0;
    int rowSpan = 1;
    int columnSpan = 1;
    int spacing = 0;
    int stretch = 0;
    std::shared_ptr<Control> control;
    std::shared_ptr<Layout> layout;
};

struct Layout
{
    enum class Type
    {
        Grid,
        Horizontal,
        Vertical,
    };

    Type type = Type::Grid;
    int margin = 0;
    int spacing = 0;
    QList<LayoutItem> items;
};

struct ToolBar
{
    QString name;
    int iconSize = 0;
    QStringList actions; // empty string = separator
};

struct Panel
{
    QString name;
    std::shared_ptr<Layout> layout;
};

struct Model
{
    QMap<QString, ToolBar> toolBars;
    QStringList toolBarOrder;
    QMap<QString, Panel> panels; // left panels, one per editor
    QStringList panelOrder;
    QMap<QString, Panel> extraPanels; // right panels
    QStringList extraPanelOrder;

    // Set by global presets. Empty means unchanged.
    QString iconset;
    QString stylesheet;
    QString colorset;

    QStringList errors;

    OFS_CORE_EXPORT QStringList referencedNames() const;
};

} // namespace OFS::Preset
