#include "Server/Core/GameRoom.h"

#include "Server/Core/GameRoomOptions.h"

namespace MoongTangE {

bool GameRoom::Join(std::uint32_t playerId)
{
    if (PlayerCount() >= GameRoomOptions::PLAYERS_PER_ROOM || !m_world.AddPlayer(playerId)) {
        return false;
    }
    m_emptySince.reset();
    return true;
}

void GameRoom::Leave(std::uint32_t playerId, Clock::time_point now)
{
    if (!m_world.HasPlayer(playerId)) {
        return;
    }
    m_world.RemovePlayer(playerId);
    if (PlayerCount() == 0) {
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

WorldSnapshot GameRoom::Snapshot() const
{
    return m_world.Snapshot();
}

} // namespace MoongTangE
