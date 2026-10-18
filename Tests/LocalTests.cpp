#include "Client/Transport/LocalTransport.h"
#include "TestSupport.h"

#include <memory>
#include <optional>

int main()
{
    return MoongTangE::test::run([] {
        MOONGTANGE_CHECK(MoongTangE::test::throws([] {
            MoongTangE::LocalTransport invalid(nullptr);
        }));
        MOONGTANGE_CHECK(MoongTangE::test::throws([] {
            MoongTangE::LocalTransport invalid(std::make_shared<MoongTangE::ServerHost>());
        }));

        auto server = std::make_shared<MoongTangE::ServerHost>();
        server->Start();
        MoongTangE::LocalTransport a(server), b(server);
        MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
            return a.Connected() && b.Connected();
        }));
        MOONGTANGE_CHECK(a.PlayerId() != b.PlayerId() && a.RoomId() == b.RoomId());

        MOONGTANGE_CHECK(!a.Send({b.PlayerId(), 1, 1, 0}));
        MOONGTANGE_CHECK(a.Send({a.PlayerId(), 1, 1, 0}));
        std::optional<MoongTangE::WorldSnapshot> state;
        MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
            state = a.Receive();
            return state && state->players.size() == 2 && state->players[0].velocity.x > 0.5;
        }));
        MOONGTANGE_CHECK(MoongTangE::test::approx(state->players[0].angularVelocity.z,
                                                  -state->players[0].velocity.x / 0.5));
        MOONGTANGE_CHECK(state->players[0].orientation != MoongTangE::Quaternion{});
        MOONGTANGE_CHECK(state->players[1].velocity.x == 0);

        a.Close();
        a.Close();
        MOONGTANGE_CHECK(!a.Connected());
        MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
            const auto room = server->SnapshotFor(b.PlayerId());
            return room && room->world.players.size() == 1;
        }));

        server->Stop();
        MOONGTANGE_CHECK(!b.Connected());

        server->Start();
        MOONGTANGE_CHECK(!b.Connected());
    });
}
