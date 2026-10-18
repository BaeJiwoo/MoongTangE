#include "Server/Core/ServerHost.h"

#include "Common/Protocol/AdmissionTicket.h"
#include "Common/SimulationConfig.h"
#include "Server/Core/GameRoomManager.h"
#include "Server/Core/TickScheduler.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace MoongTangE {

ServerHost::ServerHost(GameRoomOptions options, bool requireTicket,
                       std::chrono::seconds reservationLifetime)
    : m_options(options), m_requireTicket(requireTicket), m_reservationLifetime(reservationLifetime)
{
    m_options.Validate();
    if (reservationLifetime < std::chrono::seconds(1) ||
        reservationLifetime > std::chrono::seconds(300)) {
        throw std::invalid_argument("Reservation lifetime must be 1..300 seconds");
    }
}

ServerHost::~ServerHost()
{
    Stop();
}

bool ServerHost::Running() const
{
    return m_running.load();
}

void ServerHost::Start()
{
    std::lock_guard lifecycle(m_lifecycleMutex);
    if (m_running.exchange(true)) {
        return;
    }

    try {
        m_tickThread = std::jthread([this](std::stop_token stopToken) {
            Run(stopToken);
        });
    } catch (...) {
        m_running = false;
        throw;
    }
}

void ServerHost::Stop()
{
    std::lock_guard lifecycle(m_lifecycleMutex);
    m_running = false;
    if (m_tickThread.joinable()) {
        m_tickThread.request_stop();
        m_tickThread.join();
    }

    std::lock_guard lock(m_dataMutex);
    m_pendingCommands.clear();
    m_pendingInputs.clear();
    m_clients.clear();
    m_latestSnapshots.clear();
    m_reservations.clear();
    m_matchmakingState = {};
}

std::uint32_t ServerHost::Connect(const std::string& ticket)
{
    std::lock_guard lock(m_dataMutex);
    if (!m_running || m_clients.size() >= Capacity() * 2 ||
        m_pendingCommands.size() >= MAX_PENDING_COMMANDS || m_nextPlayerId == 0) {
        return 0;
    }

    if (!ticket.empty()) {
        const auto found = m_reservations.find(ticket);
        if (found == m_reservations.end() ||
            found->second.m_result.status != ReservationStatus::RESERVED ||
            found->second.m_claimQueued || found->second.m_cancelQueued ||
            std::chrono::steady_clock::now() >= found->second.m_expiresAt) {
            return 0;
        }
        auto& reservation = found->second;
        reservation.m_claimQueued = true;
        m_clients.emplace(reservation.m_playerId, ClientState{});
        m_pendingCommands.push_back({WorldCommandType::CLAIM, reservation.m_playerId, ticket});
        return reservation.m_playerId;
    }
    if (m_requireTicket) {
        return 0;
    }

    const auto playerId = m_nextPlayerId++;
    m_clients.emplace(playerId, ClientState{});
    m_pendingCommands.push_back({WorldCommandType::JOIN, playerId, {}});
    return playerId;
}

void ServerHost::Disconnect(std::uint32_t playerId)
{
    std::lock_guard lock(m_dataMutex);
    if (m_clients.erase(playerId)) {
        m_pendingInputs.erase(playerId);
        m_pendingCommands.push_back({WorldCommandType::LEAVE, playerId, {}});
    }
    // Cleanup has a reserved allowance of at most twice Capacity() beyond the regular limit.
}

ReservationResult ServerHost::Reserve(const std::string& ticket)
{
    std::lock_guard lock(m_dataMutex);
    if (!m_running || !valid_admission_ticket(ticket)) {
        return {ReservationStatus::UNAVAILABLE, 0};
    }
    if (const auto found = m_reservations.find(ticket); found != m_reservations.end()) {
        return ReservationView(found->second);
    }
    if (m_reservations.size() >= MAX_RESERVATION_RECORDS ||
        m_pendingCommands.size() >= MAX_PENDING_COMMANDS || m_nextPlayerId == 0) {
        return {ReservationStatus::UNAVAILABLE, 0};
    }

    const auto playerId = m_nextPlayerId++;
    ReservationRecord record;
    record.m_playerId = playerId;
    record.m_expiresAt = std::chrono::steady_clock::now() + m_reservationLifetime;
    m_reservations.emplace(ticket, record);
    m_pendingCommands.push_back({WorldCommandType::RESERVE, playerId, ticket});
    return ReservationView(record);
}

