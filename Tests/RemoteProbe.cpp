#include "Client/Transport/TcpClientTransport.h"
#include "Common/SimulationConfig.h"
#include "Common/Utility/CommandLine.h"
#include "TestSupport.h"

#include <optional>

int main(int argc, char** argv)
{
    return MoongTangE::test::run([&] {
        MOONGTANGE_CHECK(argc == 2);
        const auto [host, port] = MoongTangE::endpoint_option(argv[1]);
        MoongTangE::TcpClientTransport client(host, port);
        MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
            return client.Connected();
        }));

        const auto id = client.PlayerId();
        std::optional<double> initialX;
        MOONGTANGE_CHECK(client.Send({id, 1, 1, 0}));
        MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
            if (auto state = client.Receive()) {
                for (const auto& p : state->players) {
                    if (p.id == id) {
                        if (!initialX) {
                            initialX = p.position.x;
                        }
                        if (p.position.x > *initialX + MoongTangE::simulation::PLAYER_RADIUS) {
                            return true;
                        }
                    }
                }
            }
            return false;
        }));

        client.Close();
    });
}
