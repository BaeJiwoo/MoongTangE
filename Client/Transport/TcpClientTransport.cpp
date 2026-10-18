#include "Client/Transport/TcpClientTransport.h"

#include "Common/Protocol/AdmissionTicket.h"
#include "Network/Protocol/Codec.h"
#include "Network/TcpChannel.h"

#include <asio.hpp>
#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace MoongTangE {

class TcpClientTransport::ClientNetworkRuntime
{
  public:
    ClientNetworkRuntime(const std::string& host, std::uint16_t port, std::string ticket)
        : m_ticket(std::move(ticket))
    {
        if (!m_ticket.empty() && !valid_admission_ticket(m_ticket)) {
            throw std::invalid_argument("Invalid admission ticket");
        }
        BeginConnection(host, port);

        m_networkThread = std::thread([this] {
            m_ioContext.run();
        });
    }

    ~ClientNetworkRuntime()
    {
        Stop();
    }

    ClientNetworkRuntime(const ClientNetworkRuntime&) = delete;
    ClientNetworkRuntime& operator=(const ClientNetworkRuntime&) = delete;
    ClientNetworkRuntime(ClientNetworkRuntime&&) = delete;
    ClientNetworkRuntime& operator=(ClientNetworkRuntime&&) = delete;

    bool Connected() const
    {
        std::lock_guard lock(m_stateMutex);
        return m_playerId != 0 && !m_stopped;
    }

    std::uint32_t PlayerId() const
    {
        std::lock_guard lock(m_stateMutex);
        return m_playerId;
    }

    std::string Error() const
    {
        std::lock_guard lock(m_stateMutex);
        return m_errorMessage;
    }

    std::uint64_t RoomId() const
    {
        std::lock_guard lock(m_stateMutex);
        return m_roomId;
    }

    std::optional<WorldSnapshot> Receive()
    {
        std::lock_guard lock(m_stateMutex);
        return std::exchange(m_latestSnapshot, std::nullopt);
    }

    bool Send(InputCommand input)
    {
        if (!input.Valid()) {
            return false;
        }

        auto data = wire::input(input);

        std::lock_guard lock(m_stateMutex);
        if (m_stopped || m_playerId == 0 || input.playerId != m_playerId ||
            m_queuedInputs >= MAX_QUEUED_INPUTS) {
            return false;
        }

        ++m_queuedInputs;
        try {
            asio::post(m_ioContext, [this, data = std::move(data)] {
                {
                    std::lock_guard queueLock(m_stateMutex);
                    --m_queuedInputs;
                }

                if (m_channel) {
                    m_channel->Send(data);
                }
            });
        } catch (...) {
            --m_queuedInputs; // A failed post must not consume a queue slot.
            throw;
        }
        return true;
    }

    // Called by the one consumer thread; all callbacks are drained before destruction.
    void Stop()
    {
        {
            std::lock_guard lock(m_stateMutex);
            if (m_stopped) {
                return;
            }

            m_stopped = true;
            m_playerId = 0;
            m_roomId = 0;
            m_latestSnapshot.reset();
        }

        asio::post(m_ioContext, [this] {
            Fail("Client closed");
            m_workGuard.reset();
        });
        if (m_networkThread.joinable()) {
            m_networkThread.join();
        }
    }

  private:
    static constexpr std::size_t MAX_QUEUED_INPUTS = 64;
    static constexpr auto HANDSHAKE_TIMEOUT = std::chrono::seconds(5);

    void BeginConnection(const std::string& host, std::uint16_t port)
    {
        m_handshakeTimer.expires_after(HANDSHAKE_TIMEOUT);
        m_handshakeTimer.async_wait([this](asio::error_code error) {
            if (!error) {
                Fail("Connect/handshake timeout");
            }
        });

        m_resolver.async_resolve(
            host, std::to_string(port),
            [this](asio::error_code error, asio::ip::tcp::resolver::results_type endpoints) {
                if (m_terminal) {
                    return;
                }
                if (error) {
                    Fail(error.message());
                    return;
                }

                ConnectSocket(endpoints);
            });
    }

