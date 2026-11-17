#include "Common/Game/CollectionScore.h"
#include "GrowthTestSupport.h"
#include "Server/Core/GameRound.h"
#include "TestSupport.h"

#include <algorithm>
#include <cstdint>

namespace {

using namespace MoongTangE;

void advance(GameRound& round, std::uint64_t ticks)
{
    for (std::uint64_t tick = 0; tick < ticks; ++tick) {
        round.Step();
    }
}

InputCommand collect_nearest(const WorldSnapshot& state)
{
    const auto& player = state.players.front();
    Vector3 direction;
    double nearest = 1e9;
    for (const auto& collectible : state.collectibles) {
        if (collectible.attachedPlayerId == 0) {
            auto offset = collectible.position - player.position;
            offset.y = 0;
            if (offset.Length() < nearest) {
                nearest = offset.Length();
                direction = offset;
            }
        }
    }
    auto force = direction * 1.5 - player.velocity;
    force.y = 0;
    if (force.Length() > 1) {
        force = force.Normalized();
    }
    return {player.id, state.tick + 1, force.x, force.z};
}

void unique_objects_have_bounded_scores()
{
    CollectionScore score;
    score.Observe(0);
    MOONGTANGE_CHECK(score.Points() == 0);
    score.Observe(12);
    score.Observe(12);
    score.Observe(3);
    MOONGTANGE_CHECK(score.Points() == 1200);
    score.Observe(simulation::MAX_FOOD_PER_ROUND + 1);
    MOONGTANGE_CHECK(score.Points() ==
                     simulation::MAX_FOOD_PER_ROUND * simulation::COLLECTION_POINTS);
}

void countdown_timeout_and_replay_are_authoritative()
{
    GameRound round(123456789);
    MOONGTANGE_CHECK(round.AddPlayer(1));
    const auto original = round.Snapshot();
    MOONGTANGE_CHECK(round.Apply({1, 50, 1, 0, 0, 1}));
    advance(round, simulation::ROUND_COUNTDOWN_TICKS - 1);
    auto state = round.Snapshot();
    MOONGTANGE_CHECK(state.round.phase == RoundPhase::COUNTDOWN && state.round.remainingTicks == 1);
    MOONGTANGE_CHECK(state.players[0].position == original.players[0].position);
    round.Step();
    MOONGTANGE_CHECK(round.Snapshot().round.remainingTicks == simulation::ROUND_DURATION_TICKS);
    advance(round, simulation::ROUND_DURATION_TICKS - 1);
    MOONGTANGE_CHECK(round.Snapshot().round.phase == RoundPhase::PLAYING);
    round.Step();
    state = round.Snapshot();
    MOONGTANGE_CHECK(state.round.phase == RoundPhase::RESULTS && !state.players[0].roundWinner);
    const auto resultPosition = state.players[0].position;
    MOONGTANGE_CHECK(round.Apply({1, 51, 1, 1, 0, state.round.id + 1}));
    advance(round, simulation::ROUND_RESULTS_TICKS + 10);
    MOONGTANGE_CHECK(round.Snapshot().round.id == state.round.id);
    MOONGTANGE_CHECK(round.Snapshot().players[0].position == resultPosition);
    MOONGTANGE_CHECK(!round.Apply({1, 51, 0, 0, 0, state.round.id}));
    MOONGTANGE_CHECK(round.Apply({1, 52, 0, 0, 0, state.round.id}));
    round.Step();
    const auto replay = round.Snapshot();
    MOONGTANGE_CHECK(replay.round.id == state.round.id + 1 && replay.tick > state.tick);
    MOONGTANGE_CHECK(replay.round.phase == RoundPhase::COUNTDOWN && replay.players[0].id == 1);
    MOONGTANGE_CHECK(replay.players[0].roundScore == 0 && replay.players[0].deathTick == 0);
    MOONGTANGE_CHECK(replay.collectibles[0].position != original.collectibles[0].position);
    MOONGTANGE_CHECK(!round.Apply({1, 52, 1, 1}));
    MOONGTANGE_CHECK(round.Apply({1, 53, 0, 0, 0, state.round.id}));
    advance(round, simulation::ROUND_COUNTDOWN_TICKS + simulation::ROUND_DURATION_TICKS +
                       simulation::ROUND_RESULTS_TICKS);
    MOONGTANGE_CHECK(round.Snapshot().round.id == replay.round.id); // Stale replay cannot repeat.
}

void solo_goal_is_reachable_and_reproducible()
{
    GameRound first, replay;
    MOONGTANGE_CHECK(first.AddPlayer(1) && replay.AddPlayer(1));
    for (std::uint64_t tick = 0;
         tick <= simulation::ROUND_COUNTDOWN_TICKS + simulation::ROUND_DURATION_TICKS; ++tick) {
        const auto state = first.Snapshot();
        if (state.round.phase == RoundPhase::RESULTS) {
            break;
        }
        const auto input = collect_nearest(state);
        MOONGTANGE_CHECK(first.Apply(input) && replay.Apply(input));
        first.Step();
        replay.Step();
        const auto a = first.Snapshot(), b = replay.Snapshot();
        MOONGTANGE_CHECK(a.round.phase == b.round.phase &&
                         a.round.remainingTicks == b.round.remainingTicks);
        MOONGTANGE_CHECK(a.players[0].position == b.players[0].position &&
                         a.players[0].roundScore == b.players[0].roundScore);
    }
    const auto result = first.Snapshot();
    MOONGTANGE_CHECK(result.round.phase == RoundPhase::RESULTS && result.players[0].roundWinner);
    MOONGTANGE_CHECK(result.players[0].roundScore >= simulation::SOLO_TARGET_SCORE);
    MOONGTANGE_CHECK(result.tick <
                     simulation::ROUND_COUNTDOWN_TICKS + simulation::ROUND_DURATION_TICKS);
    MOONGTANGE_CHECK(first.Apply({1, result.tick + 1, 0, 0, 0, result.round.id}));
    advance(first, simulation::ROUND_RESULTS_TICKS - 1);
    MOONGTANGE_CHECK(first.Snapshot().round.id == result.round.id);
    first.Step();
    MOONGTANGE_CHECK(first.Snapshot().round.id == result.round.id + 1);
}

void late_join_and_departure_do_not_reset_the_clock()
{
    GameRound round(24);
    MOONGTANGE_CHECK(round.AddPlayer(1));
    advance(round, simulation::ROUND_COUNTDOWN_TICKS + 90);
    const auto beforeJoin = round.Snapshot();
    MOONGTANGE_CHECK(beforeJoin.players[0].roundScore == 0);
    MOONGTANGE_CHECK(round.AddPlayer(2));
    MOONGTANGE_CHECK(round.Snapshot().round.competitive);
    MOONGTANGE_CHECK(round.Snapshot().round.remainingTicks == beforeJoin.round.remainingTicks);
    round.RemovePlayer(2);
    MOONGTANGE_CHECK(round.Snapshot().round.competitive);
    advance(round, beforeJoin.round.remainingTicks);
    const auto result = round.Snapshot();
    MOONGTANGE_CHECK(result.round.phase == RoundPhase::RESULTS && !result.players[0].roundWinner);
    round.RemovePlayer(1);
    MOONGTANGE_CHECK(round.AddPlayer(3));
    MOONGTANGE_CHECK(round.Snapshot().round.phase == RoundPhase::COUNTDOWN);
    MOONGTANGE_CHECK(!round.Snapshot().round.competitive);
    MOONGTANGE_CHECK(round.Snapshot().players[0].roundScore == 0);
}

void competitive_ties_share_the_win()
{
    GameRound round(0);
    MOONGTANGE_CHECK(round.AddPlayer(1) && round.AddPlayer(2));
    // Steer apart to collect one object each, then brake. Both retain their earned score.
    for (std::uint64_t tick = 0;
         tick < simulation::ROUND_COUNTDOWN_TICKS + simulation::ROUND_DURATION_TICKS; ++tick) {
        const auto state = round.Snapshot();
        for (const auto& player : state.players) {
            Vector3 direction;
            double nearest = 1e9;
            if (player.roundScore == 0) {
                for (const auto& collectible : state.collectibles) {
                    if (collectible.attachedPlayerId != 0 ||
                        (player.id == 1 ? collectible.position.x > 0
                                        : collectible.position.x < 2)) {
                        continue;
                    }
                    auto offset = collectible.position - player.position;
                    offset.y = 0;
                    if (offset.Length() < nearest) {
                        nearest = offset.Length();
                        direction = offset;
                    }
                }
            }
            auto force = direction * 1.5 - player.velocity;
            force.y = 0;
            if (force.Length() > 1) {
                force = force.Normalized();
            }
            MOONGTANGE_CHECK(round.Apply({player.id, tick + 1, force.x, force.z}));
        }
        round.Step();
    }
    const auto result = round.Snapshot();
    MOONGTANGE_CHECK(result.round.phase == RoundPhase::RESULTS);
    MOONGTANGE_CHECK(result.players[0].roundScore == 100 && result.players[1].roundScore == 100);
    MOONGTANGE_CHECK(result.players[0].roundWinner && result.players[1].roundWinner);
}

void no_collection_has_no_winner()
{
    GameRound round(123456789);
    MOONGTANGE_CHECK(round.AddPlayer(1) && round.AddPlayer(2));
    advance(round, simulation::ROUND_COUNTDOWN_TICKS + simulation::ROUND_DURATION_TICKS);
    for (const auto& player : round.Snapshot().players) {
        MOONGTANGE_CHECK(player.roundScore == 0 && !player.roundWinner);
    }
}

} // namespace

int main()
{
    return MoongTangE::test::run([] {
        unique_objects_have_bounded_scores();
        countdown_timeout_and_replay_are_authoritative();
        solo_goal_is_reachable_and_reproducible();
        late_join_and_departure_do_not_reset_the_clock();
        competitive_ties_share_the_win();
        no_collection_has_no_winner();
    });
}
