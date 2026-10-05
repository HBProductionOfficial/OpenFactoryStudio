#include "core/debug/ProcessMemory.h"

#include <QCoreApplication>
#include <QFile>
#include <QtEndian>

#include <cstring>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace OFS::Debug {

namespace {

// Translation context for this file's texts. lupdate reads the qualified class name.
struct ProcessMemoryTexts
{
    Q_DECLARE_TR_FUNCTIONS(OFS::Debug::ProcessMemoryTexts)
};

ProcessMemory::Opener& testOpener()
{
    static ProcessMemory::Opener opener;
    return opener;
}

std::function<QStringList(const QString&)>& testImportReader()
{
    static std::function<QStringList(const QString&)> reader;
    return reader;
}

#ifdef Q_OS_WIN

class WindowsProcessMemory : public ProcessMemory
{
public:
    explicit WindowsProcessMemory(HANDLE process)
        : m_process(process)
    {}
    ~WindowsProcessMemory() override { CloseHandle(m_process); }

    bool read(std::uint64_t address, void* data, std::size_t size) const override
    {
        SIZE_T done = 0;
        return ReadProcessMemory(m_process, reinterpret_cast<LPCVOID>(static_cast<std::uintptr_t>(address)), data, size,
                                 &done) &&
               done == size;
    }

    bool write(std::uint64_t address, const void* data, std::size_t size) override
    {
        SIZE_T done = 0;
        return WriteProcessMemory(m_process, reinterpret_cast<LPVOID>(static_cast<std::uintptr_t>(address)), data, size,
                                  &done) &&
               done == size;
    }

private:
    HANDLE m_process;
};

#endif

} // namespace

ProcessMemory::~ProcessMemory() = default;

QByteArray ProcessMemory::bytes(std::uint64_t address, std::size_t size) const
{
    QByteArray out(qsizetype(size), Qt::Uninitialized);
    if (size == 0 || !read(address, out.data(), size))
        return {};
    return out;
}

std::optional<std::int32_t> ProcessMemory::readInt(std::uint64_t address) const
{
    if (const std::optional<std::uint32_t> value = readUInt(address))
        return static_cast<std::int32_t>(*value);
    return std::nullopt;
}

std::optional<std::uint32_t> ProcessMemory::readUInt(std::uint64_t address) const
{
    unsigned char raw[4];
    if (!read(address, raw, sizeof raw))
        return std::nullopt;
    return qFromLittleEndian<std::uint32_t>(raw);
}

std::optional<float> ProcessMemory::readFloat(std::uint64_t address) const
{
    const std::optional<std::uint32_t> bits = readUInt(address);
    if (!bits)
        return std::nullopt;
    float value = 0;
    std::memcpy(&value, &*bits, sizeof value);
    return value;
}

bool ProcessMemory::writeInt(std::uint64_t address, std::int32_t value)
{
    unsigned char raw[4];
    qToLittleEndian(value, raw);
    return write(address, raw, sizeof raw);
}

bool ProcessMemory::writeByte(std::uint64_t address, std::uint8_t value)
{
    return write(address, &value, 1);
}

std::unique_ptr<ProcessMemory> ProcessMemory::open(qint64 processId, QString* error)
{
    if (testOpener())
        return testOpener()(processId, error);
#ifdef Q_OS_WIN
    const DWORD access = PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_LIMITED_INFORMATION;
    if (HANDLE process = OpenProcess(access, FALSE, static_cast<DWORD>(processId)))
        return std::make_unique<WindowsProcessMemory>(process);
    if (error)
        *error = ProcessMemoryTexts::tr("Cannot open the engine's process (error %1).").arg(GetLastError());
#else
    Q_UNUSED(processId)
    if (error)
        *error = ProcessMemoryTexts::tr("Reading another program's memory is only possible on Windows.");
#endif
    return nullptr;
}

bool ProcessMemory::isAvailable()
{
#ifdef Q_OS_WIN
    return true;
#else
    return bool(testOpener());
#endif
}

void ProcessMemory::setOpener(Opener opener)
{
    testOpener() = std::move(opener);
}

void MemoryImage::map(std::uint64_t address, const QByteArray& bytes)
{
    m_blocks[address] = bytes;
}

void MemoryImage::setInt(std::uint64_t address, std::int32_t value)
{
    std::uint64_t offset = 0;
    if (QByteArray* block = blockAt(address, 4, &offset))
        qToLittleEndian(value, block->data() + offset);
}

void MemoryImage::setFloat(std::uint64_t address, float value)
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof bits);
    setInt(address, static_cast<std::int32_t>(bits));
}

void MemoryImage::setText(std::uint64_t address, const QByteArray& text)
{
    std::uint64_t offset = 0;
    if (QByteArray* block = blockAt(address, std::size_t(text.size()) + 1, &offset)) {
        std::memcpy(block->data() + offset, text.constData(), std::size_t(text.size()));
        (*block)[qsizetype(offset) + text.size()] = '\0';
    }
}

