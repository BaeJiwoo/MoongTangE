#include "Client/Transport/TcpClientTransport.h"
#include "Common/SimulationConfig.h"
#include "Common/Utility/CommandLine.h"
#include "TestSupport.h"

int main(int argc, char** argv) {
    return MoongTangE::test::run([&] {
        MOONGTANGE_CHECK(argc == 2);
        const auto [host, port] = MoongTangE::endpointOption(argv[1]);
        MoongTangE::TcpClientTransport client(host, port);
        MOONGTANGE_CHECK(MoongTangE::test::waitFor([&] {
            return client.connected();
        }));
        const auto id = client.playerId();
        MOONGTANGE_CHECK(client.send({id, 1, 1, 0}));
        MOONGTANGE_CHECK(MoongTangE::test::waitFor([&] {
            if (auto state = client.receive()) {
                for (const auto& p : state->players) {
                    if (p.id == id && p.position.x > MoongTangE::simulation::spawnX(id) +
                                                         MoongTangE::simulation::PLAYER_RADIUS) {
                        return true;
                    }
                }
            }
            return false;
        }));
        client.close();
    });
}
