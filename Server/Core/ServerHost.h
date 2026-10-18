#pragma once

#include "Common/Protocol/AdmissionResult.h"
#include "Common/Protocol/InputCommand.h"
#include "Common/Protocol/RoomSnapshot.h"
#include "Server/Core/GameRoomOptions.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace MoongTangE {

class ServerHost
{
  public:
    static constexpr std::size_t MAX_PENDING_COMMANDS = 1024;

    explicit ServerHost(GameRoomOptions options = {});
    ServerHost(const ServerHost&) = delete;
    ServerHost& operator=(const ServerHost&) = delete;
    ServerHost(ServerHost&&) = delete;
    ServerHost& operator=(ServerHost&&) = delete;
    ~ServerHost();

    void Start();
    void Stop();
    bool Running() const;

    // Returns a request ID; only the tick thread confirms admission.
    std::uint32_t Connect();
    AdmissionResult Admission(std::uint32_t playerId) const;
    void Disconnect(std::uint32_t playerId);

    bool Submit(InputCommand input);

    std::optional<RoomSnapshot> SnapshotFor(std::uint32_t playerId) const;
    std::vector<RoomSnapshot> Snapshots() const;
    std::size_t Capacity() const;
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

    struct ClientState
    {
        AdmissionResult m_admission;
        std::uint64_t m_lastInputSequence{};
    };

    GameRoomOptions m_options;

    // Data lock protects requests, admission, sequence state, snapshots and IDs.
    // Lifecycle lock serializes Start/Stop; only Run owns and changes the rooms.
    mutable std::mutex m_dataMutex;
    std::mutex m_lifecycleMutex;

    std::deque<WorldCommand> m_pendingCommands;
    std::map<std::uint32_t, ClientState> m_clients;
    std::map<std::uint64_t, RoomSnapshot> m_latestSnapshots;
    std::uint32_t m_nextPlayerId{1};

    std::atomic<bool> m_running{false};
    std::jthread m_tickThread;
};

} // namespace MoongTangE
