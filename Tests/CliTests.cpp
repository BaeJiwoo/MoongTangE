#include "Client/ClientOptions.h"
#include "Common/Utility/CommandLine.h"
#include "Server/ServerOptions.h"
#include "TestSupport.h"

#include <array>
#include <string_view>
#include <vector>

int main()
{
    return MoongTangE::test::run([] {
        MOONGTANGE_CHECK(MoongTangE::command_line_arguments(0, nullptr).empty());

        MOONGTANGE_CHECK(MoongTangE::unsigned_option("27015", 65535) == 27015);
        MOONGTANGE_CHECK(MoongTangE::unsigned_option("0", 65535, true) == 0);
        for (auto bad : {"", "-1", "65536", "10x", "0", "1.5"}) {
            MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
                MoongTangE::unsigned_option(bad, 65535);
            }));
        }

        MOONGTANGE_CHECK(MoongTangE::endpoint_option("localhost:27015").host == "localhost");
        MOONGTANGE_CHECK(MoongTangE::endpoint_option("[::1]:1234").host == "::1");
        for (auto bad : {"localhost", ":1234", "host:0", "[::1]:x", "::1:1234", "[]:1234",
                         "[host:1234", "host]:1234"}) {
            MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
                MoongTangE::endpoint_option(bad);
            }));
        }

        using namespace std::string_view_literals;
        MOONGTANGE_CHECK(!MoongTangE::parse_client_options({}).remote);
        const std::string ticket(64, 'a');
        const auto ticketClient = MoongTangE::parse_client_options(
            std::array{"--connect"sv, "localhost:27015"sv, "--ticket"sv, std::string_view(ticket)});
        MOONGTANGE_CHECK(ticketClient.ticket == ticket);
        MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
            MoongTangE::parse_client_options(std::array{"--ticket"sv, std::string_view(ticket)});
        }));
        const auto remote = MoongTangE::parse_client_options(
            std::array{"--connect"sv, "localhost:27015"sv, "--smoke"sv});
        MOONGTANGE_CHECK(remote.smoke && remote.remote->host == "localhost" &&
                         remote.remote->port == 27015);
        for (const auto& arguments :
             std::vector<std::vector<std::string_view>>{{"--connect"},
                                                        {"--unknown"},
                                                        {"--ticket", "invalid"},
                                                        {"--single", "--connect", "host:1"},
                                                        {"--connect", ""}}) {
            MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
                MoongTangE::parse_client_options(arguments);
            }));
        }

        const auto server =
            MoongTangE::parse_server_options(std::array{"--port"sv, "0"sv, "--run-for"sv, "2"sv});
        MOONGTANGE_CHECK(server.m_port == 0 && server.m_runDuration.count() == 2 &&
                         !server.m_publicBind);
        MOONGTANGE_CHECK(server.m_rooms.m_maxRooms == 8 &&
                         server.m_rooms.m_emptyRoomLifetime.count() == 30);
        MOONGTANGE_CHECK(!server.m_apiPort && !server.m_apiPublic && !server.m_requireTicket);
        const auto api = MoongTangE::parse_server_options(
            std::array{"--api-port"sv, "0"sv, "--api-public"sv, "--require-ticket"sv,
                       "--reservation-seconds"sv, "1"sv});
        MOONGTANGE_CHECK(api.m_apiPort == 0 && api.m_apiPublic && api.m_requireTicket &&
                         api.m_reservationLifetime.count() == 1 && !api.m_publicBind);
        const auto rooms = MoongTangE::parse_server_options(
            std::array{"--max-rooms"sv, "2"sv, "--empty-room-seconds"sv, "0"sv});
        MOONGTANGE_CHECK(rooms.m_rooms.m_maxRooms == 2 &&
                         rooms.m_rooms.m_emptyRoomLifetime.count() == 0);
        MOONGTANGE_CHECK(MoongTangE::parse_server_options(std::array{"--public"sv}).m_publicBind);
        MOONGTANGE_CHECK(MoongTangE::parse_client_options(std::array{"--help"sv}).showHelp);
        MOONGTANGE_CHECK(MoongTangE::parse_server_options(std::array{"--help"sv}).m_showHelp);
        for (const auto& arguments :
             std::vector<std::vector<std::string_view>>{{"--port"},
                                                        {"--api-port"},
                                                        {"--api-port", "65536"},
                                                        {"--api-public"},
                                                        {"--require-ticket"},
                                                        {"--reservation-seconds", "0"},
                                                        {"--reservation-seconds", "301"},
                                                        {"--max-rooms"},
                                                        {"--max-rooms", "0"},
                                                        {"--max-rooms", "257"},
                                                        {"--empty-room-seconds"},
                                                        {"--empty-room-seconds", "-1"},
                                                        {"--empty-room-seconds", "86401"},
                                                        {"--run-for"},
                                                        {"--run-for", "0"},
                                                        {"--run-for", "86401"},
                                                        {"--unknown"}}) {
            MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
                MoongTangE::parse_server_options(arguments);
            }));
        }
    });
}
