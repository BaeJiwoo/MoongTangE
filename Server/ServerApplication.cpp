#include "Server/ServerApplication.h"
#include "Server/Core/ServerHost.h"
#include "Server/Network/AsioServer.h"
#include <asio.hpp>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace sandbox {
namespace {
std::shared_ptr<ServerHost> startCore() {
    auto core = std::make_shared<ServerHost>();
    core->start();
    return core;
}
} // namespace

ServerApplication::ServerApplication(ServerOptions options) : options_(std::move(options)) {
    if (options_.duration < std::chrono::seconds::zero()) {
        throw std::invalid_argument("Server duration cannot be negative");
    }
}

void ServerApplication::run() {
    if (started_.exchange(true)) {
        throw std::logic_error("ServerApplication can run only once");
    }
    if (stopRequested_) {
        return;
    }

    // Declaration order guarantees networking closes before the final core owner is released.
    const auto core = startCore();
    const std::string address = options_.publicBind ? "0.0.0.0" : "127.0.0.1";
    AsioServer network(core, address, options_.port);

    // Asio delivers signal callbacks on this control thread, outside the native signal handler.
    asio::io_context control;
    asio::signal_set signals(control, SIGINT, SIGTERM);
    asio::steady_timer deadline(control);
    signals.async_wait([this](asio::error_code error, int) {
        if (!error) {
            requestStop();
        }
    });
    if (options_.duration > std::chrono::seconds::zero()) {
        deadline.expires_after(options_.duration);
        deadline.async_wait([this](asio::error_code error) {
            if (!error) {
                requestStop();
            }
        });
    }

    std::cout << "Listening " << address << ':' << network.port() << std::endl;
    if (options_.publicBind) {
        std::cerr << "Warning: unauthenticated test server; use trusted networks only.\n";
    }
    port_ = network.port();
    constexpr auto stopPollInterval = std::chrono::milliseconds(50);
    while (!stopRequested_) {
        control.run_for(stopPollInterval);
    }
    // Destroying control services cancels their callbacks, then network and core shut down.
}

void ServerApplication::requestStop() noexcept {
    stopRequested_ = true;
}

std::uint16_t ServerApplication::port() const noexcept {
    return port_.load();
}
} // namespace sandbox
