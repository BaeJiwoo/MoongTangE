#include "Common/Utility/CommandLine.h"
#include "Server/Core/ServerHost.h"
#include "Server/Network/AsioServer.h"
#include "Server/ServerOptions.h"
#include <chrono>
#include <csignal>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace {
// Signal handlers must use sig_atomic_t rather than locks or ordinary shared objects.
volatile std::sig_atomic_t stopRequested = 0;

void requestStop(int) {
    stopRequested = 1;
}

void runServer(const sandbox::ServerOptions& options) {
    auto core = std::make_shared<sandbox::ServerHost>();
    core->start();
    const std::string address = options.publicBind ? "0.0.0.0" : "127.0.0.1";
    // Reverse destruction closes networking before the last ServerHost owner is released.
    sandbox::AsioServer server(core, address, options.port);
    std::cout << "Listening " << address << ':' << server.port() << std::endl;
    if (options.publicBind) {
        std::cerr << "Warning: unauthenticated test server; use trusted networks only.\n";
    }

    const auto started = std::chrono::steady_clock::now();
    while (stopRequested == 0) {
        const auto elapsed = std::chrono::steady_clock::now() - started;
        if (options.duration.count() != 0 && elapsed >= options.duration) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        const auto arguments = sandbox::commandLineArguments(argc, argv);
        const auto options = sandbox::parseServerOptions(arguments);
        if (options.showHelp) {
            std::cout << "DedicatedServer [--port 27015] [--public] [--run-for seconds]\n";
            return 0;
        }
        std::signal(SIGINT, requestStop);
        std::signal(SIGTERM, requestStop);
        runServer(options);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}