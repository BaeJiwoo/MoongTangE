#include "Client/Transport/LocalTransport.h"
#include "Client/Transport/TcpClientTransport.h"
#include "Common/Game/GameWorld.h"
#include "GameplayTestSupport.h"
#include "GrowthTestSupport.h"
#include "Network/Protocol/Codec.h"
#include "Server/Core/GameRound.h"
#include "Server/Network/NetworkServer.h"
#include "TestSupport.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>

namespace {

MoongTangE::PlayerState exercise(MoongTangE::IClientTransport& client)
{
    MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
        return client.Connected();
    }));

    MoongTangE::test::wait_for_gameplay(client);
    MoongTangE::PlayerState result;
    const auto id = client.PlayerId();
    auto settled = [&] {
        if (auto state = client.Receive()) {
            for (const auto& p : state->players) {
                if (p.id == id && p.position.y == 0.5) {
                    result = p;
                    return true;
                }
            }
        }
        return false;
    };
    MOONGTANGE_CHECK(MoongTangE::test::wait_for(settled));

    MOONGTANGE_CHECK(client.Send({id, 1, 1, -1}));
    MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
        if (auto state = client.Receive()) {
            for (const auto& p : state->players) {
                if (p.id == id && p.velocity.x > 0) {
                    MOONGTANGE_CHECK(p.velocity.Length() <=
                                     MoongTangE::simulation::PLAYER_MOVE_SPEED);
                    MOONGTANGE_CHECK(MoongTangE::test::approx(p.velocity.x, -p.velocity.z));
                    MOONGTANGE_CHECK(p.angularVelocity == MoongTangE::Vector3{});
                    MOONGTANGE_CHECK(p.orientation != MoongTangE::Quaternion{} &&
                                     p.orientation.Unit());
                    return true;
                }
            }
        }
        return false;
    }));

    MOONGTANGE_CHECK(client.Send({id, 2, 0, 0}));
    MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
        if (auto state = client.Receive()) {
            for (const auto& p : state->players) {
                if (p.id == id && p.velocity == MoongTangE::Vector3{}) {
                    result = p;
                    return true;
                }
            }
        }
        return false;
    }));
    return result;
}

void deterministic_transport_equivalence()
{
    using namespace MoongTangE;

    // Identical input schedule: serialize/decode the TCP path, then compare every tick.
    GameWorld direct, serialized;
    MOONGTANGE_CHECK(direct.AddPlayer(1));
    MOONGTANGE_CHECK(serialized.AddPlayer(1));

    for (std::uint64_t tick = 0; tick < 600; ++tick) {
        InputCommand input{1, tick + 1, tick < 200 ? 1.0 : -1.0, tick % 120 < 60 ? 1.0 : 0.0};
        MOONGTANGE_CHECK(direct.Apply(input));
        MOONGTANGE_CHECK(serialized.Apply(wire::read_input(wire::input(input), 1)));

        direct.Step();
        serialized.Step();

        auto a = direct.Snapshot(),
             b = wire::read_snapshot(wire::snapshot({1, serialized.Snapshot()})).world;
        MOONGTANGE_CHECK(a.tick == b.tick);
        MOONGTANGE_CHECK(a.players[0].position == b.players[0].position);
        MOONGTANGE_CHECK(a.players[0].velocity == b.players[0].velocity);
        MOONGTANGE_CHECK(a.players[0].orientation == b.players[0].orientation);
        MOONGTANGE_CHECK(a.players[0].angularVelocity == b.players[0].angularVelocity);
        MOONGTANGE_CHECK(a.collectibles.size() == b.collectibles.size());
        MOONGTANGE_CHECK(a.players[0].radius == b.players[0].radius &&
                         a.players[0].foodEaten == b.players[0].foodEaten);
        MOONGTANGE_CHECK(a.mines.size() == b.mines.size());
        for (std::size_t index = 0; index < a.mines.size(); ++index) {
            MOONGTANGE_CHECK(a.mines[index].readyTick == b.mines[index].readyTick);
        }
        for (std::size_t index = 0; index < a.collectibles.size(); ++index) {
            MOONGTANGE_CHECK(a.collectibles[index].id == b.collectibles[index].id);
            MOONGTANGE_CHECK(a.collectibles[index].kind == b.collectibles[index].kind);
            MOONGTANGE_CHECK(a.collectibles[index].position == b.collectibles[index].position);
            MOONGTANGE_CHECK(a.collectibles[index].orientation ==
                             b.collectibles[index].orientation);
            MOONGTANGE_CHECK(a.collectibles[index].scale == b.collectibles[index].scale);
        }
    }
}

void asynchronous_transport_invariants()
{
    using namespace MoongTangE;

    // Real asynchronous transports: compare physical invariants, allowing arrival-time
    // differences.
    auto localCore = std::make_shared<ServerHost>(), tcpCore = std::make_shared<ServerHost>();
    localCore->Start();
    tcpCore->Start();

    LocalTransport local(localCore);
    NetworkServer server(tcpCore, "127.0.0.1", 0);
    TcpClientTransport tcp("127.0.0.1", server.Port());

    const auto a = exercise(local), b = exercise(tcp);
    MOONGTANGE_CHECK(a.position.y == b.position.y && a.velocity == b.velocity);
    MOONGTANGE_CHECK(a.angularVelocity == Vector3{} && b.angularVelocity == Vector3{});
    MOONGTANGE_CHECK(a.position.x > 0 && b.position.x > 0 && a.position.z < 0 && b.position.z < 0);

    local.Close();
    tcp.Close();
    MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
        return std::ranges::all_of(localCore->Snapshots(),
                                   [](const auto& room) {
                                       return room.world.players.empty();
                                   }) &&
               std::ranges::all_of(tcpCore->Snapshots(), [](const auto& room) {
                   return room.world.players.empty();
               });
    }));
}

