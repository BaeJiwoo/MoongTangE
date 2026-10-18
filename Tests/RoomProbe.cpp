#include "Client/Transport/TcpClientTransport.h"
#include "Common/Utility/CommandLine.h"
#include "TestSupport.h"

#include <map>
#include <memory>
#include <set>
#include <vector>

int main(int argc, char** argv)
{
    return MoongTangE::test::run([&] {
        using namespace MoongTangE;
        MOONGTANGE_CHECK(argc == 2);
        const auto endpoint = endpoint_option(argv[1]);
        std::vector<std::unique_ptr<TcpClientTransport>> clients;
        std::map<std::uint64_t, std::set<std::uint32_t>> memberships;
        for (int index = 0; index < 9; ++index) {
            clients.push_back(std::make_unique<TcpClientTransport>(endpoint.host, endpoint.port));
        }
        for (const auto& client : clients) {
            MOONGTANGE_CHECK(test::wait_for([&] {
                return client->Connected();
            }));
            memberships[client->RoomId()].insert(client->PlayerId());
        }
        MOONGTANGE_CHECK(memberships.size() == 3);
        for (const auto& client : clients) {
            const auto& expected = memberships.at(client->RoomId());
            MOONGTANGE_CHECK(expected.size() == 3);
            MOONGTANGE_CHECK(test::wait_for([&] {
                const auto snapshot = client->Receive();
                if (!snapshot) {
                    return false;
                }
                std::set<std::uint32_t> actual;
                for (const auto& player : snapshot->players) {
                    actual.insert(player.id);
                }
                return actual == expected;
            }));
        }

        TcpClientTransport excess(endpoint.host, endpoint.port);
        MOONGTANGE_CHECK(test::wait_for([&] {
            return !excess.Error().empty();
        }));
        MOONGTANGE_CHECK(excess.Error() == "All game rooms are full");
        const auto vacatedRoom = clients.front()->RoomId();
        clients.front()->Close();
        IClientTransport* observer = nullptr;
        for (const auto& client : clients) {
            if (client->RoomId() == vacatedRoom) {
                observer = client.get();
                break;
            }
        }
        MOONGTANGE_CHECK(observer != nullptr);
        MOONGTANGE_CHECK(test::wait_for([&] {
            const auto snapshot = observer->Receive();
            return snapshot && snapshot->players.size() == 2;
        }));
        TcpClientTransport replacement(endpoint.host, endpoint.port);
        MOONGTANGE_CHECK(test::wait_for([&] {
            return replacement.Connected();
        }));
        MOONGTANGE_CHECK(replacement.RoomId() == vacatedRoom);
    });
}
