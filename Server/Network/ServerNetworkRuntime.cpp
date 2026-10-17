#include "Server/Network/ServerNetworkRuntime.h"

#include "Network/Protocol/Codec.h"
#include "Network/TcpChannel.h"

#include <stdexcept>
#include <utility>

namespace MoongTangE {

ServerNetworkRuntime::ServerNetworkRuntime(std::shared_ptr<ServerHost> host,
                                           const std::string& address, std::uint16_t port)
    : m_serverHost(std::move(host)), m_acceptor(m_ioContext), m_snapshotTimer(m_ioContext)
{
    if (!m_serverHost || !m_serverHost->Running()) {
        throw std::invalid_argument("ServerCore is not running");
    }

    const asio::ip::tcp::endpoint endpoint(asio::ip::make_address(address), port);
    m_acceptor.open(endpoint.protocol());
    m_acceptor.bind(endpoint);
    m_acceptor.listen();
    m_boundPort = m_acceptor.local_endpoint().port();

    AcceptNextConnection();
    ScheduleSnapshotBroadcast();

    m_networkThread = std::thread([this] {
        m_ioContext.run();
    });
}

ServerNetworkRuntime::~ServerNetworkRuntime()
{
    asio::post(m_ioContext, [this] {
        CloseAllSessions();
    });
    if (m_networkThread.joinable()) {
        m_networkThread.join();
    }
}

std::uint16_t ServerNetworkRuntime::Port() const
{
    return m_boundPort;
}

void ServerNetworkRuntime::CloseAllSessions()
{
    m_stopping = true;
    asio::error_code ignored;
    m_acceptor.close(ignored);
    m_snapshotTimer.cancel();

    // Closing invokes removal callbacks; iterate a snapshot, not the modified map.
    const auto sessions = m_sessions;
    for (const auto& [channel, playerId] : sessions) {
        (void)playerId;
        channel->Close("Server stopping");
    }
}

void ServerNetworkRuntime::AcceptNextConnection()
{
    m_acceptor.async_accept([this](asio::error_code error, asio::ip::tcp::socket socket) {
        if (m_stopping) {
            return;
        }
        if (!error && m_sessions.size() < MAX_CONNECTIONS) {
            AddSession(std::move(socket));
        }

        if (m_acceptor.is_open()) {
            AcceptNextConnection();
        }
    });
}

void ServerNetworkRuntime::AddSession(asio::ip::tcp::socket socket)
{
    auto channel = std::make_shared<TcpChannel>(std::move(socket));
    m_sessions.emplace(channel, 0);

    // Weak callbacks avoid a channel -> callback -> channel ownership cycle.
    const std::weak_ptr<TcpChannel> weakChannel = channel;
    channel->Start(
        [this, weakChannel](const wire::Bytes& data) {
            if (auto connection = weakChannel.lock()) {
                HandleMessage(connection, data);
            }
        },
        [this, weakChannel](std::string) {
            if (auto connection = weakChannel.lock()) {
                RemoveSession(connection);
            }
        });
}

void ServerNetworkRuntime::RemoveSession(const std::shared_ptr<TcpChannel>& channel)
{
    const auto session = m_sessions.find(channel);
    if (session != m_sessions.end()) {
        m_serverHost->Disconnect(session->second);
        m_sessions.erase(session);
    }
}

void ServerNetworkRuntime::HandleMessage(const std::shared_ptr<TcpChannel>& channel,
                                         const wire::Bytes& data)
{
    auto& playerId = m_sessions.at(channel);
    if (playerId == 0) {
        wire::read_client_hello(data);
        playerId = m_serverHost->Connect();
        wire::require(playerId != 0);
        channel->Send(wire::server_hello(playerId));
    } else if (wire::type(data) == wire::MessageType::INPUT) {
        wire::require(m_serverHost->Submit(wire::read_input(data, playerId)));
    } else if (wire::is_disconnect(data)) {
        channel->Close("Client disconnected");
    } else {
        channel->Close("Unexpected message");
    }
}

void ServerNetworkRuntime::BroadcastLatestSnapshot()
{
    const auto state = m_serverHost->Snapshot();
    if (state.tick == 0 || state.tick == m_lastPublishedTick) {
        return;
    }

    m_lastPublishedTick = state.tick;
    const auto bytes = wire::snapshot(state);

    // Send() can disconnect a slow peer and remove it from m_sessions.
    const auto sessions = m_sessions;
    for (const auto& [channel, playerId] : sessions) {
        if (playerId != 0) {
            channel->Send(bytes);
        }
    }
}

void ServerNetworkRuntime::ScheduleSnapshotBroadcast()
{
    m_snapshotTimer.expires_after(SNAPSHOT_INTERVAL);
    m_snapshotTimer.async_wait([this](asio::error_code error) {
        if (error || m_stopping) {
            return;
        }

        BroadcastLatestSnapshot();
        ScheduleSnapshotBroadcast();
    });
}

} // namespace MoongTangE
