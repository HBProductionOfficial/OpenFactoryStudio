#include "core/preset/PresetEngine.h"

#include <QFile>

#include <sol/sol.hpp>

namespace OFS::Preset {

namespace {

// Lua-side handles. They point into the model being built so chained calls modify it in place.

struct LayoutHandle;

struct ControlHandle
{
    std::shared_ptr<Control> control;

    ControlHandle& wordwrap()
    {
        control->wordWrap = true;
        return *this;
    }
    ControlHandle& setContent(const LayoutHandle& layout);

    ControlHandle& size(int w, int h)
    {
        control->size = QSize(w, h);
        return *this;
    }
    ControlHandle& icon(int px)
    {
        control->iconSize = px;
        return *this;
    }
    ControlHandle& flat()
    {
        control->flat = true;
        return *this;
    }
    ControlHandle& horizontal()
    {
        control->orientation = Qt::Horizontal;
        return *this;
    }
    ControlHandle& vertical()
    {
        control->orientation = Qt::Vertical;
        return *this;
    }
    ControlHandle& align(sol::variadic_args args)
    {
        Qt::Alignment a;
        for (auto v : args) {
            if (v.get_type() == sol::type::number)
                a |= Qt::Alignment(v.get<int>());
        }
        control->alignment = a;
        return *this;
    }
};

struct LayoutHandle
{
    std::shared_ptr<Layout> layout;

    int margin() const { return layout->margin; }
    void setMargin(int m) { layout->margin = m; }
    int spacing() const { return layout->spacing; }
    void setSpacing(int s) { layout->spacing = s; }

    static void place(LayoutItem& item, const sol::variadic_args& args, int first)
    {
        int values[4] = {0, 0, 1, 1};
        int n = 0;
        for (int i = first; i < static_cast<int>(args.size()) && n < 4; ++i, ++n) {
            if (args[i].get_type() == sol::type::number)
                values[n] = args[i].get<int>();
        }
        item.row = values[0];
        item.column = values[1];
        item.rowSpan = values[2] == 0 ? 1 : values[2];
        item.columnSpan = values[3] == 0 ? 1 : values[3];
    }

    ControlHandle addControl(const std::string& name, sol::variadic_args args)
    {
        LayoutItem item;
        item.kind = LayoutItem::Kind::Control;
        item.control = std::make_shared<Control>();
        item.control->name = QString::fromStdString(name);
        place(item, args, 0);
        layout->items.append(item);
        return ControlHandle{item.control};
    }

    // Grid layouts: addLayout(layout, row, column[, rowSpan, columnSpan]).
    // Box layouts:  addLayout(layout[, stretch]).
    LayoutHandle& addLayout(const LayoutHandle& child, sol::variadic_args args)
    {
        LayoutItem item;
        item.kind = LayoutItem::Kind::Layout;
        item.layout = child.layout;
        if (layout->type == Layout::Type::Grid) {
            place(item, args, 0);
        } else if (args.size() > 0 && args[0].get_type() == sol::type::number) {
            item.stretch = args[0].get<int>();
        }
        layout->items.append(item);
        return *this;
    }

    LayoutHandle& addSpacing(int size)
    {
        LayoutItem item;
        item.kind = LayoutItem::Kind::Spacing;
        item.spacing = size;
        layout->items.append(item);
        return *this;
    }

    LayoutHandle& addSpacer(LayoutItem::Kind kind, const sol::variadic_args& args)
    {
        LayoutItem item;
        item.kind = kind;
        place(item, args, 0);
        layout->items.append(item);
        return *this;
    }

    LayoutHandle& addVerticalSpacer(sol::variadic_args args)
    {
        return addSpacer(LayoutItem::Kind::VerticalSpacer, args);
    }
    LayoutHandle& addHorizontalSpacer(sol::variadic_args args)
    {
        return addSpacer(LayoutItem::Kind::HorizontalSpacer, args);
    }
    LayoutHandle& addSeparator(sol::variadic_args args) { return addSpacer(LayoutItem::Kind::Separator, args); }
};

ControlHandle& ControlHandle::setContent(const LayoutHandle& layout)
{
    control->content = layout.layout;
    return *this;
}

struct ToolBarHandle
{
    Model* model;
    QString name;

