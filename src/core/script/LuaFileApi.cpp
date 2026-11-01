#include "core/project/Archives.h"
#include "core/script/LuaApi.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMargins>
#include <QRegularExpression>

#include <algorithm>
#include <memory>

namespace OFS::Script {

namespace {

using Rgb = ScriptRgb;

struct Rgba
{
    // Lua builds it with parentheses, which older compilers only allow with a constructor.
    Rgba() = default;
    Rgba(int red, int green, int blue, int alpha)
        : r(red)
        , g(green)
        , b(blue)
        , a(alpha)
    {}
    int r = 0;
    int g = 0;
    int b = 0;
    int a = 0;
};

struct File
{
    std::shared_ptr<QIODevice> device;
};

using Archive = ScriptArchive;

QList<Zip::Entry> zipEntries(const Archive& archive)
{
    return Archives::entries(archive.root);
}

QStringList stringsOf(const sol::object& object)
{
    QStringList list;
    if (object.get_type() == sol::type::table) {
        const sol::table table = object.as<sol::table>();
        for (const auto& [key, value] : table) {
            if (value.get_type() == sol::type::string)
                list << fromStd(value.as<std::string>());
        }
    } else if (object.get_type() == sol::type::string) {
        list << fromStd(object.as<std::string>());
    }
    return list;
}

sol::table tableOf(sol::this_state state, const QStringList& list)
{
    sol::state_view view(state);
    sol::table table = view.create_table(int(list.size()), 0);
    for (int i = 0; i < list.size(); ++i)
        table[i + 1] = toStd(list[i]);
    return table;
}

bool wildcardMatch(const QStringList& patterns, const QString& name)
{
    if (patterns.isEmpty())
        return true;
    for (const QString& pattern : patterns) {
        const QRegularExpression re(QRegularExpression::wildcardToRegularExpression(pattern),
                                    QRegularExpression::CaseInsensitiveOption);
        if (re.match(name).hasMatch())
            return true;
    }
    return false;
}

QStringList listEntries(const Archive& archive, const QString& path, const QStringList& patterns, int filters, int sort)
{
    const QDir::Filters dirFilters = filters < 0 ? QDir::Filters(QDir::NoFilter) : QDir::Filters(filters);
    const QDir::SortFlags sortFlags = sort < 0 ? QDir::SortFlags(QDir::NoSort) : QDir::SortFlags(sort);
    if (!archive.zip) {
        QDir dir(archive.onDisk(path));
        return dir.entryList(patterns, dirFilters | QDir::NoDotAndDotDot, sortFlags);
    }
    const QString prefix = Archive::entryName(path).isEmpty() ? QString() : Archive::entryName(path) + QLatin1Char('/');
    const bool wantDirs = filters < 0 || (filters & QDir::Dirs) || (filters & QDir::AllDirs);
    const bool wantFiles = filters < 0 || (filters & QDir::Files);
    QStringList dirs;
    QStringList files;
    for (const Zip::Entry& entry : zipEntries(archive)) {
        if (!entry.name.startsWith(prefix))
            continue;
        QString rest = entry.name.mid(prefix.size());
        const bool isDir = entry.directory || rest.contains(QLatin1Char('/'));
        rest = rest.section(QLatin1Char('/'), 0, 0);
        if (rest.isEmpty() || !wildcardMatch(patterns, rest))
            continue;
        QStringList& into = isDir ? dirs : files;
        if (!into.contains(rest) && (isDir ? wantDirs : wantFiles))
            into << rest;
    }
    QStringList all = dirs + files;
    if (sort >= 0 && !(sort & QDir::Unsorted)) {
        std::sort(all.begin(), all.end(),
                  [](const QString& a, const QString& b) { return a.compare(b, Qt::CaseInsensitive) < 0; });
        if (sort & QDir::Reversed)
            std::reverse(all.begin(), all.end());
    }
    return all;
}

} // namespace

void registerFileApi(sol::state& lua, sol::table api)
{
    // Small values.
    api.new_usertype<Rgb>("RGB", sol::constructors<Rgb(), Rgb(int, int, int)>(), //
                          "r", &Rgb::r, "g", &Rgb::g, "b", &Rgb::b,              //
                          "red", &Rgb::r, "green", &Rgb::g, "blue", &Rgb::b,     //
                          sol::meta_function::to_string, [](const Rgb& c) {
                              return "RGB(" + std::to_string(c.r) + ", " + //
                                     std::to_string(c.g) + ", " + std::to_string(c.b) + ")";
                          });
    api.new_usertype<Rgba>(
        "RGBA", sol::constructors<Rgba(), Rgba(int, int, int, int)>(),           //
        "r", &Rgba::r, "g", &Rgba::g, "b", &Rgba::b, "a", &Rgba::a,              //
        "red", &Rgba::r, "green", &Rgba::g, "blue", &Rgba::b, "alpha", &Rgba::a, //
        "toRGB", [](const Rgba& c) { return Rgb{c.r, c.g, c.b}; },               //
        "fromRGB",
        [](Rgba& c, const Rgb& rgb) {
            c.r = rgb.r;
            c.g = rgb.g;
            c.b = rgb.b;
        });
    api.new_usertype<QMargins>("Margins", sol::constructors<QMargins(), QMargins(int, int, int, int)>(), //
                               "left", sol::property(&QMargins::left, &QMargins::setLeft),               //
                               "top", sol::property(&QMargins::top, &QMargins::setTop),                  //
                               "right", sol::property(&QMargins::right, &QMargins::setRight),            //
                               "bottom", sol::property(&QMargins::bottom, &QMargins::setBottom));

    // Lists are plain 1-based Lua tables. Flags are integers combined with |.
    sol::table stringList = lua.create_table();
    stringList["new"] = [](sol::variadic_args args, sol::this_state state) {
        QStringList items;
        for (const sol::object arg : args) {
            for (const QString& item : stringsOf(arg))
                items << item;
        }
        return tableOf(state, items);
    };
    api["StringList"] = stringList;
    for (const char* flags : {"Filters", "SortFlags"}) {
        sol::table type = lua.create_table();
        type["new"] = [](sol::optional<int> value) {
            return value.value_or(0);
        };
        api[flags] = type;
    }

    api.new_usertype<File>(
        "File", sol::no_constructor,                                                                 //
        "pos", sol::property([](File& f) { return f.device ? qint64(f.device->pos()) : 0; }),        //
        "size", sol::property([](File& f) { return f.device ? qint64(f.device->size()) : 0; }),      //
        "atEnd", sol::property([](File& f) { return !f.device || f.device->atEnd(); }),              //
        "isOpen", sol::property([](File& f) { return f.device && f.device->isOpen(); }),             //
        "isSequential", sol::property([](File& f) { return f.device && f.device->isSequential(); }), //
        "read",
        [](File& f, sol::optional<qint64> count) {
            if (!f.device)
                return std::string();
            const QByteArray data = count ? f.device->read(*count) : f.device->readAll();
            return data.toStdString();
        },
        "readAll", [](File& f) { return f.device ? f.device->readAll().toStdString() : std::string(); },   //
        "readLine", [](File& f) { return f.device ? f.device->readLine().toStdString() : std::string(); }, //
        "write",
        [](File& f, const std::string& data) {
            return f.device ? qint64(f.device->write(data.data(), qint64(data.size()))) : qint64(-1);
        },
        "seek", [](File& f, qint64 pos) { return f.device && f.device->seek(pos); }, //
        "reset", [](File& f) { return f.device && f.device->reset(); },              //
        "close",
        [](File& f) {
            if (f.device)
                f.device->close();
        });

    api.new_usertype<Archive>(
        "Archive", sol::no_constructor, //
        // instance(fileName, readOnly): the archive (ZIP, RAR...), the folder, or the folder of a file.
        "instance",
        [](const std::string& fileName, sol::optional<bool> readOnly) {
            Archive archive;
            const QFileInfo info(QDir::fromNativeSeparators(fromStd(fileName)));
            if (info.isFile() && Archives::formatOf(info.filePath())) {
                archive.root = info.absoluteFilePath();
                archive.zip = true;
                archive.readOnly = true;
            } else {
                archive.root = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
                archive.readOnly = readOnly.value_or(false);
            }
            return archive;
        },
        "name", sol::property([](Archive& a) { return toStd(QFileInfo(a.root).completeBaseName()); }), //
        "path", sol::property([](Archive& a) { return toStd(a.folder()); }),                           //
        "isReadOnly", [](Archive& a) { return a.readOnly; },                                           //
        "absoluteFilePath",
        [](Archive& a, const std::string& name) {
            return toStd(QFileInfo(a.zip ? a.folder() + QLatin1Char('/') + fromStd(name) : a.onDisk(fromStd(name)))
                             .absolutePath());
        },
        "absoluteFileName",
        [](Archive& a, const std::string& name) {
            return toStd(a.zip ? QDir::cleanPath(a.folder() + QLatin1Char('/') + fromStd(name))
                               : a.onDisk(fromStd(name)));
        },
        "fileExists",
        [](Archive& a, const std::string& name) {
            if (!a.zip)
                return QFileInfo(a.onDisk(fromStd(name))).isFile();
            const QString entry = Archive::entryName(fromStd(name));
            const QList<Zip::Entry> entries = zipEntries(a);
            return std::any_of(entries.cbegin(), entries.cend(),
                               [&entry](const Zip::Entry& e) { return !e.directory && e.name == entry; });
        },
        "directoryExists",
        [](Archive& a, const std::string& name) {
            if (!a.zip)
                return QFileInfo(a.onDisk(fromStd(name))).isDir();
            const QString entry = Archive::entryName(fromStd(name));
            if (entry.isEmpty())
                return true;
            const QList<Zip::Entry> entries = zipEntries(a);
            return std::any_of(entries.cbegin(), entries.cend(), [&entry](const Zip::Entry& e) {
                return e.name == entry || e.name.startsWith(entry + QLatin1Char('/'));
            });
        },
        "isWritable",
        [](Archive& a, const std::string& name) {
            if (a.zip || a.readOnly)
                return false;
            const QFileInfo info(a.onDisk(fromStd(name)));
            return info.exists() ? info.isWritable() : QFileInfo(info.absolutePath()).isWritable();
        },
        // entries(path, nameFilters, filters, sort): the names under path.
        "entries",
        [](Archive& a, sol::optional<std::string> path, sol::object nameFilters, sol::optional<int> filters,
           sol::optional<int> sort, sol::this_state state) {
            return tableOf(state, listEntries(a, fromStd(path.value_or(std::string())), stringsOf(nameFilters),
                                              filters.value_or(-1), sort.value_or(0)));
        },
        "openFile",
        [](Archive& a, const std::string& name, sol::optional<int> mode) -> sol::optional<File> {
            const int flags = mode.value_or(1);
            if (a.zip) {
                if (flags & 2)
                    return sol::nullopt;
                QString error;
                const QByteArray data = Archives::read(a.root, Archive::entryName(fromStd(name)), &error);
                if (!error.isEmpty())
                    return sol::nullopt;
                auto buffer = std::make_shared<QBuffer>();
                buffer->setData(data);
                buffer->open(QIODevice::ReadOnly);
                return File{buffer};
            }
            if ((flags & 2) && a.readOnly)
                return sol::nullopt;
            auto file = std::make_shared<QFile>(a.onDisk(fromStd(name)));
            QIODevice::OpenMode openMode;
            if (flags & 1)
                openMode |= QIODevice::ReadOnly;
            if (flags & 2)
                openMode |= QIODevice::WriteOnly;
            if (!file->open(openMode))
                return sol::nullopt;
            return File{file};
        },
        "closeFile",
        [](Archive&, File& file) {
            if (file.device)
                file.device->close();
        },
        "removeFile",
        [](Archive& a, const std::string& name) {
            return !a.zip && !a.readOnly && QFile::remove(a.onDisk(fromStd(name)));
        },
        "renameFile",
        [](Archive& a, const std::string& from, const std::string& to) {
            return !a.zip && !a.readOnly && QFile::rename(a.onDisk(fromStd(from)), a.onDisk(fromStd(to)));
        },
        "flush", [](Archive&) {}, //
        "release", [](Archive&) {});
}

} // namespace OFS::Script