void defeat_and_restart_cross_both_transports(bool localVictim)
{
    using namespace MoongTangE;
    GameRoomOptions options;
    options.m_worldSeed = 24;
    auto core = std::make_shared<ServerHost>(options);
    core->Start();
    NetworkServer server(core, "127.0.0.1", 0);

    std::unique_ptr<IClientTransport> winner;
    if (localVictim) {
        winner = std::make_unique<TcpClientTransport>("127.0.0.1", server.Port());
    } else {
        winner = std::make_unique<LocalTransport>(core);
    }
    MOONGTANGE_CHECK(test::wait_for([&] {
        return winner->Connected();
    }));
    std::unique_ptr<IClientTransport> victim;
    if (localVictim) {
        victim = std::make_unique<LocalTransport>(core);
    } else {
        victim = std::make_unique<TcpClientTransport>("127.0.0.1", server.Port());
    }
    MOONGTANGE_CHECK(test::wait_for([&] {
        return victim->Connected();
    }));
    const auto playerId = victim->PlayerId();
    const auto roomId = victim->RoomId();
    test::wait_for_gameplay(*victim);
    WorldSnapshot state;
    std::uint64_t winnerSequence = 0;
    MOONGTANGE_CHECK(test::wait_for(
        [&] {
            if (auto received = victim->Receive()) {
                state = std::move(*received);
            }
            if (state.players.size() != 2) {
                return false;
            }
            const auto& grown = state.players[0];
            if (grown.radius >= 0.8) {
                MOONGTANGE_CHECK(winner->Send({winner->PlayerId(), ++winnerSequence, 0, 0}));
                return true;
            }
            auto input = test::steer_to(state, winner->PlayerId(),
                                        test::nearest_food(state, winner->PlayerId()));
            input.sequence = ++winnerSequence;
            MOONGTANGE_CHECK(winner->Send(input));
            return false;
        },
        std::chrono::seconds(12)));
    std::uint64_t sequence = 0;
    MOONGTANGE_CHECK(test::wait_for(
        [&] {
            if (auto received = victim->Receive()) {
                state = std::move(*received);
            }
            if (state.players[1].deathTick != 0) {
                return true;
            }
            auto input = test::steer_to(state, winner->PlayerId(), state.players[1].position);
            input.sequence = ++winnerSequence;
            MOONGTANGE_CHECK(winner->Send(input));
            return false;
        },
        std::chrono::seconds(12)));
    const auto deathTick = state.players[1].deathTick;
    MOONGTANGE_CHECK(victim->Connected() && state.players[0].deathTick == 0);

    MOONGTANGE_CHECK(test::wait_for([&] {
        MOONGTANGE_CHECK(victim->Send({playerId, ++sequence, 0, 0, deathTick}));
        if (auto received = victim->Receive()) {
            state = std::move(*received);
            return state.players[1].deathTick == 0;
        }
        return false;
    }));
    MOONGTANGE_CHECK(victim->PlayerId() == playerId && victim->RoomId() == roomId);
    MOONGTANGE_CHECK((state.players[1].position - state.players[0].position).Length() > 3);
    MOONGTANGE_CHECK(state.players.size() == 2);
    victim->Close();
    winner->Close();
}

void round_results_and_replay_cross_the_protocol()
{
    using namespace MoongTangE;
    GameRound direct(24), serialized(24);
    MOONGTANGE_CHECK(direct.AddPlayer(1) && direct.AddPlayer(2));
    MOONGTANGE_CHECK(serialized.AddPlayer(1) && serialized.AddPlayer(2));
    bool observedWinner = false;
    for (std::uint64_t tick = 0; tick < 6000; ++tick) {
        const auto state = direct.Snapshot();
        auto input = test::steer_to(state, 1, test::nearest_food(state, 1));
        input.sequence = tick + 1;
        input.nextRoundId = state.round.phase == RoundPhase::RESULTS ? state.round.id : 0;
        MOONGTANGE_CHECK(direct.Apply(input));
        MOONGTANGE_CHECK(serialized.Apply(wire::read_input(wire::input(input), 1)));
        direct.Step();
        serialized.Step();
        const auto a = direct.Snapshot();
        const auto b = wire::read_snapshot(wire::snapshot({1, serialized.Snapshot()})).world;
        MOONGTANGE_CHECK(a.tick == b.tick && a.round.id == b.round.id &&
                         a.round.phase == b.round.phase &&
                         a.round.remainingTicks == b.round.remainingTicks);
        for (std::size_t index = 0; index < a.players.size(); ++index) {
            MOONGTANGE_CHECK(a.players[index].position == b.players[index].position &&
                             a.players[index].roundScore == b.players[index].roundScore &&
                             a.players[index].roundWinner == b.players[index].roundWinner);
            observedWinner = observedWinner || b.players[index].roundWinner;
        }
    }
    MOONGTANGE_CHECK(observedWinner && direct.Snapshot().round.id == 2);
}

} // namespace

int main()
{
    return MoongTangE::test::run([] {
        deterministic_transport_equivalence();
        round_results_and_replay_cross_the_protocol();
        asynchronous_transport_invariants();
        defeat_and_restart_cross_both_transports(false);
        defeat_and_restart_cross_both_transports(true);
    });
}
