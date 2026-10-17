#pragma once

#include "Common/Protocol/InputCommand.h"
#include "Common/Protocol/WorldSnapshot.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <thread>

namespace MoongTangE {

class ServerHost
{
  public:
    static constexpr std::size_t MAX_PENDING_COMMANDS = 1024;

    ServerHost() = default;
    ServerHost(const ServerHost&) = delete;
    ServerHost& operator=(const ServerHost&) = delete;
    ServerHost(ServerHost&&) = delete;
    ServerHost& operator=(ServerHost&&) = delete;
    ~ServerHost();

    void Start();
    void Stop();
    bool Running() const;

    std::uint32_t Connect();
    void Disconnect(std::uint32_t playerId);

    bool Submit(InputCommand input);

    WorldSnapshot Snapshot() const;
    std::size_t Pending() const;
    bool HasClient(std::uint32_t playerId) const;

  private:
    enum class WorldCommandType
    {
        JOIN,
        INPUT,
        LEAVE
    };

    struct WorldCommand
    {
        WorldCommandType m_type{WorldCommandType::JOIN};
        std::uint32_t m_playerId{};
        InputCommand m_input{};
    };

    void Run(std::stop_token stopToken);

    // m_dataMutex protects the command queue, sequence map, snapshot and ID; m_lifecycleMutex
    // serializes start/stop. The tick thread alone owns the GameWorld inside Run().
    mutable std::mutex m_dataMutex;
    std::mutex m_lifecycleMutex;

    std::deque<WorldCommand> m_pendingCommands;
    std::map<std::uint32_t, std::uint64_t> m_lastInputSequences;
    WorldSnapshot m_latestSnapshot;
    std::uint32_t m_nextPlayerId{1};

    std::atomic<bool> m_running{false};
    std::jthread m_tickThread;
};

} // namespace MoongTangE
