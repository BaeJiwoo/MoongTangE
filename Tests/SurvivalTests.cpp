#include "GrowthTestSupport.h"

using namespace MoongTangE;

namespace {

void eating_and_restart(std::uint32_t eaterId, std::uint32_t victimId)
{
    GameWorld world;
    MOONGTANGE_CHECK(world.AddPlayer(eaterId));
    test::grow_player(world, eaterId, 1.3);
    MOONGTANGE_CHECK(world.AddPlayer(victimId));
    for (int tick = 0; tick < 3000; ++tick) {
        const auto state = world.Snapshot();
        const auto& victim = *std::ranges::find(state.players, victimId, &PlayerState::id);
        if (victim.deathTick != 0) {
            break;
        }
        MOONGTANGE_CHECK(world.Apply(test::steer_to(state, eaterId, victim.position)));
        world.Step();
    }
    auto state = world.Snapshot();
    auto victim = *std::ranges::find(state.players, victimId, &PlayerState::id);
    MOONGTANGE_CHECK(victim.deathTick != 0 && world.PlayerCount() == 2);
    const auto deathTick = victim.deathTick;
    const auto deathPosition = victim.position;
    for (int tick = 0; tick < 30; ++tick) {
        MOONGTANGE_CHECK(world.Apply({victimId, state.tick + 1, 1, 1, deathTick + 1}));
        world.Step();
        state = world.Snapshot();
        victim = *std::ranges::find(state.players, victimId, &PlayerState::id);
        MOONGTANGE_CHECK(victim.deathTick == deathTick && victim.position == deathPosition);
    }
    MOONGTANGE_CHECK(world.Apply({victimId, state.tick + 1, 0, 0, deathTick}));
    world.Step();
    state = world.Snapshot();
    victim = *std::ranges::find(state.players, victimId, &PlayerState::id);
    MOONGTANGE_CHECK(victim.deathTick == 0 && victim.radius == simulation::PLAYER_RADIUS);
    MOONGTANGE_CHECK(victim.foodEaten == 0 && victim.mineHitTick == 0);
    MOONGTANGE_CHECK((victim.position - deathPosition).Length() > 2);
}

void mines_shrink_only_large_players_and_rearm()
{
    GameWorld small(1);
    MOONGTANGE_CHECK(small.AddPlayer(1));
    const auto mine = small.Snapshot().mines.front();
    for (int tick = 0; tick < 250; ++tick) {
        const auto state = small.Snapshot();
        MOONGTANGE_CHECK(small.Apply(test::steer_to(state, 1, mine.position)));
        small.Step();
    }
    const auto smallState = small.Snapshot();
    MOONGTANGE_CHECK((smallState.players[0].position - mine.position).Length() < 0.8);
    MOONGTANGE_CHECK(smallState.players[0].radius < simulation::MINE_TRIGGER_RADIUS);
    MOONGTANGE_CHECK(smallState.players[0].mineHitTick == 0 && smallState.mines[0].readyTick == 0);
    // Use a fresh world for the large-character path; consuming food is the only way to grow.
    GameWorld world;
    MOONGTANGE_CHECK(world.AddPlayer(1));
    test::grow_player(world, 1, 1.4);
    auto state = world.Snapshot();
    const auto target = std::ranges::min_element(state.mines, {}, [&](const auto& item) {
                            return (item.position - state.players[0].position).Length();
                        })->position;
    bool hit = false;
    for (int tick = 0; tick < 3000; ++tick) {
        state = world.Snapshot();
        MOONGTANGE_CHECK(world.Apply(test::steer_to(state, 1, target)));
        world.Step();
        const auto after = world.Snapshot();
        if (after.players[0].mineHitTick != state.players[0].mineHitTick) {
            MOONGTANGE_CHECK(after.players[0].radius < state.players[0].radius);
            MOONGTANGE_CHECK(after.players[0].deathTick == 0);
            MOONGTANGE_CHECK(after.players[0].foodEaten == state.players[0].foodEaten);
            MOONGTANGE_CHECK(test::approx(
                std::pow(after.players[0].radius, 3),
                std::max(std::pow(simulation::PLAYER_RADIUS, 3),
                         std::pow(state.players[0].radius, 3) * simulation::MINE_RETAINED_VOLUME)));
            const auto hitTick = after.players[0].mineHitTick;
            for (std::uint64_t frame = 0; frame < simulation::MINE_IMMUNITY_TICKS - 1; ++frame) {
                world.Step();
                MOONGTANGE_CHECK(world.Snapshot().players[0].mineHitTick == hitTick);
            }
            MOONGTANGE_CHECK(std::ranges::any_of(after.mines, [&](const auto& item) {
                return item.readyTick == hitTick + simulation::MINE_RESPAWN_TICKS;
            }));
            hit = true;
            break;
        }
    }
    MOONGTANGE_CHECK(hit);
}

void equal_players_do_not_eat_each_other()
{
    GameWorld world;
    MOONGTANGE_CHECK(world.AddPlayer(1) && world.AddPlayer(2));
    for (std::uint64_t tick = 1; tick < 180; ++tick) {
        MOONGTANGE_CHECK(world.Apply({1, tick, 1, 0}) && world.Apply({2, tick, -1, 0}));
        world.Step();
        const auto state = world.Snapshot();
        MOONGTANGE_CHECK(state.players[0].deathTick == 0 && state.players[1].deathTick == 0);
        MOONGTANGE_CHECK(state.players[1].position.x - state.players[0].position.x >= 1 - 1e-9);
    }
}

} // namespace

int main()
{
    return test::run([] {
        eating_and_restart(1, 2);
        eating_and_restart(2, 1);
        mines_shrink_only_large_players_and_rearm();
        equal_players_do_not_eat_each_other();
    });
}
