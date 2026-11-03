#include "Client/Matchmaking/MultiplayerConnection.h"
#include "TestSupport.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

int main(int argc, char** argv)
{
    return MoongTangE::test::run([&] {
        MOONGTANGE_CHECK(argc == 3 || argc == 4);
        auto api =
            std::make_shared<MoongTangE::MatchmakingClient>(argv[1], argc == 4 ? argv[3] : "");
        const std::string mode = argv[2];
        const int attemptCount = mode == "connect" ? 2 : mode == "fail_many" ? 65 : 1;
        for (int attempt = 0; attempt < attemptCount; ++attempt) {
            MoongTangE::MultiplayerConnection connection(api, "GuestProbe");
            if (mode == "cancel") {
                MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
                    return connection.Stage() == MoongTangE::ConnectionStage::MATCHING;
                }));
                connection.Cancel();
                MOONGTANGE_CHECK(connection.Stage() == MoongTangE::ConnectionStage::CANCELLED);
            }
            MOONGTANGE_CHECK(MoongTangE::test::wait_for(
                [&] {
                    return connection.Finished();
                },
                std::chrono::seconds(22)));
            if (mode == "cancel") {
                MOONGTANGE_CHECK(!connection.TakeTransport());
                MOONGTANGE_CHECK(connection.Error().empty());
                return;
            }
            if (mode.starts_with("fail")) {
                MOONGTANGE_CHECK(connection.Stage() == MoongTangE::ConnectionStage::FAILED);
                MOONGTANGE_CHECK(!connection.Error().empty());
                std::cout << connection.Error() << '\n';
                MOONGTANGE_CHECK(!connection.TakeTransport());
                continue;
            }
            if (!connection.Error().empty()) {
                throw std::runtime_error(connection.Error());
            }
            auto transport = connection.TakeTransport();
            MOONGTANGE_CHECK(transport && transport->Connected());
            const auto playerId = transport->PlayerId();
            std::optional<double> initialX;
            std::uint64_t sequence = 0;
            MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
                MOONGTANGE_CHECK(transport->Send({playerId, ++sequence, 1, 0}));
                if (auto snapshot = transport->Receive()) {
                    for (const auto& player : snapshot->players) {
                        if (player.id != playerId) {
                            continue;
                        }
                        if (!initialX) {
                            initialX = player.position.x;
                        }
                        if (player.position.x > *initialX + 0.25) {
                            return true;
                        }
                    }
                }
                return false;
            }));
            transport->Close();
            std::this_thread::sleep_for(std::chrono::milliseconds(1200));
        }
    });
}
