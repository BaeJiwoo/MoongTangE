#include "Client/ClientApplication.h"
#include "Client/ClientOptions.h"
#include "Common/Utility/CommandLine.h"
#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        const auto arguments = MoongTangE::commandLineArguments(argc, argv);
        const auto options = MoongTangE::parseClientOptions(arguments);
        if (options.showHelp) {
            std::cout << "Client [--single | --connect host:port] [--smoke]\n";
            return 0;
        }
        MoongTangE::ClientApplication application(options);
        application.run();
        std::cout << "Client stopped successfully\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}