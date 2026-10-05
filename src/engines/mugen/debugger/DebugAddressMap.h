#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>

namespace OFS::Mugen {

// One field of the external debugger's address map (data/engines/mugen/debug.xml).
struct DebugAddress
{
    enum class Type
    {
        Int,
        Float,
        Byte,
        String,
        Pointer, // 32 bits, an address in the engine's process
    };

    QString name;
    std::optional<std::uint32_t> value; // unknown for this engine version (shows N/A)
    bool absolute = false;
    Type type = Type::Int;
    int count = 1;               // array length, pointer table length or string bytes
    QHash<int, QString> letters; // coded values ("1:S,2:C")

    bool isKnown() const { return value.has_value(); }
    int elementSize() const;
    int size() const;
};

// Address map of one engine version. Global fields are absolute addresses or offsets in the game
// structure. "player_" fields are offsets in a player's structure.
class DebugAddressMap
{
public:
    static QList<DebugAddressMap> load(const QString& path, QString* error = nullptr);
    static QList<DebugAddressMap> parse(const QByteArray& xml, QString* error = nullptr);
    // The program must load every library the map names.
    static std::optional<DebugAddressMap> forExecutable(const QList<DebugAddressMap>& maps, const QString& executable);

    QString version() const { return m_version; }
    QStringList imports() const { return m_imports; }
    const QList<DebugAddress>& addresses() const { return m_addresses; }
    const DebugAddress* find(const QString& name) const;
    // Only fields that have a value.
    const DebugAddress* known(const QString& name) const;
    int playerSize() const;

private:
    QString m_version;
    QStringList m_imports;
    QList<DebugAddress> m_addresses;
    QHash<QString, int> m_index;
};

} // namespace OFS::Mugen
