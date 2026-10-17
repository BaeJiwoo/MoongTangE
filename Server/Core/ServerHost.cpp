#include "Server/Core/ServerHost.h"

#include "Common/Game/GameWorld.h"
#include "Common/SimulationConfig.h"
#include "Server/Core/TickScheduler.h"

#include <utility>

namespace MoongTangE {
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
    m_lastInputSequences.clear();
    m_latestSnapshot = {};
}

std::uint32_t ServerHost::Connect()
{
    std::lock_guard lock(m_dataMutex);
    if (!m_running || m_lastInputSequences.size() >= MAX_PLAYERS ||
        m_pendingCommands.size() >= MAX_PENDING_COMMANDS || m_nextPlayerId == 0) {
        return 0;
    }
    const auto playerId = m_nextPlayerId++;
    m_lastInputSequences.emplace(playerId, 0);
    m_pendingCommands.push_back({WorldCommandType::JOIN, playerId});
    return playerId;
}

void ServerHost::Disconnect(std::uint32_t playerId)
{
    std::lock_guard lock(m_dataMutex);
    if (m_lastInputSequences.erase(playerId)) {
        m_pendingCommands.push_back({WorldCommandType::LEAVE, playerId});
    }
    // Leaves may exceed MAX_PENDING_COMMANDS by at most MAX_PLAYERS, so cleanup is never dropped.
}

bool ServerHost::Submit(InputCommand input)
{
    if (!input.Valid()) {
        return false;
    }
    std::lock_guard lock(m_dataMutex);
    const auto connection = m_lastInputSequences.find(input.playerId);
    if (!m_running || connection == m_lastInputSequences.end() ||
        input.sequence <= connection->second || m_pendingCommands.size() >= MAX_PENDING_COMMANDS) {
        return false;
    }
    connection->second = input.sequence;
    m_pendingCommands.push_back({WorldCommandType::INPUT, input.playerId, input});
    return true;
}

WorldSnapshot ServerHost::Snapshot() const
{
    std::lock_guard lock(m_dataMutex);
    return m_latestSnapshot;
}

std::size_t ServerHost::Pending() const
{
    std::lock_guard lock(m_dataMutex);
    return m_pendingCommands.size();
}

bool ServerHost::HasClient(std::uint32_t playerId) const
{
    std::lock_guard lock(m_dataMutex);
    return m_running && m_lastInputSequences.contains(playerId);
}

void ServerHost::Run(std::stop_token stopToken)
{
    GameWorld world;
    TickScheduler scheduler;
    while (!stopToken.stop_requested()) {
        std::deque<WorldCommand> commands;
        {
            std::lock_guard lock(m_dataMutex);
            commands.swap(m_pendingCommands);
        }
        for (const auto& command : commands) {
            switch (command.m_type) {
            case WorldCommandType::JOIN:
                world.AddPlayer(command.m_playerId);
                break;
            case WorldCommandType::INPUT:
                world.Apply(command.m_input);
                break;
            case WorldCommandType::LEAVE:
                world.RemovePlayer(command.m_playerId);
                break;
            }
        }
        world.Step();
        auto state = world.Snapshot();
        if (state.tick % simulation::SNAPSHOT_INTERVAL_TICKS == 0) {
            std::lock_guard lock(m_dataMutex);
            m_latestSnapshot = std::move(state);
        }
        std::this_thread::sleep_until(scheduler.Advance());
    }
}
} // namespace MoongTangE