ReservationResult ServerHost::Reservation(const std::string& ticket) const
{
    std::lock_guard lock(m_dataMutex);
    const auto found = m_reservations.find(ticket);
    return found == m_reservations.end() ? ReservationResult{} : ReservationView(found->second);
}

ReservationResult ServerHost::ReservationView(const ReservationRecord& record)
{
    auto result = record.m_result;
    if (result.status == ReservationStatus::PENDING ||
        result.status == ReservationStatus::RESERVED) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   record.m_expiresAt - std::chrono::steady_clock::now())
                                   .count();
        result.expiresInMs = std::max<std::int64_t>(0, remaining);
    }
    return result;
}

bool ServerHost::CancelReservation(const std::string& ticket)
{
    std::lock_guard lock(m_dataMutex);
    const auto found = m_reservations.find(ticket);
    if (!m_running || found == m_reservations.end()) {
        return false;
    }
    auto& record = found->second;
    if (record.m_cancelQueued || (record.m_result.status != ReservationStatus::RESERVED &&
                                  record.m_result.status != ReservationStatus::PENDING)) {
        return true;
    }
    if (m_pendingCommands.size() >= MAX_PENDING_COMMANDS) {
        return false;
    }
    record.m_cancelQueued = true;
    m_pendingCommands.push_back({WorldCommandType::CANCEL, record.m_playerId, ticket});
    return true;
}

MatchmakingState ServerHost::Matchmaking() const
{
    std::lock_guard lock(m_dataMutex);
    auto state = m_matchmakingState;
    state.running = m_running;
    return state;
}

AdmissionResult ServerHost::Admission(std::uint32_t playerId) const
{
    std::lock_guard lock(m_dataMutex);
    const auto client = m_clients.find(playerId);
    return m_running && client != m_clients.end()
               ? client->second.m_admission
               : AdmissionResult{AdmissionStatus::UNAVAILABLE, 0, 0};
}

bool ServerHost::Submit(InputCommand input)
{
    if (!input.Valid()) {
        return false;
    }

    std::lock_guard lock(m_dataMutex);
    const auto client = m_clients.find(input.playerId);
    if (!m_running || client == m_clients.end() ||
        client->second.m_admission.status != AdmissionStatus::ACCEPTED ||
        input.sequence <= client->second.m_lastInputSequence) {
        return false;
    }

    client->second.m_lastInputSequence = input.sequence;
    // Movement is held state: only the latest input per player is needed at a tick boundary.
    // Its separate slot cannot consume another player's slot or block lifecycle commands.
    m_pendingInputs.insert_or_assign(input.playerId, input);
    return true;
}

std::optional<RoomSnapshot> ServerHost::SnapshotFor(std::uint32_t playerId) const
{
    std::lock_guard lock(m_dataMutex);
    const auto client = m_clients.find(playerId);
    if (!m_running || client == m_clients.end() ||
        client->second.m_admission.status != AdmissionStatus::ACCEPTED) {
        return {};
    }

    const auto room = m_latestSnapshots.find(client->second.m_admission.roomId);
    return room == m_latestSnapshots.end() ? std::nullopt : std::optional(room->second);
}

std::vector<RoomSnapshot> ServerHost::Snapshots() const
{
    std::lock_guard lock(m_dataMutex);
    std::vector<RoomSnapshot> snapshots;
    snapshots.reserve(m_latestSnapshots.size());
    for (const auto& [roomId, snapshot] : m_latestSnapshots) {
        (void)roomId;
        snapshots.push_back(snapshot);
    }
    return snapshots;
}

std::size_t ServerHost::Capacity() const
{
    return m_options.m_maxRooms * GameRoomOptions::PLAYERS_PER_ROOM;
}

std::size_t ServerHost::Pending() const
{
    std::lock_guard lock(m_dataMutex);
    return m_pendingCommands.size() + m_pendingInputs.size();
}

bool ServerHost::HasClient(std::uint32_t playerId) const
{
    return Admission(playerId).status == AdmissionStatus::ACCEPTED;
}