bool MemoryImage::read(std::uint64_t address, void* data, std::size_t size) const
{
    if (onRead)
        onRead(address);
    std::uint64_t offset = 0;
    const QByteArray* block = blockAt(address, size, &offset);
    if (!block)
        return false;
    std::memcpy(data, block->constData() + offset, size);
    return true;
}

bool MemoryImage::write(std::uint64_t address, const void* data, std::size_t size)
{
    std::uint64_t offset = 0;
    QByteArray* block = blockAt(address, size, &offset);
    if (!block)
        return false;
    std::memcpy(block->data() + offset, data, size);
    if (onWrite)
        onWrite(address);
    return true;
}

QByteArray* MemoryImage::blockAt(std::uint64_t address, std::size_t size, std::uint64_t* offset)
{
    return const_cast<QByteArray*>(std::as_const(*this).blockAt(address, size, offset));
}

const QByteArray* MemoryImage::blockAt(std::uint64_t address, std::size_t size, std::uint64_t* offset) const
{
    auto it = m_blocks.upper_bound(address);
    if (it == m_blocks.begin())
        return nullptr;
    --it;
    const std::uint64_t start = address - it->first;
    if (start + size > std::uint64_t(it->second.size()))
        return nullptr;
    *offset = start;
    return &it->second;
}

void setImportReader(std::function<QStringList(const QString& executable)> reader)
{
    testImportReader() = std::move(reader);
}

QStringList importedLibraries(const QString& executable)
{
    if (testImportReader()) {
        QStringList libraries = testImportReader()(executable);
        for (QString& library : libraries)
            library = library.toLower();
        return libraries;
    }
    QFile file(executable);
    // Engines are small, so anything far larger is not one.
    if (!file.open(QIODevice::ReadOnly) || file.size() > 256 * 1024 * 1024)
        return {};
    const QByteArray data = file.readAll();
    const auto size = std::uint64_t(data.size());
    const auto* bytes = reinterpret_cast<const unsigned char*>(data.constData());
    auto u16 = [&](std::uint64_t at) -> std::optional<std::uint32_t> {
        if (at + 2 > size)
            return std::nullopt;
        return qFromLittleEndian<std::uint16_t>(bytes + at);
    };
    auto u32 = [&](std::uint64_t at) -> std::optional<std::uint32_t> {
        if (at + 4 > size)
            return std::nullopt;
        return qFromLittleEndian<std::uint32_t>(bytes + at);
    };
    if (size < 64 || bytes[0] != 'M' || bytes[1] != 'Z')
        return {};
    const std::uint64_t pe = u32(0x3C).value_or(0);
    if (u32(pe) != std::optional<std::uint32_t>(0x00004550)) // "PE\0\0"
        return {};
    const std::uint32_t sections = u16(pe + 6).value_or(0);
    const std::uint32_t optionalSize = u16(pe + 20).value_or(0);
    const std::uint64_t optional = pe + 24;
    const std::uint32_t magic = u16(optional).value_or(0);
    // The data directories follow the optional header's fixed part. Imports are the second one.
    const std::uint64_t directories = magic == 0x20b ? optional + 112 : magic == 0x10b ? optional + 96 : 0;
    if (!directories || u32(directories - 4).value_or(0) < 2)
        return {};
    const std::uint32_t importRva = u32(directories + 8).value_or(0);
    const std::uint64_t sectionTable = optional + optionalSize;
    auto offsetOf = [&](std::uint32_t rva) -> std::optional<std::uint64_t> {
        for (std::uint32_t i = 0; i < sections; ++i) {
            const std::uint64_t s = sectionTable + 40ull * i;
            const std::uint32_t virtualSize = u32(s + 8).value_or(0);
            const std::uint32_t virtualAddress = u32(s + 12).value_or(0);
            const std::uint32_t rawSize = u32(s + 16).value_or(0);
            const std::uint32_t rawPointer = u32(s + 20).value_or(0);
            if (rva >= virtualAddress && rva < virtualAddress + std::max(virtualSize, rawSize))
                return std::uint64_t(rawPointer) + (rva - virtualAddress);
        }
        return std::nullopt;
    };
    QStringList out;
    std::optional<std::uint64_t> entry = importRva ? offsetOf(importRva) : std::nullopt;
    for (int i = 0; entry && i < 4096; ++i, *entry += 20) {
        const std::uint32_t nameRva = u32(*entry + 12).value_or(0);
        if (nameRva == 0)
            break;
        const std::optional<std::uint64_t> name = offsetOf(nameRva);
        if (!name || *name >= size)
            break;
        const char* text = data.constData() + *name;
        const auto length = std::uint64_t(qstrnlen(text, uint(std::min<std::uint64_t>(size - *name, 260))));
        out << QString::fromLatin1(text, qsizetype(length)).toLower();
    }
    return out;
}

} // namespace OFS::Debug
