#include "Server/Core/GameRoom.h"

#include "Server/Core/GameRoomOptions.h"

namespace MoongTangE {

GameRoom::GameRoom(std::uint64_t seed) : m_world(seed) {}

bool GameRoom::Join(std::uint32_t playerId)
{
    if (PlayerCount() + ReservedCount() >= GameRoomOptions::PLAYERS_PER_ROOM ||
        m_reserved.contains(playerId) || !m_world.AddPlayer(playerId)) {
        return false;
    }
    m_emptySince.reset();
    return true;
}

bool GameRoom::Reserve(std::uint32_t playerId)
{
    if (playerId == 0 || PlayerCount() + ReservedCount() >= GameRoomOptions::PLAYERS_PER_ROOM ||
        m_world.HasPlayer(playerId) || !m_reserved.insert(playerId).second) {
        return false;
    }
    m_emptySince.reset();
    return true;
}

bool GameRoom::Claim(std::uint32_t playerId)
{
    if (!m_reserved.contains(playerId) || !m_world.AddPlayer(playerId)) {
        return false;
    }
    m_reserved.erase(playerId);
    return true;
}

void GameRoom::Leave(std::uint32_t playerId, Clock::time_point now)
{
    if (m_reserved.erase(playerId) == 0 && !m_world.HasPlayer(playerId)) {
        return;
    }
    m_world.RemovePlayer(playerId);
    if (PlayerCount() + ReservedCount() == 0) {
        m_emptySince = now;
    }
}

bool GameRoom::Apply(InputCommand input)
{
    return m_world.Apply(input);
}

void GameRoom::Step()
{
    if (PlayerCount() != 0) {
        m_world.Step();
    }
}

std::size_t GameRoom::PlayerCount() const
{
    return m_world.PlayerCount();
}

bool GameRoom::Expired(Clock::time_point now, std::chrono::seconds lifetime) const
{
    return m_emptySince && now - *m_emptySince >= lifetime;
}

std::size_t GameRoom::ReservedCount() const
{
    return m_reserved.size();
}

WorldSnapshot GameRoom::Snapshot() const
{
    return m_world.Snapshot();
}

} // namespace MoongTangE
