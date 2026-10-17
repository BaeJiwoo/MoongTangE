#include "Client/ClientOptions.h"
#include "Common/Utility/CommandLine.h"
#include "Server/ServerOptions.h"
#include "TestSupport.h"
#include <array>
#include <string_view>
#include <vector>

int main() {
    return test::run([] {
        CHECK(sandbox::commandLineArguments(0, nullptr).empty());
        CHECK(sandbox::unsignedOption("27015", 65535) == 27015);
        CHECK(sandbox::unsignedOption("0", 65535, true) == 0);
        for (auto bad : {"", "-1", "65536", "10x", "0", "1.5"}) {
            CHECK(test::throws([&] {
                sandbox::unsignedOption(bad, 65535);
            }));
        }
        CHECK(sandbox::endpointOption("localhost:27015").host == "localhost");
        CHECK(sandbox::endpointOption("[::1]:1234").host == "::1");
        for (auto bad : {"localhost", ":1234", "host:0", "[::1]:x", "::1:1234", "[]:1234",
                         "[host:1234", "host]:1234"}) {
            CHECK(test::throws([&] {
                sandbox::endpointOption(bad);
            }));
        }
        using namespace std::string_view_literals;
        CHECK(!sandbox::parseClientOptions({}).remote);
        const auto remote = sandbox::parseClientOptions(
            std::array{"--connect"sv, "localhost:27015"sv, "--smoke"sv});
        CHECK(remote.smoke && remote.remote->host == "localhost" && remote.remote->port == 27015);
        for (const auto& arguments :
             std::vector<std::vector<std::string_view>>{{"--connect"},
                                                        {"--unknown"},
                                                        {"--single", "--connect", "host:1"},
                                                        {"--connect", ""}}) {
            CHECK(test::throws([&] {
                sandbox::parseClientOptions(arguments);
            }));
        }
        const auto server =
            sandbox::parseServerOptions(std::array{"--port"sv, "0"sv, "--run-for"sv, "2"sv});
        CHECK(server.m_port == 0 && server.m_runDuration.count() == 2 && !server.m_publicBind);
        CHECK(sandbox::parseServerOptions(std::array{"--public"sv}).m_publicBind);
        CHECK(sandbox::parseClientOptions(std::array{"--help"sv}).showHelp);
        CHECK(sandbox::parseServerOptions(std::array{"--help"sv}).m_showHelp);
        for (const auto& arguments :
             std::vector<std::vector<std::string_view>>{{"--port"},
                                                        {"--run-for"},
                                                        {"--run-for", "0"},
                                                        {"--run-for", "86401"},
                                                        {"--unknown"}}) {
            CHECK(test::throws([&] {
                sandbox::parseServerOptions(arguments);
            }));
        }
    });
}
