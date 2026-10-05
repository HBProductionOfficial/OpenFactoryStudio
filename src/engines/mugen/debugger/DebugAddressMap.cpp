#include "engines/mugen/debugger/DebugAddressMap.h"

#include "core/debug/ProcessMemory.h"

#include <QCoreApplication>
#include <QFile>
#include <QXmlStreamReader>

namespace OFS::Mugen {

namespace {

struct DebugAddressMapTexts
{
    Q_DECLARE_TR_FUNCTIONS(OFS::Mugen::DebugAddressMapTexts)
};

std::optional<DebugAddress::Type> typeOf(QStringView text)
{
    if (text.isEmpty() || text == u"int")
        return DebugAddress::Type::Int;
    if (text == u"float")
        return DebugAddress::Type::Float;
    if (text == u"byte")
        return DebugAddress::Type::Byte;
    if (text == u"string")
        return DebugAddress::Type::String;
    if (text == u"pointer")
        return DebugAddress::Type::Pointer;
    return std::nullopt;
}

} // namespace

int DebugAddress::elementSize() const
{
    switch (type) {
    case Type::Byte:
        return 1;
    case Type::String:
        return std::max(1, count);
    case Type::Int:
    case Type::Float:
    case Type::Pointer:
        break;
    }
    return 4;
}

int DebugAddress::size() const
{
    return type == Type::String ? elementSize() : elementSize() * std::max(1, count);
}

QList<DebugAddressMap> DebugAddressMap::load(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = DebugAddressMapTexts::tr("Cannot read %1.").arg(path);
        return {};
    }
    return parse(file.readAll(), error);
}

QList<DebugAddressMap> DebugAddressMap::parse(const QByteArray& xml, QString* error)
{
    QList<DebugAddressMap> maps;
    QXmlStreamReader reader(xml);
    auto fail = [&](const QString& message) {
        if (error)
            *error = DebugAddressMapTexts::tr("Line %1: %2").arg(reader.lineNumber()).arg(message);
        return QList<DebugAddressMap>();
    };
    if (!reader.readNextStartElement())
        return fail(reader.errorString());
    while (reader.readNextStartElement()) {
        if (reader.name() != u"mugen") {
            reader.skipCurrentElement();
            continue;
        }
        DebugAddressMap map;
        map.m_version = reader.attributes().value(u"version").toString().toLower();
        for (const QString& library : reader.attributes().value(u"imports").toString().split(u',', Qt::SkipEmptyParts))
            map.m_imports << library.trimmed().toLower();
        while (reader.readNextStartElement()) {
            if (reader.name() != u"address") {
                reader.skipCurrentElement();
                continue;
            }
            const QXmlStreamAttributes a = reader.attributes();
            DebugAddress address;
            address.name = a.value(u"name").toString();
            const QString value = a.value(u"value").toString().trimmed();
            if (!value.isEmpty()) {
                bool ok = false;
                const uint number = value.toUInt(&ok);
                if (!ok)
                    return fail(DebugAddressMapTexts::tr("\"%1\" is not a decimal number.").arg(value));
                address.value = number;
            }
            address.absolute = a.value(u"absolute") == u"true";
            const std::optional<DebugAddress::Type> type = typeOf(a.value(u"type"));
            if (!type)
                return fail(DebugAddressMapTexts::tr("Unknown type \"%1\".").arg(a.value(u"type").toString()));
            address.type = *type;
            if (a.hasAttribute(u"count"))
                address.count = std::max(1, a.value(u"count").toInt());
            for (const QString& pair : a.value(u"enum").toString().split(u',', Qt::SkipEmptyParts)) {
                const qsizetype colon = pair.indexOf(u':');
                if (colon > 0)
                    address.letters.insert(pair.left(colon).trimmed().toInt(), pair.mid(colon + 1).trimmed());
            }
            if (!address.name.isEmpty()) {
                map.m_index.insert(address.name, int(map.m_addresses.size()));
                map.m_addresses << address;
            }
            reader.skipCurrentElement();
        }
        maps << map;
    }
    if (reader.hasError())
        return fail(reader.errorString());
    return maps;
}

std::optional<DebugAddressMap> DebugAddressMap::forExecutable(const QList<DebugAddressMap>& maps,
                                                              const QString& executable)
{
    const QStringList libraries = Debug::importedLibraries(executable);
    if (libraries.isEmpty())
        return std::nullopt;
    for (const DebugAddressMap& map : maps) {
        if (map.m_imports.isEmpty())
            continue;
        const bool all = std::all_of(map.m_imports.cbegin(), map.m_imports.cend(),
                                     [&libraries](const QString& library) { return libraries.contains(library); });
        if (all)
            return map;
    }
    return std::nullopt;
}

const DebugAddress* DebugAddressMap::find(const QString& name) const
{
    const auto it = m_index.constFind(name);
    return it == m_index.cend() ? nullptr : &m_addresses[*it];
}

const DebugAddress* DebugAddressMap::known(const QString& name) const
{
    const DebugAddress* address = find(name);
    return address && address->isKnown() ? address : nullptr;
}

int DebugAddressMap::playerSize() const
{
    int end = 0;
    for (const DebugAddress& a : m_addresses) {
        if (a.isKnown() && !a.absolute && a.name.startsWith(QLatin1String("player_")))
            end = std::max(end, int(*a.value) + a.size());
    }
    return end;
}

} // namespace OFS::Mugen
