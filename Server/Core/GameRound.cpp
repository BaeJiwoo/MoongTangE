#include "Server/Core/GameRound.h"

#include <algorithm>

namespace MoongTangE {

GameRound::GameRound(std::uint64_t seed) : m_seed(seed), m_world(seed) {}

bool GameRound::AddPlayer(std::uint32_t playerId)
{
    if (!m_world.AddPlayer(playerId)) {
        return false;
    }
    m_participants.emplace(playerId, Participant{});
    if (m_state.phase != RoundPhase::RESULTS && PlayerCount() > 1) {
        m_state.competitive = true;
    }
    return true;
}

void GameRound::RemovePlayer(std::uint32_t playerId)
{
    if (!HasPlayer(playerId)) {
        return;
    }
    m_world.RemovePlayer(playerId);
    m_participants.erase(playerId);
    // An empty room must not hand an abandoned, nearly expired game to its next visitor.
    if (m_participants.empty()) {
        StartNextRound();
    }
}

bool GameRound::HasPlayer(std::uint32_t playerId) const
{
    return m_world.HasPlayer(playerId);
}

std::size_t GameRound::PlayerCount() const
{
    return m_participants.size();
}

bool GameRound::Apply(InputCommand input)
{
    const auto found = m_participants.find(input.playerId);
    if (!input.Valid() || found == m_participants.end() ||
        input.sequence <= found->second.lastSequence) {
        return false;
    }
    auto& participant = found->second;
    participant.lastSequence = input.sequence;
    if (m_state.phase == RoundPhase::RESULTS && input.nextRoundId == m_state.id) {
        participant.replayRequested = true;
    }
    return m_state.phase != RoundPhase::PLAYING || m_world.Apply(input);
}

void GameRound::Step()
{
    if (m_participants.empty()) {
        return;
    }
    if (m_state.phase != RoundPhase::PLAYING) {
        m_world.AdvanceClock();
        if (m_state.remainingTicks > 0) {
            --m_state.remainingTicks;
        }
        if (m_state.remainingTicks == 0) {
            if (m_state.phase == RoundPhase::COUNTDOWN) {
                m_state.phase = RoundPhase::PLAYING;
                m_state.remainingTicks = simulation::ROUND_DURATION_TICKS;
            } else if (std::ranges::any_of(m_participants, [](const auto& entry) {
                           return entry.second.replayRequested;
                       })) {
                StartNextRound();
            }
        }
        return;
    }

    m_world.Step();
    const auto state = m_world.Snapshot();
    for (const auto& player : state.players) {
        m_participants.at(player.id).score.Observe(player.foodEaten);
    }
    --m_state.remainingTicks;
    const bool soloTargetReached =
        !m_state.competitive &&
        m_participants.begin()->second.score.Points() >= simulation::SOLO_TARGET_SCORE;
    if (m_state.remainingTicks == 0 || soloTargetReached) {
        FinishRound();
    }
}

void GameRound::FinishRound()
{
    std::uint32_t bestScore = 0;
    for (const auto& [id, participant] : m_participants) {
        (void)id;
        bestScore = std::max(bestScore, participant.score.Points());
    }
    for (auto& [id, participant] : m_participants) {
        (void)id;
        participant.winner =
            participant.score.Points() == bestScore &&
            (m_state.competitive ? bestScore > 0 : bestScore >= simulation::SOLO_TARGET_SCORE);
    }
    m_state.phase = RoundPhase::RESULTS;
    m_state.remainingTicks = simulation::ROUND_RESULTS_TICKS;
}

void GameRound::StartNextRound()
{
    const auto tick = m_world.Snapshot().tick;
    ++m_state.id;
    m_state.phase = RoundPhase::COUNTDOWN;
    m_state.remainingTicks = simulation::ROUND_COUNTDOWN_TICKS;
    m_state.competitive = m_participants.size() > 1;
    m_world = GameWorld(m_seed + m_state.id - 1, tick);
    for (auto& [id, participant] : m_participants) {
        // Keep sequence high-water marks across rounds to reject old in-flight inputs.
        participant.score = CollectionScore{};
        participant.replayRequested = false;
        participant.winner = false;
        m_world.AddPlayer(id);
    }
}

WorldSnapshot GameRound::Snapshot() const
{
    auto state = m_world.Snapshot();
    state.round = m_state;
    for (auto& player : state.players) {
        const auto& participant = m_participants.at(player.id);
        player.roundScore = participant.score.Points();
        player.roundWinner = participant.winner;
    }
    return state;
}

} // namespace MoongTangE