    void ConnectSocket(const asio::ip::tcp::resolver::results_type& endpoints)
    {
        asio::async_connect(m_socket, endpoints,
                            [this](asio::error_code error, const asio::ip::tcp::endpoint&) {
                                if (m_terminal) {
                                    return;
                                }
                                if (error) {
                                    Fail(error.message());
                                    return;
                                }

                                StartHandshake();
                            });
    }

    void StartHandshake()
    {
        m_channel = std::make_shared<TcpChannel>(std::move(m_socket));
        m_channel->Start(
            [this](const wire::Bytes& data) {
                ReceiveMessage(data);
            },
            [this](std::string reason) {
                m_terminal = true;
                RecordFailure(std::move(reason));
            });

        m_channel->Send(wire::client_hello(m_ticket));
        m_ticket.clear();
    }

    void ReceiveMessage(const wire::Bytes& data)
    {
        std::lock_guard lock(m_stateMutex);
        if (m_stopped) {
            return;
        }
        if (m_playerId == 0) {
            const auto admission = wire::read_server_hello(data);
            if (admission.status != AdmissionStatus::ACCEPTED) {
                throw std::runtime_error(admission.status == AdmissionStatus::FULL
                                             ? "All game rooms are full"
                                             : "Game server unavailable");
            }
            m_playerId = admission.playerId;
            m_roomId = admission.roomId;
            m_lastReceivedTick = 0;
            m_latestSnapshot.reset();
            m_handshakeTimer.cancel();
            return;
        }

        auto room = wire::read_snapshot(data);
        wire::require(room.roomId == m_roomId);
        auto& state = room.world;
        if (state.tick > m_lastReceivedTick) {
            m_lastReceivedTick = state.tick;
            m_latestSnapshot = std::move(state);
        }
    }

    void RecordFailure(std::string reason)
    {
        std::lock_guard lock(m_stateMutex);
        if (m_errorMessage.empty()) {
            m_errorMessage = std::move(reason);
        }
        m_playerId = 0;
        m_roomId = 0;
        m_latestSnapshot.reset();
    }

    void Fail(std::string reason)
    {
        m_terminal = true;
        RecordFailure(std::move(reason));

        m_resolver.cancel();
        m_handshakeTimer.cancel();

        asio::error_code ignored;
        m_socket.close(ignored);

        if (m_channel) {
            m_channel->Close("Connection failed");
        }
    }

    asio::io_context m_ioContext;
    asio::executor_work_guard<asio::io_context::executor_type> m_workGuard{
        m_ioContext.get_executor()};

    asio::ip::tcp::resolver m_resolver{m_ioContext};
    asio::ip::tcp::socket m_socket{m_ioContext};
    asio::steady_timer m_handshakeTimer{m_ioContext};
    std::shared_ptr<TcpChannel> m_channel;

    // The consumer and io thread share only these fields, always under m_stateMutex.
    mutable std::mutex m_stateMutex;
    std::uint32_t m_playerId{};
    std::uint64_t m_roomId{};
    std::size_t m_queuedInputs{};
    std::optional<WorldSnapshot> m_latestSnapshot;
    std::string m_errorMessage;
    bool m_stopped{};

    // The io thread alone owns the connection lifecycle and snapshot ordering.
    std::uint64_t m_lastReceivedTick{};
    bool m_terminal{};
    std::string m_ticket;
    std::thread m_networkThread;
};

TcpClientTransport::TcpClientTransport(const std::string& host, std::uint16_t port,
                                       std::string ticket)
    : m_networkRuntime(std::make_unique<ClientNetworkRuntime>(host, port, std::move(ticket)))
{
}

TcpClientTransport::~TcpClientTransport() = default;

bool TcpClientTransport::Connected() const
{
    return m_networkRuntime->Connected();
}

std::uint32_t TcpClientTransport::PlayerId() const
{
    return m_networkRuntime->PlayerId();
}

bool TcpClientTransport::Send(InputCommand input)
{
    return m_networkRuntime->Send(input);
}

std::uint64_t TcpClientTransport::RoomId() const
{
    return m_networkRuntime->RoomId();
}

std::optional<WorldSnapshot> TcpClientTransport::Receive()
{
    return m_networkRuntime->Receive();
}

std::string TcpClientTransport::Error() const
{
    return m_networkRuntime->Error();
}

void TcpClientTransport::Close()
{
    m_networkRuntime->Stop();
}

} // namespace MoongTangE
