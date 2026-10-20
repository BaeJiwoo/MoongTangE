#include "Common/Utility/CommandLine.h"
#include "Server/ServerApplication.h"
#include "Server/ServerOptions.h"

#include <exception>
#include <iostream>

int main(int argc, char** argv)
{
    try {
        const auto arguments = MoongTangE::command_line_arguments(argc, argv);
        const auto options = MoongTangE::parse_server_options(arguments);
        if (options.m_showHelp) {
            std::cout << "DedicatedServer [--port 27015] [--public] [--run-for seconds]\n"
                         "  [--max-rooms 8] [--empty-room-seconds 30] [--world-seed uint64]\n"
                         "  [--matchmaking-url http://host:port --server-id ID]\n"
                         "  [--advertise-host host --require-ticket]\n"
                         "  [--reservation-seconds 30] Key: MOONGTANGE_CONTROL_KEY\n"
                         "  3 players per room; rooms are created on demand.\n";
            return 0;
        }
        MoongTangE::ServerApplication application(options);
        application.Run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
