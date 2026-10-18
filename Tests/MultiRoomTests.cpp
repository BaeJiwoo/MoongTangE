#include "Client/Transport/LocalTransport.h"
#include "Client/Transport/TcpClientTransport.h"
#include "Server/Network/AsioServer.h"
#include "TestSupport.h"

#include <algorithm>
#include <memory>
#include <set>
#include <vector>

namespace {

using namespace MoongTangE;

void rooms_isolate_players_and_reuse_vacancies(bool remote)
{
    auto host = std::make_shared<ServerHost>(GameRoomOptions{.m_maxRooms = 3});
    host->Start();
    AsioServer server(host, "127.0.0.1", 0);
    auto connect = [&]() -> std::unique_ptr<IClientTransport> {
        std::unique_ptr<IClientTransport> client;
        if (remote) {
            client = std::make_unique<TcpClientTransport>("127.0.0.1", server.Port());
        } else {
            client = std::make_unique<LocalTransport>(host);
        }
        MOONGTANGE_CHECK(test::wait_for([&] {
            return client->Connected();
        }));
        return client;
    };

    std::vector<std::unique_ptr<IClientTransport>> clients;
    std::vector<std::set<std::uint32_t>> members(3);
    for (std::size_t index = 0; index < 7; ++index) {
        auto client = connect();
        MOONGTANGE_CHECK(client->RoomId() == index / 3 + 1);
        members[index / 3].insert(client->PlayerId());
        clients.push_back(std::move(client));
    }
    for (std::size_t index = 0; index < clients.size(); ++index) {
        MOONGTANGE_CHECK(test::wait_for([&] {
            const auto state = clients[index]->Receive();
            if (!state) {
                return false;
            }
            std::set<std::uint32_t> ids;
            for (const auto& player : state->players) {
                ids.insert(player.id);
            }
            return ids == members[index / 3];
        }));
    }

    const auto departed = clients.front()->PlayerId();
    clients.front()->Close();
    MOONGTANGE_CHECK(test::wait_for([&] {
        const auto room = host->SnapshotFor(clients[1]->PlayerId());
        return room && room->world.players.size() == 2;
    }));
    auto replacement = connect();
    MOONGTANGE_CHECK(replacement->RoomId() == 1 && replacement->PlayerId() != departed);
    MOONGTANGE_CHECK(clients[3]->RoomId() == 2 && clients[6]->RoomId() == 3);

    MOONGTANGE_CHECK(replacement->Send({replacement->PlayerId(), 1, 1, 0}));
    MOONGTANGE_CHECK(test::wait_for([&] {
        const auto state = replacement->Receive();
        return state && std::ranges::any_of(state->players, [&](const auto& player) {
                   return player.id == replacement->PlayerId() && player.velocity.x > 0.5;
               });
    }));
    const auto other = host->SnapshotFor(clients[3]->PlayerId());
    MOONGTANGE_CHECK(other && std::ranges::all_of(other->world.players, [](const auto& player) {
                         return player.velocity.x == 0;
                     }));
}

void full_server_reports_rejection()
{
    auto host = std::make_shared<ServerHost>(GameRoomOptions{.m_maxRooms = 1});
    host->Start();
    AsioServer server(host, "127.0.0.1", 0);
    std::vector<std::unique_ptr<LocalTransport>> admitted;
    for (int index = 0; index < 3; ++index) {
        admitted.push_back(std::make_unique<LocalTransport>(host));
        MOONGTANGE_CHECK(test::wait_for([&] {
            return admitted.back()->Connected();
        }));
    }

    LocalTransport local(host);
    TcpClientTransport remote("127.0.0.1", server.Port());
    MOONGTANGE_CHECK(test::wait_for([&] {
        return !local.Error().empty() && !remote.Error().empty();
    }));
    MOONGTANGE_CHECK(local.Error() == "All game rooms are full");
    MOONGTANGE_CHECK(remote.Error() == local.Error());
    MOONGTANGE_CHECK(!local.Connected() && !remote.Connected());
    MOONGTANGE_CHECK(local.RoomId() == 0 && remote.RoomId() == 0);
}

} // namespace

int main()
{
    return MoongTangE::test::run([] {
        rooms_isolate_players_and_reuse_vacancies(false);
        rooms_isolate_players_and_reuse_vacancies(true);
        full_server_reports_rejection();
    });
}
