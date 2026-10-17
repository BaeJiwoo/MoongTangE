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
std::shared_ptr<ServerHost> startServerCore() {
    auto serverCore = std::make_shared<ServerHost>();
    serverCore->start();
    return serverCore;
}
} // namespace

ServerApplication::ServerApplication(ServerOptions options) : m_options(std::move(options)) {
    if (m_options.m_runDuration < std::chrono::seconds::zero()) {
        throw std::invalid_argument("Server duration cannot be negative");
    }
}

void ServerApplication::run() {
    if (m_hasRun.exchange(true)) {
        throw std::logic_error("ServerApplication can run only once");
    }
    if (m_stopRequested) {
        return;
    }

    // Declaration order closes networking before releasing the final server core owner.
    const auto serverCore = startServerCore();
    const std::string bindAddress = m_options.m_publicBind ? "0.0.0.0" : "127.0.0.1";
    AsioServer networkServer(serverCore, bindAddress, m_options.m_port);

    // Asio delivers signal callbacks on the control thread, outside the native signal handler.
    asio::io_context controlContext;
    asio::signal_set shutdownSignals(controlContext, SIGINT, SIGTERM);
    asio::steady_timer shutdownDeadline(controlContext);
    shutdownSignals.async_wait([this](asio::error_code error, int) {
        if (!error) {
            requestStop();
        }
    });
    if (m_options.m_runDuration > std::chrono::seconds::zero()) {
        shutdownDeadline.expires_after(m_options.m_runDuration);
        shutdownDeadline.async_wait([this](asio::error_code error) {
            if (!error) {
                requestStop();
            }
        });
    }

    std::cout << "Listening " << bindAddress << ':' << networkServer.port() << std::endl;
    if (m_options.m_publicBind) {
        std::cerr << "Warning: unauthenticated test server; use trusted networks only.\n";
    }
    m_boundPort = networkServer.port();
    constexpr auto STOP_POLL_INTERVAL = std::chrono::milliseconds(50);
    while (!m_stopRequested) {
        controlContext.run_for(STOP_POLL_INTERVAL);
    }
    // Control services release their callbacks before networking and the server core shut down.
}

void ServerApplication::requestStop() noexcept {
    m_stopRequested = true;
}

std::uint16_t ServerApplication::port() const noexcept {
    return m_boundPort.load();
}
} // namespace sandbox
