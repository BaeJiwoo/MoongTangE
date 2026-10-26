#include "Server/ServerApplication.h"

#include "Server/Api/MatchmakingControlClient.h"
#include "Server/Core/ServerHost.h"
#include "Server/Network/NetworkServer.h"

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace MoongTangE {
namespace {

volatile std::sig_atomic_t shutdownRequested = 0;

void request_shutdown(int)
{
    shutdownRequested = 1;
}

class SignalRegistration
{
  public:
    SignalRegistration()
    {
        shutdownRequested = 0;
        m_previousInterrupt = std::signal(SIGINT, request_shutdown);
        m_previousTerminate = std::signal(SIGTERM, request_shutdown);
    }

    ~SignalRegistration()
    {
        std::signal(SIGINT, m_previousInterrupt);
        std::signal(SIGTERM, m_previousTerminate);
    }

    SignalRegistration(const SignalRegistration&) = delete;
    SignalRegistration& operator=(const SignalRegistration&) = delete;
    SignalRegistration(SignalRegistration&&) = delete;
    SignalRegistration& operator=(SignalRegistration&&) = delete;

  private:
    using Handler = void (*)(int);

    Handler m_previousInterrupt{};
    Handler m_previousTerminate{};
};

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
    if (m_options.m_requireTicket && !m_options.m_matchmakingUrl) {
        throw std::invalid_argument("Ticket mode requires an external matchmaking service");
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
    NetworkServer networkServer(serverCore, bindAddress, m_options.m_port);
    std::unique_ptr<MatchmakingControlClient> matchmakingControl;
    if (m_options.m_matchmakingUrl) {
        const auto controlKey = std::getenv("MOONGTANGE_CONTROL_KEY");
        if (!controlKey) {
            throw std::invalid_argument("Set MOONGTANGE_CONTROL_KEY before connecting matchmaking");
        }
        matchmakingControl = std::make_unique<MatchmakingControlClient>(
            serverCore, MatchmakingControlOptions{*m_options.m_matchmakingUrl, m_options.m_serverId,
                                                  m_options.m_advertisedHost, networkServer.Port(),
                                                  m_options.m_reservationLifetime, controlKey});
    }

    SignalRegistration shutdownSignals;
    const auto shutdownDeadline = m_options.m_runDuration > std::chrono::seconds::zero()
                                      ? std::chrono::steady_clock::now() + m_options.m_runDuration
                                      : std::chrono::steady_clock::time_point::max();

    std::cout << "Listening " << bindAddress << ':' << networkServer.Port() << std::endl;
    if (matchmakingControl) {
        std::cout << "Matchmaking control " << *m_options.m_matchmakingUrl << " as "
                  << m_options.m_serverId << std::endl;
    }
    if (m_options.m_publicBind) {
        std::cerr << "Warning: unauthenticated test server; use trusted networks only.\n";
    }

    m_boundPort = networkServer.Port();

    constexpr auto STOP_POLL_INTERVAL = std::chrono::milliseconds(50);
    while (!m_stopRequested) {
        if (shutdownRequested || std::chrono::steady_clock::now() >= shutdownDeadline) {
            RequestStop();
            break;
        }
        std::this_thread::sleep_for(STOP_POLL_INTERVAL);
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
