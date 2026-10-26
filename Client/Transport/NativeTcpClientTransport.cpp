#include "Client/Transport/TcpClientTransport.h"
#include "Common/Protocol/AdmissionTicket.h"
#include "Network/Native/NativeEventLoop.h"
#include "Network/Native/NativeTcpChannel.h"
#include "Network/Protocol/Codec.h"

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
    ClientNetworkRuntime(const std::string& host, std::uint16_t port, std::string ticket,
                         std::string nickname)
        : m_ticket(std::move(ticket)), m_nickname(std::move(nickname))
    {
        if (!m_ticket.empty() && !valid_admission_ticket(m_ticket)) {
            throw std::invalid_argument("Invalid admission ticket");
        }

        BeginConnection(host, port);
        m_networkThread = std::thread([this] {
            m_eventLoop.Run();
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

    std::string Error() const
    {
        std::lock_guard lock(m_stateMutex);
        return m_errorMessage;
    }

    bool Send(InputCommand input)
    {
        if (!input.Valid()) {
            return false;
        }

        auto data = wire::input(input);
        {
            std::lock_guard lock(m_stateMutex);
            if (m_stopped || m_playerId == 0 || input.playerId != m_playerId ||
                m_queuedInputs >= MAX_QUEUED_INPUTS) {
                return false;
            }
            ++m_queuedInputs;
        }

        m_eventLoop.Post([this, data = std::move(data)] {
            {
                std::lock_guard lock(m_stateMutex);
                --m_queuedInputs;
            }

            if (m_channel) {
                m_channel->Send(data);
            }
        });
        return true;
    }

    void Stop()
    {
        bool sendDisconnect = false;
        {
            std::lock_guard lock(m_stateMutex);
            if (m_stopped) {
                return;
            }

            sendDisconnect = m_playerId != 0;
            m_stopped = true;
            m_playerId = 0;
            m_roomId = 0;
            m_latestSnapshot.reset();
        }

        m_eventLoop.Post([this, sendDisconnect] {
            m_terminal = true;
            if (sendDisconnect && m_channel) {
                m_channel->SendAndClose(wire::disconnect(), "Client closed");
            } else {
                CloseConnection("Client closed");
            }
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
        m_handshakeTimer = m_eventLoop.Schedule(HANDSHAKE_TIMEOUT, [this] {
            Fail("Connect/handshake timeout");
        });
        m_eventLoop.Connect(host, port,
                            [this](NativeEventLoop::ConnectionId connection, std::string error) {
                                if (m_terminal) {
                                    if (connection != 0) {
                                        m_eventLoop.Close(connection);
                                    }
                                    return;
                                }
                                if (!error.empty()) {
                                    Fail(std::move(error));
                                    return;
                                }

                                StartHandshake(connection);
                            });
    }

    void StartHandshake(NativeEventLoop::ConnectionId connection)
    {
        m_connection = connection;
        m_channel = std::make_shared<NativeTcpChannel>(m_eventLoop, connection);
        m_channel->Start(
            [this](const wire::Bytes& data) {
                ReceiveMessage(data);
            },
            [this](std::string reason) {
                RecordFailure(std::move(reason));
                m_eventLoop.Stop();
            });

        m_channel->Send(wire::client_hello(m_ticket, m_nickname));
        m_ticket.clear();
        m_nickname.clear();
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
                if (admission.status == AdmissionStatus::FULL) {
                    throw std::runtime_error("All game rooms are full");
                }
                if (admission.status == AdmissionStatus::INVALID_NICKNAME) {
                    throw std::runtime_error("Enter a valid nickname before joining multiplayer");
                }
                throw std::runtime_error("Game server unavailable");
            }

            m_playerId = admission.playerId;
            m_roomId = admission.roomId;
            m_lastReceivedTick = 0;
            m_latestSnapshot.reset();
            m_eventLoop.Cancel(m_handshakeTimer);
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
        if (!m_stopped && m_errorMessage.empty()) {
            m_errorMessage = std::move(reason);
        }
        m_playerId = 0;
        m_roomId = 0;
        m_latestSnapshot.reset();
    }

    void CloseConnection(std::string reason)
    {
        m_eventLoop.Cancel(m_handshakeTimer);
        if (m_channel) {
            m_channel->Close(std::move(reason));
        } else if (m_connection != 0) {
            m_eventLoop.Close(m_connection);
            m_eventLoop.Stop();
        } else {
            m_eventLoop.Stop();
        }
    }

    void Fail(std::string reason)
    {
        if (m_terminal) {
            return;
        }

        m_terminal = true;
        RecordFailure(std::move(reason));
        CloseConnection("Connection failed");
    }

    NativeEventLoop m_eventLoop;
    NativeEventLoop::ConnectionId m_connection{};
    NativeEventLoop::TimerId m_handshakeTimer{};
    std::shared_ptr<NativeTcpChannel> m_channel;

    mutable std::mutex m_stateMutex;
    std::uint32_t m_playerId{};
    std::uint64_t m_roomId{};
    std::size_t m_queuedInputs{};
    std::optional<WorldSnapshot> m_latestSnapshot;
    std::string m_errorMessage;
    bool m_stopped{};

    std::uint64_t m_lastReceivedTick{};
    bool m_terminal{};
    std::string m_ticket;
    std::string m_nickname;
    std::thread m_networkThread;
};

TcpClientTransport::TcpClientTransport(const std::string& host, std::uint16_t port,
                                       std::string ticket, std::string nickname)
    : m_networkRuntime(std::make_unique<ClientNetworkRuntime>(host, port, std::move(ticket),
                                                              std::move(nickname)))
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

std::uint64_t TcpClientTransport::RoomId() const
{
    return m_networkRuntime->RoomId();
}

bool TcpClientTransport::Send(InputCommand input)
{
    return m_networkRuntime->Send(input);
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
