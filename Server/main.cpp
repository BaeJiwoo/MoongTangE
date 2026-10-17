#include "Common/Utility/CommandLine.h"
#include "Server/ServerApplication.h"
#include "Server/ServerOptions.h"
#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        const auto arguments = sandbox::commandLineArguments(argc, argv);
        const auto options = sandbox::parseServerOptions(arguments);
        if (options.showHelp) {
            std::cout << "DedicatedServer [--port 27015] [--public] [--run-for seconds]\n";
            return 0;
        }
        sandbox::ServerApplication application(options);
        application.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
