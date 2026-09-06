#include "core/script/LuaApi.h"

#include "core/engine/Engine.h"
#include "core/engine/EngineManager.h"
#include "core/project/Project.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QRadioButton>
#include <QSpacerItem>
#include <QSpinBox>
#include <QWizard>
#include <QWizardPage>

#include <initializer_list>
#include <stdexcept>
#include <utility>

namespace OFS::Script {

Session::~Session()
{
    // Only delete top-level widgets, they take their children with them.
    for (const QPointer<QObject>& object : objects) {
        if (object && (!object->parent() || object->parent() == environment.parent))
            delete object.data();
    }
    for (const QPointer<Project>& project : projects)
        delete project.data();
}

Engine* findEngine(const Session& session, const QString& name)
{
    if (!session.environment.engines)
        return nullptr;
    for (Engine* engine : session.environment.engines->engines()) {
        if (engine->name().compare(name, Qt::CaseInsensitive) == 0)
            return engine;
    }
    return nullptr;
}

namespace {

QWidget* widgetOf(const sol::object& object)
{
    return object.valid() && object.is<QWidget*>() ? object.as<QWidget*>() : nullptr;
}

// Text from a string or a number (FF.radioButton(42, page) is accepted).
QString textOf(const sol::object& object)
{
    if (object.get_type() == sol::type::string)
        return fromStd(object.as<std::string>());
    if (object.get_type() == sol::type::number) {
        const double value = object.as<double>();
        return value == double(qint64(value)) ? QString::number(qint64(value)) : QString::number(value);
    }
    return QString();
}

template <typename T>
auto textProperty(QString (T::*get)() const, void (T::*set)(const QString&))
{
    return sol::property([get](T& object) { return toStd((object.*get)()); },
                         [set](T& object, const std::string& text) { (object.*set)(fromStd(text)); });
}

using EnumValues = std::initializer_list<std::pair<sol::string_view, int>>;

void addEnum(sol::table api, const char* name, EnumValues values)
{
    api.new_enum<int>(name, values);
}

void registerEnums(sol::table api)
{
    addEnum(api, "Align", {{"Left", 1}, {"Right", 2}, {"HCenter", 4}, {"Top", 32}, {"Bottom", 64}, {"VCenter", 128}});
    addEnum(api, "FileType", {{"Main", 0}, {"Sprites", 1}, {"Sounds", 2}, {"Animations", 3}, {"Scripts", 4}});
    addEnum(api, "Filter",
            {{"Dirs", 1},
             {"Files", 2},
             {"AllEntries", 7},
             {"NoSymLinks", 8},
             {"AllDirs", 1024},
             {"CaseSensitive", 2048},
             {"NoFilter", -1}});
    addEnum(api, "SortFlag",
            {{"Name", 0},
             {"Time", 1},
             {"Size", 2},
             {"DirsFirst", 4},
             {"Reversed", 8},
             {"IgnoreCase", 16},
             {"DirsLast", 32},
             {"Type", 128},
             {"NoSort", -1}});
    addEnum(api, "OpenFlags", {{"ReadOnly", 1}, {"WriteOnly", 2}, {"ReadWrite", 3}});
    addEnum(api, "OpenMode", {{"ReadOnly", 1}, {"WriteOnly", 2}, {"ReadWrite", 3}});
    addEnum(api, "ShareMode", {{"NoShare", 0}, {"Share", 1}, {"AskToShare", 2}});
    addEnum(api, "Flip", {{"None", 0}, {"Horizontal", 1}, {"Vertical", 2}, {"Both", 3}});
    addEnum(api, "Mode",
            {{"None", 0},
             {"Addition", 1},
             {"Subtraction", 2},
             {"Overlay", 3},
             {"HardLight", 4},
             {"Dodge", 5},
             {"Opacity", 6}});
    addEnum(api, "PaletteMode", {{"Ignore", 0}, {"OwnPalette", 1}, {"AdaptImage", 2}, {"SwapPalette", 3}});
    addEnum(api, "Type", {{"Attack", 1}, {"Collision", 2}});
    addEnum(api, "MoveMode", {{"MoveAnchor", 0}, {"KeepAnchor", 1}});
    addEnum(api, "MoveOperation",
            {{"NoMove", 0},
             {"Start", 1},
             {"StartOfLine", 3},
             {"StartOfWord", 5},
             {"PreviousLine", 6},
             {"PreviousCharacter", 7},
             {"PreviousWord", 8},
             {"End", 11},
             {"EndOfWord", 14},
             {"EndOfLine", 15},
             {"NextLine", 16},
             {"NextCharacter", 17},
             {"NextWord", 18}});
    addEnum(api, "SelectionType", {{"WordUnderCursor", 0}, {"LineUnderCursor", 1}, {"Document", 3}});
}

template <typename Button>
void registerButton(sol::table api, const char* name)
{
    api.new_usertype<Button>(name, sol::no_constructor,                                               //
                             sol::base_classes, sol::bases<QAbstractButton, QWidget>(),               //
                             "text", textProperty(&QAbstractButton::text, &QAbstractButton::setText), //
                             "checked", sol::property(&QAbstractButton::isChecked, &QAbstractButton::setChecked));
}

void addSpacer(QLayout& layout, bool vertical)
{
    auto* spacer = vertical ? new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding)
                            : new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    if (auto* box = qobject_cast<QBoxLayout*>(&layout))
        box->addSpacerItem(spacer);
    else
        layout.addItem(spacer);
}

void registerWidgets(sol::table api)
{
    api.new_usertype<QWidget>("Widget", sol::no_constructor,                                       //
                              "enabled", sol::property(&QWidget::isEnabled, &QWidget::setEnabled), //
                              "visible", sol::property(&QWidget::isVisible, &QWidget::setVisible), //
                              "toolTip", textProperty(&QWidget::toolTip, &QWidget::setToolTip),    //
                              "windowTitle", textProperty(&QWidget::windowTitle, &QWidget::setWindowTitle));
    api.new_usertype<QDialog>("Dialog", sol::no_constructor, sol::base_classes, sol::bases<QWidget>(), //
                              "exec", [](QDialog& dialog) { return dialog.exec(); });
    api.new_usertype<QWizard>(
        "Wizard", sol::no_constructor, sol::base_classes, sol::bases<QDialog, QWidget>(),               //
        "addPage", [](QWizard& wizard, QWizardPage* page) { return page ? wizard.addPage(page) : -1; }, //
        "exec", [](QWizard& wizard) { return wizard.exec(); });
    api.new_usertype<QWizardPage>("WizardPage", sol::no_constructor,                                  //
                                  sol::base_classes, sol::bases<QWidget>(),                           //
                                  "title", textProperty(&QWizardPage::title, &QWizardPage::setTitle), //
                                  "subTitle", textProperty(&QWizardPage::subTitle, &QWizardPage::setSubTitle));
    registerButton<QRadioButton>(api, "RadioButton");
    registerButton<QCheckBox>(api, "CheckBox");
    api.new_usertype<QComboBox>(
        "ComboBox", sol::no_constructor, sol::base_classes, sol::bases<QWidget>(), //
        "addItem",
        [](QComboBox& c, const sol::object& text, sol::optional<std::string> data) {
            c.addItem(textOf(text), data ? QVariant(fromStd(*data)) : QVariant());
        },
        "count", sol::property(&QComboBox::count),                                            //
        "currentIndex", sol::property(&QComboBox::currentIndex, &QComboBox::setCurrentIndex), //
        "currentText", sol::property([](QComboBox& c) { return toStd(c.currentText()); }),    //
        "currentData", sol::property([](QComboBox& c) { return toStd(c.currentData().toString()); }));

    // Not in FFS: extra widgets for script dialogs.
    api.new_usertype<QLabel>("Label", sol::no_constructor, sol::base_classes, sol::bases<QWidget>(), //
                             "text", textProperty(&QLabel::text, &QLabel::setText));
    api.new_usertype<QLineEdit>("LineEdit", sol::no_constructor,                             //
                                sol::base_classes, sol::bases<QWidget>(),                    //
                                "text", textProperty(&QLineEdit::text, &QLineEdit::setText), //
                                "placeholderText",
                                textProperty(&QLineEdit::placeholderText, &QLineEdit::setPlaceholderText));
    api.new_usertype<QSpinBox>("SpinBox", sol::no_constructor, sol::base_classes, sol::bases<QWidget>(), //
                               "value", sol::property(&QSpinBox::value, &QSpinBox::setValue),            //
                               "minimum", sol::property(&QSpinBox::minimum, &QSpinBox::setMinimum),      //
                               "maximum", sol::property(&QSpinBox::maximum, &QSpinBox::setMaximum));

    api.new_usertype<QLayout>(
        "Layout", sol::no_constructor,                                     //
        "spacing", sol::property(&QLayout::spacing, &QLayout::setSpacing), //
        "margin",
        sol::property([](QLayout& l) { return l.contentsMargins().left(); },
                      [](QLayout& l, int margin) { l.setContentsMargins(margin, margin, margin, margin); }),
        "margins",
        sol::property(&QLayout::contentsMargins, [](QLayout& l, const QMargins& m) { l.setContentsMargins(m); }));
    api.new_usertype<QBoxLayout>(
        "BoxLayout", sol::no_constructor, sol::base_classes, sol::bases<QLayout>(), //
        "addWidget", [](QBoxLayout& l, QWidget* widget) { l.addWidget(widget); },   //
        "addLayout", [](QBoxLayout& l, QLayout* child) { l.addLayout(child); },     //
        "addSpacing", [](QBoxLayout& l, int size) { l.addSpacing(size); },          //
        "addStretch", [](QBoxLayout& l) { l.addStretch(); },                        //
        "addVerticalSpacer", [](QBoxLayout& l) { addSpacer(l, true); },             //
        "addHorizontalSpacer", [](QBoxLayout& l) { addSpacer(l, false); });
    api.new_usertype<QVBoxLayout>("VerticalLayout", sol::no_constructor, sol::base_classes,
                                  sol::bases<QBoxLayout, QLayout>());
    api.new_usertype<QHBoxLayout>("HorizontalLayout", sol::no_constructor, sol::base_classes,
                                  sol::bases<QBoxLayout, QLayout>());
    // addWidget(widget, row, column[, rowSpan, columnSpan]), and the same for layouts and spacers.
    api.new_usertype<QGridLayout>(
        "GridLayout", sol::no_constructor, sol::base_classes, sol::bases<QLayout>(), //
        "addWidget",
        [](QGridLayout& l, QWidget* widget, int row, int column, sol::optional<int> rows, sol::optional<int> columns) {
            l.addWidget(widget, row, column, rows.value_or(1), columns.value_or(1));
        },
        "addLayout",
        [](QGridLayout& l, QLayout* child, int row, int column, sol::optional<int> rows, sol::optional<int> columns) {
            l.addLayout(child, row, column, rows.value_or(1), columns.value_or(1));
        },
        "addVerticalSpacer",
        [](QGridLayout& l, int row, int column) {
            l.addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding), row, column);
        },
        "addHorizontalSpacer",
        [](QGridLayout& l, int row, int column) {
            l.addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum), row, column);
        });
}

