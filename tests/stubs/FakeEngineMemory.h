#pragma once

// Fake engine memory for the Debug > Start on Engine tests.

#include "core/debug/ProcessMemory.h"
#include "engines/mugen/debugger/DebugAddressMap.h"

#include <QtGlobal>

#include <algorithm>
#include <memory>

namespace OFS::Test {

class FakeEngineMemory
{
public:
    static constexpr std::uint32_t GameAddress = 0x02000000;

    explicit FakeEngineMemory(Mugen::DebugAddressMap map)
        : m_map(std::move(map))
        , m_image(std::make_shared<Debug::MemoryImage>())
    {
        int gameSize = 0;
        for (const Mugen::DebugAddress& a : m_map.addresses()) {
            if (!a.isKnown())
                continue;
            if (a.absolute)
                m_image->map(*a.value, QByteArray(std::max(8, a.size()), '\0'));
            else if (!a.name.startsWith(QLatin1String("player_")))
                gameSize = std::max(gameSize, int(*a.value) + a.size());
        }
        m_image->map(GameAddress, QByteArray(gameSize + 64, '\0'));
        m_image->setInt(*m_map.known(QStringLiteral("game_base"))->value, std::int32_t(GameAddress));
    }

    const Mugen::DebugAddressMap& map() const { return m_map; }
    Debug::MemoryImage& image() { return *m_image; }
    std::shared_ptr<Debug::MemoryImage> sharedImage() const { return m_image; }

    // Slots 0 to 3 hold the players, the later slots hold helpers.
    void addPlayer(int slot, std::uint32_t address, int id, const QByteArray& name)
    {
        m_image->map(address, QByteArray(m_map.playerSize() + 16, '\0'));
        setSlot(slot, address);
        set(address, "player_active", 1);
        set(address, "player_id", id);
        set(address, "player_alive", 1);
        set(address, "player_life", 1000);
        set(address, "player_facing", 1);
        set(address, "player_state_type", 1);
        set(address, "player_physics", 1);
        if (const Mugen::DebugAddress* a = m_map.known(QStringLiteral("player_name")))
            m_image->setText(address + *a->value, name);
    }
    void setSlot(int slot, std::uint32_t address)
    {
        m_image->setInt(GameAddress + *m_map.known(QStringLiteral("players"))->value + 4u * std::uint32_t(slot),
                        std::int32_t(address));
    }

    void set(std::uint32_t player, const char* field, std::int32_t value, int index = 0)
    {
        m_image->setInt(at(player, field, index), value);
    }
    void setFloat(std::uint32_t player, const char* field, float value, int index = 0)
    {
        m_image->setFloat(at(player, field, index), value);
    }
    std::int32_t get(std::uint32_t player, const char* field, int index = 0) const
    {
        return m_image->readInt(at(player, field, index)).value_or(-1);
    }

    void setGlobal(const char* field, std::int32_t value)
    {
        const Mugen::DebugAddress* a = m_map.known(QLatin1String(field));
        const std::uint64_t address = a->absolute ? *a->value : GameAddress + *a->value;
        if (a->type == Mugen::DebugAddress::Type::Byte)
            m_image->writeByte(address, std::uint8_t(value));
        else if (a->type == Mugen::DebugAddress::Type::Float)
            m_image->setFloat(address, float(value));
        else
            m_image->setInt(address, value);
    }
    std::int32_t global(const char* field) const
    {
        const Mugen::DebugAddress* a = m_map.known(QLatin1String(field));
        const std::uint64_t address = a->absolute ? *a->value : GameAddress + *a->value;
        if (a->type == Mugen::DebugAddress::Type::Byte) {
            const QByteArray b = m_image->bytes(address, 1);
            return b.isEmpty() ? -1 : std::int32_t(static_cast<unsigned char>(b[0]));
        }
        return m_image->readInt(address).value_or(-1);
    }
    std::uint64_t globalAddress(const char* field) const
    {
        const Mugen::DebugAddress* a = m_map.known(QLatin1String(field));
        return a->absolute ? *a->value : GameAddress + *a->value;
    }

    void frame(const QList<std::uint32_t>& players)
    {
        setGlobal("game_time", global("game_time") + 1);
        for (std::uint32_t p : players)
            set(p, "player_state_time", get(p, "player_state_time") + 1);
    }

    void changeState(std::uint32_t player, int state)
    {
        set(player, "player_prev_state_no", get(player, "player_state_no"));
        set(player, "player_state_no", state);
        set(player, "player_state_time", 0);
    }

    // Copies the engine's main loop around its pause flag: each read of the copy is one pass.
    void playLoop(const QList<std::uint32_t>& players)
    {
        const std::uint64_t copy = globalAddress("frame_paused");
        m_image->onRead = [this, copy, players](std::uint64_t address) {
            if (address != copy || m_inLoop)
                return;
            m_inLoop = true;
            const std::int32_t paused = global("pause");
            setGlobal("frame_paused", paused);
            if (paused == 0)
                frame(players);
            m_inLoop = false;
        };
    }

private:
    std::uint64_t at(std::uint32_t player, const char* field, int index) const
    {
        const Mugen::DebugAddress* a = m_map.known(QLatin1String(field));
        Q_ASSERT(a);
        return std::uint64_t(player) + *a->value + std::uint64_t(index) * std::uint64_t(a->elementSize());
    }

    Mugen::DebugAddressMap m_map;
    std::shared_ptr<Debug::MemoryImage> m_image;
    bool m_inLoop = false;
};

// Shared image, so the test can change it while the session owns the memory.
class SharedImageMemory : public Debug::ProcessMemory
{
public:
    explicit SharedImageMemory(std::shared_ptr<Debug::MemoryImage> image)
        : m_image(std::move(image))
    {}
    bool read(std::uint64_t address, void* data, std::size_t size) const override
    {
        return m_image->read(address, data, size);
    }
    bool write(std::uint64_t address, const void* data, std::size_t size) override
    {
        return m_image->write(address, data, size);
    }

private:
    std::shared_ptr<Debug::MemoryImage> m_image;
};

} // namespace OFS::Test
