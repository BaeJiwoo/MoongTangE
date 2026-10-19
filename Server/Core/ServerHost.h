#pragma once

#include "Common/Protocol/AdmissionResult.h"
#include "Common/Protocol/InputCommand.h"
#include "Common/Protocol/RoomSnapshot.h"
#include "Server/Core/GameRoomOptions.h"
#include "Server/Core/MatchmakingState.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace MoongTangE {

class GameRoomManager;

class ServerHost
{
  public:
    static constexpr std::size_t MAX_PENDING_COMMANDS = 1024;
    static constexpr std::size_t MAX_RESERVATION_RECORDS = 4096;

    explicit ServerHost(GameRoomOptions options = {}, bool requireTicket = false,
                        std::chrono::seconds reservationLifetime = std::chrono::seconds(30));
    ServerHost(const ServerHost&) = delete;
    ServerHost& operator=(const ServerHost&) = delete;
    ServerHost(ServerHost&&) = delete;
    ServerHost& operator=(ServerHost&&) = delete;
    ~ServerHost();

    void Start();
    void Stop();
    bool Running() const;

    // Returns a request ID; only the tick thread confirms admission.
    std::uint32_t Connect(const std::string& ticket = {}, const std::string& nickname = "Player");
    AdmissionResult Admission(std::uint32_t playerId) const;
    void Disconnect(std::uint32_t playerId);

    ReservationResult Reserve(const std::string& ticket);
    ReservationResult Reservation(const std::string& ticket) const;
    bool CancelReservation(const std::string& ticket);
    MatchmakingState Matchmaking() const;

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
        LEAVE,
        RESERVE,
        CLAIM,
        CANCEL
    };

    struct WorldCommand
    {
        WorldCommandType m_type{WorldCommandType::JOIN};
        std::uint32_t m_playerId{};
        std::string m_ticket;
    };

    void Run(std::stop_token stopToken);
    void ApplyCommand(const WorldCommand& command, GameRoomManager& rooms,
                      std::chrono::steady_clock::time_point now);
    void ExpireReservations(GameRoomManager& rooms, std::chrono::steady_clock::time_point now);

    struct ClientState
    {
        AdmissionResult m_admission;
        std::string m_nickname;
        std::uint64_t m_lastInputSequence{};
    };

    struct ReservationRecord
    {
        ReservationResult m_result{ReservationStatus::PENDING, 0};
        std::uint32_t m_playerId{};
        std::chrono::steady_clock::time_point m_expiresAt;
        bool m_claimQueued{};
        bool m_cancelQueued{};
    };

    static ReservationResult ReservationView(const ReservationRecord& record);

    GameRoomOptions m_options;
    bool m_requireTicket{};
    std::chrono::seconds m_reservationLifetime;

    // Data lock protects requests, admission, sequence state, snapshots and IDs.
    // Lifecycle lock serializes Start/Stop; only Run owns and changes the rooms.
    mutable std::mutex m_dataMutex;
    std::mutex m_lifecycleMutex;

    std::deque<WorldCommand> m_pendingCommands;
    std::map<std::uint32_t, InputCommand> m_pendingInputs;
    std::map<std::uint32_t, ClientState> m_clients;
    std::map<std::uint64_t, RoomSnapshot> m_latestSnapshots;
    std::map<std::string, ReservationRecord> m_reservations;
    MatchmakingState m_matchmakingState;
    std::uint32_t m_nextPlayerId{1};

    std::atomic<bool> m_running{false};
    std::jthread m_tickThread;
};

} // namespace MoongTangE
