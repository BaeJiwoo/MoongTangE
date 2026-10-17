#include "Client/Transport/TcpClientTransport.h"
#include "Common/SimulationConfig.h"
#include "Common/Utility/CommandLine.h"
#include "TestSupport.h"

int main(int argc, char** argv) {
    return test::run([&] {
        CHECK(argc == 2);
        const auto [host, port] = sandbox::endpointOption(argv[1]);
        sandbox::TcpClientTransport client(host, port);
        CHECK(test::waitFor([&] {
            return client.connected();
        }));
        const auto id = client.playerId();
        CHECK(client.send({id, 1, 1, 0}));
        CHECK(test::waitFor([&] {
            if (auto state = client.receive()) {
                for (const auto& p : state->players) {
                    if (p.id == id && p.position.x > sandbox::simulation::spawnX(id) +
                                                         sandbox::simulation::playerRadius) {
                        return true;
                    }
                }
            }
            return false;
        }));
        client.close();
    });
}