void ServerHost::Run(std::stop_token stopToken)
{
    GameRoomManager rooms(m_options);
    TickScheduler scheduler;
    std::uint64_t tick = 0;

    while (!stopToken.stop_requested()) {
        std::deque<WorldCommand> commands;
        std::map<std::uint32_t, InputCommand> inputs;
        {
            std::lock_guard lock(m_dataMutex);
            commands.swap(m_pendingCommands);
            inputs.swap(m_pendingInputs);
        }

        const auto now = GameRoomManager::Clock::now();
        {
            std::lock_guard lock(m_dataMutex);
            ExpireReservations(rooms, now);
            for (const auto& command : commands) {
                ApplyCommand(command, rooms, now);
            }
        }

        for (const auto& [playerId, input] : inputs) {
            (void)playerId;
            rooms.Apply(input);
        }

        rooms.Step(now);
        {
            std::lock_guard lock(m_dataMutex);
            m_matchmakingState = {true, tick + 1, now, rooms.Occupancy()};
        }
        if (++tick % simulation::SNAPSHOT_INTERVAL_TICKS == 0) {
            auto snapshots = rooms.Snapshots();
            std::lock_guard lock(m_dataMutex);
            m_latestSnapshots.clear();
            for (auto& snapshot : snapshots) {
                const auto roomId = snapshot.roomId;
                m_latestSnapshots.emplace(roomId, std::move(snapshot));
            }
        }

        std::this_thread::sleep_until(scheduler.Advance());
    }
}

void ServerHost::ApplyCommand(const WorldCommand& command, GameRoomManager& rooms,
                              std::chrono::steady_clock::time_point now)
{
    if (command.m_type == WorldCommandType::LEAVE) {
        rooms.Leave(command.m_playerId, now);
        return;
    }
    if (command.m_type == WorldCommandType::JOIN) {
        const auto client = m_clients.find(command.m_playerId);
        if (client != m_clients.end()) {
            const auto roomId = rooms.Join(command.m_playerId, now);
            client->second.m_admission =
                roomId ? AdmissionResult{AdmissionStatus::ACCEPTED, command.m_playerId, roomId}
                       : AdmissionResult{AdmissionStatus::FULL, 0, 0};
        }
        return;
    }

    const auto found = m_reservations.find(command.m_ticket);
    if (found == m_reservations.end()) {
        if (command.m_type == WorldCommandType::CLAIM) {
            if (const auto client = m_clients.find(command.m_playerId); client != m_clients.end()) {
                client->second.m_admission = {AdmissionStatus::UNAVAILABLE, 0, 0};
            }
        }
        return;
    }
    auto& record = found->second;
    auto& result = record.m_result;
    if (command.m_type == WorldCommandType::RESERVE &&
        result.status == ReservationStatus::PENDING) {
        const auto roomId = rooms.Reserve(command.m_playerId, now);
        result = {roomId ? ReservationStatus::RESERVED : ReservationStatus::FULL, roomId};
    } else if (command.m_type == WorldCommandType::CANCEL &&
               (result.status == ReservationStatus::PENDING ||
                result.status == ReservationStatus::RESERVED)) {
        rooms.Leave(command.m_playerId, now);
        result.status = ReservationStatus::CANCELLED;
    } else if (command.m_type == WorldCommandType::CLAIM) {
        const auto client = m_clients.find(command.m_playerId);
        if (client == m_clients.end()) {
            if (result.status == ReservationStatus::RESERVED) {
                rooms.Leave(command.m_playerId, now);
                result.status = ReservationStatus::CANCELLED;
            }
        } else if (result.status == ReservationStatus::RESERVED &&
                   rooms.Claim(command.m_playerId)) {
            result.status = ReservationStatus::CONSUMED;
            client->second.m_admission = {AdmissionStatus::ACCEPTED, command.m_playerId,
                                          result.roomId};
        } else {
            client->second.m_admission = {AdmissionStatus::UNAVAILABLE, 0, 0};
        }
    }
}

void ServerHost::ExpireReservations(GameRoomManager& rooms,
                                    std::chrono::steady_clock::time_point now)
{
    for (auto& [ticket, record] : m_reservations) {
        (void)ticket;
        if (now >= record.m_expiresAt && (record.m_result.status == ReservationStatus::RESERVED ||
                                          record.m_result.status == ReservationStatus::PENDING)) {
            rooms.Leave(record.m_playerId, now);
            record.m_result.status = ReservationStatus::EXPIRED;
        }
    }
    // Bounded idempotency history; callers must never recycle a ticket for a new attempt.
    std::erase_if(m_reservations, [&](const auto& entry) {
        return now >= entry.second.m_expiresAt + std::chrono::seconds(60);
    });
}

} // namespace MoongTangE
