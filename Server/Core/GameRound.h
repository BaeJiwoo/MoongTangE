#pragma once

#include "Common/Game/CollectionScore.h"
#include "Common/Game/GameWorld.h"

#include <cstddef>
#include <cstdint>
#include <map>

namespace MoongTangE {

// Owns the room's world and round rules. Access only from the server tick thread.
class GameRound
{
  public:
    explicit GameRound(std::uint64_t seed = simulation::DEFAULT_WORLD_SEED);
    bool AddPlayer(std::uint32_t playerId);
    void RemovePlayer(std::uint32_t playerId);
    bool HasPlayer(std::uint32_t playerId) const;
    std::size_t PlayerCount() const;
    bool Apply(InputCommand input);
    void Step();
    WorldSnapshot Snapshot() const;

  private:
    struct Participant
    {
        CollectionScore score;
        std::uint64_t lastSequence{};
        bool replayRequested{};
        bool winner{};
    };

    void StartNextRound();
    void FinishRound();

    std::uint64_t m_seed;
    GameWorld m_world;
    RoundState m_state{1, RoundPhase::COUNTDOWN, simulation::ROUND_COUNTDOWN_TICKS, false};
    std::map<std::uint32_t, Participant> m_participants;
};

} // namespace MoongTangE
