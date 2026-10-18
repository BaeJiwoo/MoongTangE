#include "Client/ClientApplication.h"
#include "Client/ClientOptions.h"
#include "Common/Utility/CommandLine.h"

#include <exception>
#include <iostream>

int main(int argc, char** argv)
{
    try {
        const auto arguments = MoongTangE::command_line_arguments(argc, argv);
        const auto options = MoongTangE::parse_client_options(arguments);
        if (options.showHelp) {
            std::cout << "Client [--single | --connect host:port] [--smoke]\n";
            return 0;
        }
        MoongTangE::ClientApplication application(options);
        application.Run();
        std::cout << "Client stopped successfully\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}