    ToolBar& bar() const { return model->toolBars[name]; }
    int iconSize() const { return bar().iconSize; }
    void setIconSize(int px) { bar().iconSize = px; }
    void addButton(const std::string& action) { bar().actions.append(QString::fromStdString(action)); }
    void addSeparator() { bar().actions.append(QString()); }
    void clear() { bar().actions.clear(); }
};

struct PanelHandle
{
    Model* model;
    QString name;
    bool extra;

    Panel& panel() const { return extra ? model->extraPanels[name] : model->panels[name]; }
    sol::optional<LayoutHandle> layout() const
    {
        const Panel& p = panel();
        if (!p.layout)
            return sol::nullopt;
        return LayoutHandle{p.layout};
    }
    void setLayout(const LayoutHandle& l) { panel().layout = l.layout; }
};

// FF.gridLayout([parent]) and friends: a panel or a container control given as parent receives
// the new layout as its content.
LayoutHandle newLayout(Layout::Type type, sol::variadic_args args)
{
    auto layout = std::make_shared<Layout>();
    layout->type = type;
    LayoutHandle handle{layout};
    if (args.size() > 0) {
        sol::object parent = args[0];
        if (parent.is<PanelHandle>())
            parent.as<PanelHandle>().setLayout(handle);
        else if (parent.is<ControlHandle>())
            parent.as<ControlHandle>().setContent(handle);
    }
    return handle;
}

void registerApi(sol::state& lua, Model& model)
{
    lua.new_usertype<ControlHandle>("OFSControl", sol::no_constructor,        //
                                    "size", &ControlHandle::size,             //
                                    "icon", &ControlHandle::icon,             //
                                    "flat", &ControlHandle::flat,             //
                                    "horizontal", &ControlHandle::horizontal, //
                                    "vertical", &ControlHandle::vertical,     //
                                    "align", &ControlHandle::align,           //
                                    "wordwrap", &ControlHandle::wordwrap,     //
                                    "layout", &ControlHandle::setContent);

    lua.new_usertype<LayoutHandle>("OFSLayout", sol::no_constructor,                                            //
                                   "margin", sol::property(&LayoutHandle::margin, &LayoutHandle::setMargin),    //
                                   "spacing", sol::property(&LayoutHandle::spacing, &LayoutHandle::setSpacing), //
                                   "addControl", &LayoutHandle::addControl,                                     //
                                   "addLayout", &LayoutHandle::addLayout,                                       //
                                   "addSpacing", &LayoutHandle::addSpacing,                                     //
                                   "addVerticalSpacer", &LayoutHandle::addVerticalSpacer,                       //
                                   "addHorizontalSpacer", &LayoutHandle::addHorizontalSpacer,                   //
                                   "addSeparator", &LayoutHandle::addSeparator);

    lua.new_usertype<ToolBarHandle>("OFSToolBar", sol::no_constructor,                                                //
                                    "iconSize", sol::property(&ToolBarHandle::iconSize, &ToolBarHandle::setIconSize), //
                                    "addButton", &ToolBarHandle::addButton,                                           //
                                    "addSeparator", &ToolBarHandle::addSeparator,                                     //
                                    "clear", &ToolBarHandle::clear);

    lua.new_usertype<PanelHandle>("OFSPanel", sol::no_constructor, //
                                  "layout", sol::property(&PanelHandle::layout, &PanelHandle::setLayout));

    sol::table api = lua.create_named_table("OFS");
    Model* m = &model;
    api.set_function("toolBar", [m](const std::string& name) {
        const QString n = QString::fromStdString(name);
        if (!m->toolBars.contains(n)) {
            m->toolBars[n].name = n;
            m->toolBarOrder << n;
        }
        return ToolBarHandle{m, n};
    });
    api.set_function("panel", [m](const std::string& name) {
        const QString n = QString::fromStdString(name);
        if (!m->panels.contains(n)) {
            m->panels[n].name = n;
            m->panelOrder << n;
        }
        return PanelHandle{m, n, false};
    });
    api.set_function("extraPanel", [m](const std::string& name) {
        const QString n = QString::fromStdString(name);
        if (!m->extraPanels.contains(n)) {
            m->extraPanels[n].name = n;
            m->extraPanelOrder << n;
        }
        return PanelHandle{m, n, true};
    });
    api.set_function("gridLayout", [](sol::variadic_args a) { return newLayout(Layout::Type::Grid, a); });
    api.set_function("horizontalLayout", [](sol::variadic_args a) { return newLayout(Layout::Type::Horizontal, a); });
    api.set_function("verticalLayout", [](sol::variadic_args a) { return newLayout(Layout::Type::Vertical, a); });
    api.set_function("translate", [](const std::string& text) { return text; });

    sol::table align = lua.create_table();
    const std::pair<const char*, int> flags[] = {
        {"Left", Qt::AlignLeft},     {"HCenter", Qt::AlignHCenter}, {"Right", Qt::AlignRight},
        {"Top", Qt::AlignTop},       {"VCenter", Qt::AlignVCenter}, {"Bottom", Qt::AlignBottom},
        {"Center", Qt::AlignCenter},
    };
    for (const auto& [name, value] : flags) {
        align[name] = value;
        api[name] = value;
    }
    api["Align"] = align;
    lua["FF"] = api;
}

QString takeGlobal(sol::state& lua, const char* name)
{
    sol::object value = lua[name];
    if (value.get_type() == sol::type::string)
        return QString::fromStdString(value.as<std::string>());
    return {};
}

} // namespace

bool PresetEngine::runFile(const QString& path, Model& model)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        model.errors << QStringLiteral("%1: %2").arg(path, file.errorString());
        return false;
    }
    return runString(QString::fromUtf8(file.readAll()), path, model);
}

