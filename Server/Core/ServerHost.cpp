#include "Server/Core/ServerHost.h"

#include "Common/SimulationConfig.h"
#include "Server/Core/GameRoomManager.h"
#include "Server/Core/TickScheduler.h"

#include <utility>

namespace MoongTangE {

ServerHost::ServerHost(GameRoomOptions options) : m_options(options)
{
    m_options.Validate();
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
    m_clients.clear();
    m_latestSnapshots.clear();
}

std::uint32_t ServerHost::Connect()
{
    std::lock_guard lock(m_dataMutex);
    if (!m_running || m_clients.size() >= Capacity() * 2 ||
        m_pendingCommands.size() >= MAX_PENDING_COMMANDS || m_nextPlayerId == 0) {
        return 0;
    }

    const auto playerId = m_nextPlayerId++;
    m_clients.emplace(playerId, ClientState{});
    m_pendingCommands.push_back({WorldCommandType::JOIN, playerId});
    return playerId;
}

void ServerHost::Disconnect(std::uint32_t playerId)
{
    std::lock_guard lock(m_dataMutex);
    if (m_clients.erase(playerId)) {
        m_pendingCommands.push_back({WorldCommandType::LEAVE, playerId});
    }
    // Cleanup has a reserved allowance of at most twice Capacity() beyond the regular limit.
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
        input.sequence <= client->second.m_lastInputSequence ||
        m_pendingCommands.size() >= MAX_PENDING_COMMANDS) {
        return false;
    }

    client->second.m_lastInputSequence = input.sequence;
    m_pendingCommands.push_back({WorldCommandType::INPUT, input.playerId, input});
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
    return m_pendingCommands.size();
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
        {
            std::lock_guard lock(m_dataMutex);
            commands.swap(m_pendingCommands);
        }

        const auto now = GameRoomManager::Clock::now();
        for (const auto& command : commands) {
            switch (command.m_type) {
            case WorldCommandType::JOIN: {
                std::lock_guard lock(m_dataMutex);
                const auto client = m_clients.find(command.m_playerId);
                if (client != m_clients.end()) {
                    const auto roomId = rooms.Join(command.m_playerId, now);
                    client->second.m_admission =
                        roomId
                            ? AdmissionResult{AdmissionStatus::ACCEPTED, command.m_playerId, roomId}
                            : AdmissionResult{AdmissionStatus::FULL, 0, 0};
                }
                break;
            }
            case WorldCommandType::INPUT:
                rooms.Apply(command.m_input);
                break;
            case WorldCommandType::LEAVE:
                rooms.Leave(command.m_playerId, now);
                break;
            }
        }

        rooms.Step(now);
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

} // namespace MoongTangE
