#pragma once

#include "Common/Protocol/WorldSnapshot.h"

#include <cstdint>

namespace MoongTangE {

// Presentation follows confirmed server state; restart is repeated until acknowledged.
class PlayerLifeState
{
  public:
    void Reset()
    {
        m_deathTick = 0;
        m_restartRequested = false;
    }

    void Observe(const WorldSnapshot& state, std::uint32_t playerId)
    {
        for (const auto& player : state.players) {
            if (player.id == playerId && player.deathTick != m_deathTick) {
                m_deathTick = player.deathTick;
                m_restartRequested = false;
            }
        }
    }

    bool Defeated() const
    {
        return m_deathTick != 0;
    }

    void RequestRestart()
    {
        m_restartRequested = Defeated();
    }

    std::uint64_t RestartDeathTick() const
    {
        return m_restartRequested ? m_deathTick : 0;
    }

  private:
    std::uint64_t m_deathTick{};
    bool m_restartRequested{};
};

} // namespace MoongTangE
