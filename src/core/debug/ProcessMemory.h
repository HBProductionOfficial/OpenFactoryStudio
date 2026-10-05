#pragma once

#include "core/CoreExport.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>

namespace OFS::Debug {

// Another process's memory, the engine under Debug > Start on Engine. Real processes need Windows.
class OFS_CORE_EXPORT ProcessMemory
{
public:
    virtual ~ProcessMemory();

    virtual bool read(std::uint64_t address, void* data, std::size_t size) const = 0;
    virtual bool write(std::uint64_t address, const void* data, std::size_t size) = 0;

    // Little-endian.
    QByteArray bytes(std::uint64_t address, std::size_t size) const;
    std::optional<std::int32_t> readInt(std::uint64_t address) const;
    std::optional<std::uint32_t> readUInt(std::uint64_t address) const;
    std::optional<float> readFloat(std::uint64_t address) const;
    bool writeInt(std::uint64_t address, std::int32_t value);
    bool writeByte(std::uint64_t address, std::uint8_t value);

    using Opener = std::function<std::unique_ptr<ProcessMemory>(qint64 processId, QString* error)>;
    static std::unique_ptr<ProcessMemory> open(qint64 processId, QString* error = nullptr);
    static bool isAvailable();
    static void setOpener(Opener opener);
};

// Test memory. A read or write must stay inside one block, like one mapped region of a process.
class OFS_CORE_EXPORT MemoryImage : public ProcessMemory
{
public:
    void map(std::uint64_t address, const QByteArray& bytes);
    void setInt(std::uint64_t address, std::int32_t value);
    void setFloat(std::uint64_t address, float value);
    void setText(std::uint64_t address, const QByteArray& text);

    bool read(std::uint64_t address, void* data, std::size_t size) const override;
    bool write(std::uint64_t address, const void* data, std::size_t size) override;

    std::function<void(std::uint64_t address)> onRead;
    std::function<void(std::uint64_t address)> onWrite;

private:
    QByteArray* blockAt(std::uint64_t address, std::size_t size, std::uint64_t* offset);
    const QByteArray* blockAt(std::uint64_t address, std::size_t size, std::uint64_t* offset) const;

    std::map<std::uint64_t, QByteArray> m_blocks;
};

// DLLs in a Windows program's import table, lower case. Empty for other files.
OFS_CORE_EXPORT QStringList importedLibraries(const QString& executable);
OFS_CORE_EXPORT void setImportReader(std::function<QStringList(const QString& executable)> reader);

} // namespace OFS::Debug
