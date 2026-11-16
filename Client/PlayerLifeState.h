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
        m_round = {};
        m_nextRoundRequested = false;
    }

    void Observe(const WorldSnapshot& state, std::uint32_t playerId)
    {
        if (state.round.id != m_round.id || state.round.phase != m_round.phase) {
            m_nextRoundRequested = false;
        }
        m_round = state.round;
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
        return m_restartRequested && !RoundFinished() ? m_deathTick : 0;
    }

    bool RoundFinished() const
    {
        return m_round.phase == RoundPhase::RESULTS;
    }

    bool MovementEnabled() const
    {
        return !Defeated() &&
               (m_round.phase == RoundPhase::PLAYING || m_round.phase == RoundPhase::FREE_PLAY);
    }

    void RequestNextRound()
    {
        m_nextRoundRequested = RoundFinished();
    }

    std::uint64_t NextRoundId() const
    {
        return m_nextRoundRequested ? m_round.id : 0;
    }

  private:
    std::uint64_t m_deathTick{};
    bool m_restartRequested{};
    RoundState m_round;
    bool m_nextRoundRequested{};
};

} // namespace MoongTangE
