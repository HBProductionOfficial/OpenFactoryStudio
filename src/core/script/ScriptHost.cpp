#include "core/script/ScriptHost.h"

#include "core/engine/Engine.h"
#include "core/engine/EngineManager.h"
#include "core/project/Project.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QRadioButton>
#include <QSpinBox>
#include <QWizard>
#include <QWizardPage>

#include <sol/sol.hpp>

#include <stdexcept>
#include <vector>

namespace OFS::Script {

namespace {

std::string toStd(const QString& text)
{
    return text.toStdString();
}

QString fromStd(const std::string& text)
{
    return QString::fromStdString(text);
}

struct Session
{
    Environment environment;
    QString scriptPath;
    QString context;
    std::vector<QPointer<QObject>> objects;
    std::vector<QPointer<Project>> projects;

    template <typename T>
    T* track(T* object)
    {
        objects.emplace_back(object);
        return object;
    }

    ~Session()
    {
        for (const QPointer<QObject>& object : objects) {
            if (object && (!object->parent() || object->parent() == environment.parent))
                delete object.data();
        }
        for (const QPointer<Project>& project : projects)
            delete project.data();
    }
};

QWidget* widgetOf(const sol::object& object)
{
    return object.valid() && object.is<QWidget*>() ? object.as<QWidget*>() : nullptr;
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

template <typename T>
auto textProperty(QString (T::*get)() const, void (T::*set)(const QString&))
{
    return sol::property([get](T& object) { return toStd((object.*get)()); },
                         [set](T& object, const std::string& text) { (object.*set)(fromStd(text)); });
}

template <typename Button>
void registerButton(sol::state& lua, const char* name)
{
    lua.new_usertype<Button>(name, sol::no_constructor,                                               //
                             sol::base_classes, sol::bases<QAbstractButton, QWidget>(),               //
                             "text", textProperty(&QAbstractButton::text, &QAbstractButton::setText), //
                             "checked", sol::property(&QAbstractButton::isChecked, &QAbstractButton::setChecked));
}

void registerWidgets(sol::state& lua)
{
    lua.new_usertype<QWidget>("Widget", sol::no_constructor,                                       //
                              "enabled", sol::property(&QWidget::isEnabled, &QWidget::setEnabled), //
                              "visible", sol::property(&QWidget::isVisible, &QWidget::setVisible), //
                              "toolTip", textProperty(&QWidget::toolTip, &QWidget::setToolTip),    //
                              "windowTitle", textProperty(&QWidget::windowTitle, &QWidget::setWindowTitle));

    lua.new_usertype<QWizard>(
        "Wizard", sol::no_constructor, sol::base_classes, sol::bases<QWidget>(),                        //
        "addPage", [](QWizard& wizard, QWizardPage* page) { return page ? wizard.addPage(page) : -1; }, //
        "exec", [](QWizard& wizard) { return wizard.exec(); });

    lua.new_usertype<QWizardPage>("WizardPage", sol::no_constructor,                                  //
                                  sol::base_classes, sol::bases<QWidget>(),                           //
                                  "title", textProperty(&QWizardPage::title, &QWizardPage::setTitle), //
                                  "subTitle", textProperty(&QWizardPage::subTitle, &QWizardPage::setSubTitle));

    registerButton<QRadioButton>(lua, "RadioButton");
    registerButton<QCheckBox>(lua, "CheckBox");

    lua.new_usertype<QLabel>("Label", sol::no_constructor, sol::base_classes, sol::bases<QWidget>(), //
                             "text", textProperty(&QLabel::text, &QLabel::setText));

    lua.new_usertype<QLineEdit>("LineEdit", sol::no_constructor,                             //
                                sol::base_classes, sol::bases<QWidget>(),                    //
                                "text", textProperty(&QLineEdit::text, &QLineEdit::setText), //
                                "placeholderText",
                                textProperty(&QLineEdit::placeholderText, &QLineEdit::setPlaceholderText));

    lua.new_usertype<QComboBox>(
        "ComboBox", sol::no_constructor, sol::base_classes, sol::bases<QWidget>(),            //
        "addItem", [](QComboBox& c, const std::string& text) { c.addItem(fromStd(text)); },   //
        "count", sol::property(&QComboBox::count),                                            //
        "currentIndex", sol::property(&QComboBox::currentIndex, &QComboBox::setCurrentIndex), //
        "currentText", sol::property([](QComboBox& c) { return toStd(c.currentText()); }));

    lua.new_usertype<QSpinBox>("SpinBox", sol::no_constructor, sol::base_classes, sol::bases<QWidget>(), //
                               "value", sol::property(&QSpinBox::value, &QSpinBox::setValue),            //
                               "minimum", sol::property(&QSpinBox::minimum, &QSpinBox::setMinimum),      //
                               "maximum", sol::property(&QSpinBox::maximum, &QSpinBox::setMaximum));

    lua.new_usertype<QLayout>(
        "Layout", sol::no_constructor,                                     //
        "spacing", sol::property(&QLayout::spacing, &QLayout::setSpacing), //
        "margin",
        sol::property([](QLayout& l) { return l.contentsMargins().left(); },
                      [](QLayout& l, int margin) { l.setContentsMargins(margin, margin, margin, margin); }));
    lua.new_usertype<QBoxLayout>(
        "BoxLayout", sol::no_constructor, sol::base_classes, sol::bases<QLayout>(), //
        "addWidget", [](QBoxLayout& l, QWidget* widget) { l.addWidget(widget); },   //
        "addLayout", [](QBoxLayout& l, QLayout* child) { l.addLayout(child); },     //
        "addStretch", [](QBoxLayout& l) { l.addStretch(); },                        //
        "addSpacing", [](QBoxLayout& l, int size) { l.addSpacing(size); });
    lua.new_usertype<QGridLayout>(
        "GridLayout", sol::no_constructor, sol::base_classes, sol::bases<QLayout>(), //
        "addWidget",
        [](QGridLayout& l, QWidget* widget, int row, int column, sol::optional<int> rows, sol::optional<int> columns) {
            l.addWidget(widget, row, column, rows.value_or(1), columns.value_or(1));
        },
        "addLayout",
        [](QGridLayout& l, QLayout* child, int row, int column, sol::optional<int> rows, sol::optional<int> columns) {
            l.addLayout(child, row, column, rows.value_or(1), columns.value_or(1));
        });
}

void registerApi(sol::state& lua, Session& session)
{
    Session* s = &session;
    registerWidgets(lua);

    lua.new_usertype<Project>("Project", sol::no_constructor,                                                //
                              "name", sol::property([](Project& p) { return toStd(p.name()); }),             //
                              "fileName", sol::property([](Project& p) { return toStd(p.filePath()); }),     //
                              "type", sol::property([](Project& p) { return toStd(p.typeId()); }),           //
                              "engine", sol::property([](Project& p) { return toStd(p.engine()->name()); }), //
                              "isModified", sol::property([](Project& p) { return p.isModified(); }));

    sol::table api = lua.create_named_table("FF");
    lua["OFS"] = api;

    api.set_function("wizard", [s](sol::object parent) {
        QWidget* owner = widgetOf(parent);
        return s->track(new QWizard(owner ? owner : s->environment.parent));
    });
    api.set_function("wizardPage", [s](sol::object parent) { return s->track(new QWizardPage(widgetOf(parent))); });
    api.set_function("radioButton", [s](const std::string& text, sol::object parent) {
        return s->track(new QRadioButton(fromStd(text), widgetOf(parent)));
    });
    api.set_function("checkBox", [s](const std::string& text, sol::object parent) {
        return s->track(new QCheckBox(fromStd(text), widgetOf(parent)));
    });
    api.set_function("label", [s](const std::string& text, sol::object parent) {
        return s->track(new QLabel(fromStd(text), widgetOf(parent)));
    });
    api.set_function("lineEdit", [s](sol::object parent) { return s->track(new QLineEdit(widgetOf(parent))); });
    api.set_function("comboBox", [s](sol::object parent) { return s->track(new QComboBox(widgetOf(parent))); });
    api.set_function("spinBox", [s](sol::object parent) { return s->track(new QSpinBox(widgetOf(parent))); });
    api.set_function("verticalLayout", [s](sol::object parent) {
        return s->track(new QBoxLayout(QBoxLayout::TopToBottom, widgetOf(parent)));
    });
    api.set_function("horizontalLayout", [s](sol::object parent) {
        return s->track(new QBoxLayout(QBoxLayout::LeftToRight, widgetOf(parent)));
    });
    api.set_function("gridLayout", [s](sol::object parent) { return s->track(new QGridLayout(widgetOf(parent))); });

    api.set_function("messageBox", [s](const std::string& text, sol::optional<std::string> title) {
        QMessageBox::information(s->environment.parent, fromStd(title.value_or(std::string())), fromStd(text));
    });
    api.set_function("translate", [s](const std::string& text) {
        const QByteArray context = s->context.toUtf8();
        return toStd(QCoreApplication::translate(context.constData(), text.c_str()));
    });

    api.set_function("emptyProject", [s](const std::string& engineName, const std::string& type) {
        Engine* engine = findEngine(*s, fromStd(engineName));
        if (!engine)
            throw std::runtime_error("unknown engine '" + engineName + "'");
        QString error;
        Project* project = engine->newProject(fromStd(type), s->environment.projectFolder, &error);
        if (!project)
            throw std::runtime_error(toStd(error));
        s->projects.emplace_back(project);
        return project;
    });
    api.set_function("projectFromFile", [s](const std::string& fileName, sol::optional<bool> asCopy) -> Project* {
        QString path = QDir::fromNativeSeparators(fromStd(fileName));
        if (QDir::isRelativePath(path))
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
            s->projects.emplace_back(project);
            return project;
        }
        return nullptr;
    });

    if (s->environment.engines) {
        for (Engine* engine : s->environment.engines->engines()) {
            sol::table table = lua.create_named_table(toStd(engine->scriptName()));
            const QMap<QString, QVariantMap> enums = engine->scriptEnums();
            for (auto it = enums.cbegin(); it != enums.cend(); ++it) {
                sol::table values = lua.create_table();
                for (auto value = it.value().cbegin(); value != it.value().cend(); ++value) {
                    if (value.value().typeId() == QMetaType::Int)
                        values[toStd(value.key())] = value.value().toInt();
                    else
                        values[toStd(value.key())] = toStd(value.value().toString());
                }
                table[toStd(it.key())] = values;
            }
        }
    }
}

} // namespace

Project* runProjectTemplate(const QString& path, const Environment& environment, QString* error)
{
    auto fail = [error](const QString& message) -> Project* {
        if (error)
            *error = message;
        return nullptr;
    };
    if (error)
        error->clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("%1: %2").arg(QDir::toNativeSeparators(path), file.errorString()));

    Session session;
    session.environment = environment;
    session.scriptPath = QFileInfo(path).absolutePath();
    session.context = QFileInfo(path).completeBaseName();

    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math, sol::lib::utf8);
    registerApi(lua, session);
    lua["ScriptPath"] = toStd(session.scriptPath);

    const std::string code = file.readAll().toStdString();
    const std::string chunk = "@" + toStd(QDir::toNativeSeparators(path));
    sol::protected_function_result result = lua.safe_script(code, sol::script_pass_on_error, chunk);
    if (!result.valid()) {
        const sol::error err = result;
        return fail(QString::fromUtf8(err.what()));
    }
    const sol::object value = result;
    if (value.get_type() == sol::type::lua_nil || value.get_type() == sol::type::none)
        return nullptr;
    if (!value.is<Project*>())
        return fail(QStringLiteral("%1: the script did not return a project").arg(QDir::toNativeSeparators(path)));
    Project* project = value.as<Project*>();
    for (QPointer<Project>& owned : session.projects) {
        if (owned == project)
            owned.clear();
    }
    return project;
}

} // namespace OFS::Script
