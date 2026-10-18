#include "Server/ServerApplication.h"

#include "Server/Api/MatchmakingApi.h"
#include "Server/Core/ServerHost.h"
#include "Server/Network/AsioServer.h"

#include <asio.hpp>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace MoongTangE {
namespace {

std::shared_ptr<ServerHost> start_server_core(const ServerOptions& options)
{
    auto serverCore = std::make_shared<ServerHost>(options.m_rooms, options.m_requireTicket,
                                                   options.m_reservationLifetime);
    serverCore->Start();
    return serverCore;
}

} // namespace

ServerApplication::ServerApplication(ServerOptions options) : m_options(std::move(options))
{
    m_options.m_rooms.Validate();
    if ((m_options.m_apiPublic || m_options.m_requireTicket) && !m_options.m_apiPort) {
        throw std::invalid_argument("Ticket/public API mode requires an API port");
    }
    if (m_options.m_runDuration < std::chrono::seconds::zero()) {
        throw std::invalid_argument("Server duration cannot be negative");
    }
}

void ServerApplication::Run()
{
    if (m_hasRun.exchange(true)) {
        throw std::logic_error("ServerApplication can run only once");
    }
    if (m_stopRequested) {
        return;
    }

    // Declaration order closes networking before releasing the final server core owner.
    const auto serverCore = start_server_core(m_options);
    const std::string bindAddress = m_options.m_publicBind ? "0.0.0.0" : "127.0.0.1";
    AsioServer networkServer(serverCore, bindAddress, m_options.m_port);
    std::unique_ptr<MatchmakingApi> matchmakingApi;
    if (m_options.m_apiPort) {
        const auto apiKey = std::getenv("MOONGTANGE_API_KEY");
        if (!apiKey) {
            throw std::invalid_argument("Set MOONGTANGE_API_KEY before enabling the API");
        }
        matchmakingApi = std::make_unique<MatchmakingApi>(
            serverCore, m_options.m_apiPublic ? "0.0.0.0" : "127.0.0.1", *m_options.m_apiPort,
            apiKey, networkServer.Port(), m_options.m_requireTicket,
            static_cast<unsigned>(m_options.m_reservationLifetime.count()));
    }

    // Asio delivers signal callbacks on the control thread, outside the native signal handler.
    asio::io_context controlContext;
    asio::signal_set shutdownSignals(controlContext, SIGINT, SIGTERM);
    asio::steady_timer shutdownDeadline(controlContext);

    shutdownSignals.async_wait([this](asio::error_code error, int) {
        if (!error) {
            RequestStop();
        }
    });

    if (m_options.m_runDuration > std::chrono::seconds::zero()) {
        shutdownDeadline.expires_after(m_options.m_runDuration);
        shutdownDeadline.async_wait([this](asio::error_code error) {
            if (!error) {
                RequestStop();
            }
        });
    }

    std::cout << "Listening " << bindAddress << ':' << networkServer.Port() << std::endl;
    if (matchmakingApi) {
        std::cout << "Matchmaking API " << (m_options.m_apiPublic ? "0.0.0.0" : "127.0.0.1") << ':'
                  << matchmakingApi->Port() << std::endl;
    }
    if (m_options.m_publicBind) {
        std::cerr << "Warning: unauthenticated test server; use trusted networks only.\n";
    }

    m_boundPort = networkServer.Port();

    constexpr auto STOP_POLL_INTERVAL = std::chrono::milliseconds(50);
    while (!m_stopRequested) {
        controlContext.run_for(STOP_POLL_INTERVAL);
    }

    // Control services release their callbacks before networking and the server core shut down.
}

void ServerApplication::RequestStop() noexcept
{
    m_stopRequested = true;
}

std::uint16_t ServerApplication::Port() const noexcept
{
    return m_boundPort.load();
}

} // namespace MoongTangE