bool PresetEngine::runString(const QString& source, const QString& chunkName, Model& model)
{
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math);
    registerApi(lua, model);

    const std::string code = source.toStdString();
    const std::string chunk = "@" + chunkName.toStdString();
    sol::protected_function_result result = lua.safe_script(code, sol::script_pass_on_error, chunk);
    if (!result.valid()) {
        const sol::error err = result;
        model.errors << QString::fromUtf8(err.what());
        return false;
    }

    sol::object apply = lua["apply"];
    if (apply.get_type() == sol::type::function) {
        sol::protected_function fn = apply;
        sol::protected_function_result r = fn();
        if (!r.valid()) {
            const sol::error err = r;
            model.errors << QString::fromUtf8(err.what());
            return false;
        }
    }

    if (QString v = takeGlobal(lua, "iconset"); !v.isEmpty())
        model.iconset = v;
    if (QString v = takeGlobal(lua, "stylesheet"); !v.isEmpty())
        model.stylesheet = v;
    if (QString v = takeGlobal(lua, "colorset"); !v.isEmpty())
        model.colorset = v;
    return true;
}

} // namespace OFS::Preset

namespace OFS::Preset {

QStringList Model::referencedNames() const
{
    QStringList names;
    for (const ToolBar& bar : toolBars) {
        for (const QString& action : bar.actions) {
            if (!action.isEmpty())
                names << action;
        }
    }
    std::function<void(const Layout&)> walk = [&](const Layout& layout) {
        for (const LayoutItem& item : layout.items) {
            if (item.kind == LayoutItem::Kind::Control && item.control) {
                names << item.control->name;
                if (item.control->content)
                    walk(*item.control->content);
            } else if (item.kind == LayoutItem::Kind::Layout && item.layout)
                walk(*item.layout);
        }
    };
    for (const Panel& panel : panels) {
        if (panel.layout)
            walk(*panel.layout);
    }
    for (const Panel& panel : extraPanels) {
        if (panel.layout)
            walk(*panel.layout);
    }
    names.removeDuplicates();
    return names;
}

} // namespace OFS::Preset
