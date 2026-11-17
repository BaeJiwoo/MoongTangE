#include "GrowthTestSupport.h"
#include "Server/Core/GameRoom.h"

using namespace MoongTangE;

namespace {

void play_continues_past_old_time_and_food_limits()
{
    GameRoom room;
    MOONGTANGE_CHECK(room.Join(1));
    MOONGTANGE_CHECK(room.Snapshot().round.phase == RoundPhase::FREE_PLAY);
    MOONGTANGE_CHECK(room.Apply({1, 1, 0, -1}));
    room.Step();
    MOONGTANGE_CHECK(room.Snapshot().players[0].velocity.z < 0); // No countdown.
    for (std::uint64_t tick = 0; tick < 18000; ++tick) {
        const auto before = room.Snapshot();
        auto input = test::steer_to(before, 1, test::nearest_food(before, 1));
        input.nextRoundId = 1; // Old replay requests must never reset a continuous room.
        MOONGTANGE_CHECK(room.Apply(input));
        MOONGTANGE_CHECK(!room.Apply(input));
        room.Step();
        const auto state = room.Snapshot();
        MOONGTANGE_CHECK(state.tick == before.tick + 1 && state.round.id == 0);
        MOONGTANGE_CHECK(state.round.phase == RoundPhase::FREE_PLAY &&
                         state.round.remainingTicks == 0);
        MOONGTANGE_CHECK(!state.players[0].roundWinner && state.players[0].roundScore == 0);
        MOONGTANGE_CHECK(state.players[0].deathTick == 0);
    }
    const auto state = room.Snapshot();
    MOONGTANGE_CHECK(state.players[0].foodEaten > 20);
    MOONGTANGE_CHECK(state.tick > 90 * simulation::TICKS_PER_SECOND);
    MOONGTANGE_CHECK(!state.collectibles.empty());
    MOONGTANGE_CHECK(std::ranges::any_of(state.collectibles, [](const auto& food) {
        return food.id > simulation::INITIAL_COLLECTIBLES;
    }));
    MOONGTANGE_CHECK(room.Apply({1, state.tick + 1, 0, 1}));
    for (int tick = 0; tick < 10; ++tick) {
        room.Step();
    }
    MOONGTANGE_CHECK(room.Snapshot().players[0].velocity.z > 0);
}

void joins_and_departures_do_not_reset_the_arena()
{
    GameRoom room;
    const auto now = GameRoom::Clock::time_point{};
    MOONGTANGE_CHECK(room.Join(1));
    for (int tick = 0; tick < 1000; ++tick) {
        const auto state = room.Snapshot();
        MOONGTANGE_CHECK(room.Apply(test::steer_to(state, 1, test::nearest_food(state, 1))));
        room.Step();
    }
    const auto original = room.Snapshot();
    MOONGTANGE_CHECK(original.players[0].foodEaten > 0);
    MOONGTANGE_CHECK(room.Join(2));
    MOONGTANGE_CHECK(room.Snapshot().tick == original.tick);
    MOONGTANGE_CHECK(room.Snapshot().players[0].radius == original.players[0].radius);
    room.Leave(2, now);
    MOONGTANGE_CHECK(room.Snapshot().players[0].position == original.players[0].position);
    room.Leave(1, now);
    room.Step();
    const auto empty = room.Snapshot();
    MOONGTANGE_CHECK(empty.tick == original.tick && empty.players.empty());
    MOONGTANGE_CHECK(room.Join(3));
    const auto returned = room.Snapshot();
    MOONGTANGE_CHECK(returned.tick == original.tick &&
                     returned.players[0].radius == simulation::PLAYER_RADIUS);
    MOONGTANGE_CHECK(returned.collectibles.size() == original.collectibles.size());
    for (std::size_t index = 0; index < original.collectibles.size(); ++index) {
        MOONGTANGE_CHECK(returned.collectibles[index].id == original.collectibles[index].id);
        MOONGTANGE_CHECK(returned.collectibles[index].position ==
                         original.collectibles[index].position);
    }
    for (std::size_t index = 0; index < original.mines.size(); ++index) {
        MOONGTANGE_CHECK(returned.mines[index].readyTick == original.mines[index].readyTick);
    }
}

} // namespace

int main()
{
    return test::run([] {
        play_continues_past_old_time_and_food_limits();
        joins_and_departures_do_not_reset_the_arena();
    });
}