QString projectTypeOf(const Engine& engine, const sol::object& type)
{
    if (type.get_type() == sol::type::number)
        return engine.scriptProjectTypeId(type.as<int>());
    if (type.get_type() == sol::type::string)
        return fromStd(type.as<std::string>());
    return QString();
}

Project* adoptProject(Session& session, Project* project)
{
    if (session.environment.openProject)
        session.environment.openProject(project);
    else
        session.projects.emplace_back(project);
    return project;
}

} // namespace

void registerApi(sol::state& lua, Session& session)
{
    Session* s = &session;
    sol::table api = lua.create_named_table("FF");
    lua["OFS"] = api;
    registerEnums(api);
    registerWidgets(api);
    registerFileApi(lua, api);
    registerProjectApi(lua, api, session);

    // Widgets without a parent belong to the run and go away with it.
    api.set_function("wizard", [s](sol::object parent) {
        QWidget* owner = widgetOf(parent);
        return s->track(new QWizard(owner ? owner : s->environment.parent));
    });
    api.set_function("wizardPage", [s](sol::object parent) { return s->track(new QWizardPage(widgetOf(parent))); });
    api.set_function("radioButton", [s](sol::object text, sol::object parent) {
        return s->track(new QRadioButton(textOf(text), widgetOf(parent)));
    });
    api.set_function("checkBox", [s](sol::object text, sol::object parent) {
        return s->track(new QCheckBox(textOf(text), widgetOf(parent)));
    });
    api.set_function("comboBox", [s](sol::object parent) { return s->track(new QComboBox(widgetOf(parent))); });
    // A layout given a parent widget becomes that widget's layout.
    api.set_function("verticalLayout", [s](sol::object parent) { return s->track(new QVBoxLayout(widgetOf(parent))); });
    api.set_function("horizontalLayout",
                     [s](sol::object parent) { return s->track(new QHBoxLayout(widgetOf(parent))); });
    api.set_function("gridLayout", [s](sol::object parent) { return s->track(new QGridLayout(widgetOf(parent))); });
    // Not in FFS.
    api.set_function("label", [s](sol::object text, sol::object parent) {
        return s->track(new QLabel(textOf(text), widgetOf(parent)));
    });
    api.set_function("lineEdit", [s](sol::object parent) { return s->track(new QLineEdit(widgetOf(parent))); });
    api.set_function("spinBox", [s](sol::object parent) { return s->track(new QSpinBox(widgetOf(parent))); });
    api.set_function("messageBox", [s](sol::object text, sol::optional<std::string> title) {
        QMessageBox::information(s->environment.parent, fromStd(title.value_or(std::string())), textOf(text));
    });

    api.set_function("translate", [s](sol::optional<std::string> text) {
        if (!text)
            return std::string();
        const QByteArray context = s->context.toUtf8();
        return toStd(QCoreApplication::translate(context.constData(), text->c_str()));
    });

    api.set_function("emptyProject", [s](sol::object engineName, sol::object type) -> Project* {
        if (engineName.get_type() != sol::type::string)
            return nullptr;
        Engine* engine = findEngine(*s, fromStd(engineName.as<std::string>()));
        if (!engine)
            return nullptr;
        const QString typeId = projectTypeOf(*engine, type);
        if (typeId.isEmpty())
            throw std::runtime_error("unknown project type");
        QString error;
        Project* project = engine->newProject(typeId, s->environment.projectFolder, &error);
        if (!project)
            throw std::runtime_error(toStd(error));
        return adoptProject(*s, project);
    });
    api.set_function("projectFromFile", [s](const std::string& fileName, sol::optional<bool> asCopy) -> Project* {
        QString path = QDir::fromNativeSeparators(fromStd(fileName));
        if (QDir::isRelativePath(path) && !s->scriptPath.isEmpty())
            path = QDir(s->scriptPath).filePath(path);
        if (!s->environment.engines)
            return nullptr;
        for (Engine* engine : s->environment.engines->engines()) {
            if (!engine->canOpen(path))
                continue;
            QString error;
            Project* project = engine->openProject(path, &error);
            if (!project)
                throw std::runtime_error(toStd(error));
            if (asCopy.value_or(false))
                project->openAsCopy();
            return adoptProject(*s, project);
        }
        return nullptr;
    });
    api.set_function("projectCount",
                     [s] { return s->environment.projects ? int(s->environment.projects().size()) : 0; });
    api.set_function("projectAt", [s](int index) -> Project* {
        const QList<Project*> projects = s->environment.projects ? s->environment.projects() : QList<Project*>();
        return index >= 0 && index < projects.size() ? projects[index] : nullptr;
    });
    api.set_function("currentProject", [s]() -> Project* {
        return s->environment.currentProject ? s->environment.currentProject() : nullptr;
    });
    api.set_function("currentEngine", [s]() -> Engine* {
        Project* project = s->environment.currentProject ? s->environment.currentProject() : nullptr;
        return project ? project->engine() : nullptr;
    });
    api.set_function("engineList", [s](sol::this_state state) {
        sol::state_view view(state);
        sol::table list = view.create_table();
        if (s->environment.engines) {
            for (Engine* engine : s->environment.engines->engines())
                list[toStd(engine->name())] = engine;
        }
        return list;
    });
    api.set_function("engineByName", [s](const std::string& name) -> Engine* { return findEngine(*s, fromStd(name)); });

    if (s->environment.engines) {
        for (Engine* engine : s->environment.engines->engines()) {
            sol::table table = lua.create_named_table(toStd(engine->scriptName()));
            const QMap<QString, QVariantMap> enums = engine->scriptEnums();
            for (auto it = enums.cbegin(); it != enums.cend(); ++it) {
                std::vector<std::pair<std::string, int>> values;
                for (auto value = it.value().cbegin(); value != it.value().cend(); ++value)
                    values.emplace_back(toStd(value.key()), value.value().toInt());
                sol::table entries = lua.create_table();
                for (const auto& [key, number] : values)
                    entries[key] = number;
                sol::table guard = lua.create_table();
                sol::table meta = lua.create_table();
                meta["__index"] = entries;
                meta["__newindex"] = [](sol::this_state state) {
                    luaL_error(state, "sol: cannot modify the elements of an enumeration table");
                };
                meta["__pairs"] = [entries](sol::this_state state) {
                    sol::state_view view(state);
                    return std::make_tuple(view["next"], entries, sol::lua_nil);
                };
                guard[sol::metatable_key] = meta;
                table[toStd(it.key())] = guard;
            }
        }
    }
}

} // namespace OFS::Script
