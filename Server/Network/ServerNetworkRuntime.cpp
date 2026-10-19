#include "Server/Network/ServerNetworkRuntime.h"

#include "Common/Protocol/Nickname.h"
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
    for (const auto& [channel, session] : sessions) {
        (void)session;
        channel->Close("Server stopping");
    }
}

void ServerNetworkRuntime::AcceptNextConnection()
{
    m_acceptor.async_accept([this](asio::error_code error, asio::ip::tcp::socket socket) {
        if (m_stopping) {
            return;
        }
        if (!error && m_sessions.size() < m_serverHost->Capacity() * 2) {
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
    m_sessions.emplace(channel, NetworkSession{});

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
        m_serverHost->Disconnect(session->second.m_playerId);
        m_sessions.erase(session);
    }
}

void ServerNetworkRuntime::HandleMessage(const std::shared_ptr<TcpChannel>& channel,
                                         const wire::Bytes& data)
{
    auto& session = m_sessions.at(channel);
    if (!session.m_helloReceived) {
        const auto hello = wire::read_client_hello(data);
        session.m_helloReceived = true;
        if (!valid_nickname(hello.nickname)) {
            session.m_rejected = true;
            channel->SendAndClose(wire::server_hello({AdmissionStatus::INVALID_NICKNAME, 0, 0}),
                                  "Invalid nickname");
            return;
        }
        session.m_playerId = m_serverHost->Connect(hello.ticket, hello.nickname);
        CompleteAdmission(channel);
    } else if (wire::is_disconnect(data)) {
        channel->Close("Client disconnected");
    } else if (wire::type(data) == wire::MessageType::INPUT) {
        wire::require(session.m_roomId != 0 &&
                      m_serverHost->Submit(wire::read_input(data, session.m_playerId)));
    } else {
        channel->Close("Unexpected message");
    }
}

void ServerNetworkRuntime::CompleteAdmission(const std::shared_ptr<TcpChannel>& channel)
{
    auto& session = m_sessions.at(channel);
    if (!session.m_helloReceived || session.m_rejected || session.m_roomId != 0) {
        return;
    }
    const auto admission = m_serverHost->Admission(session.m_playerId);
    if (admission.status == AdmissionStatus::PENDING) {
        return;
    }
    if (admission.status == AdmissionStatus::ACCEPTED) {
        session.m_roomId = admission.roomId;
        channel->Send(wire::server_hello(admission));
    } else {
        session.m_rejected = true;
        channel->SendAndClose(wire::server_hello(admission), "Admission rejected");
    }
}

void ServerNetworkRuntime::BroadcastLatestSnapshot()
{
    std::map<std::uint64_t, std::pair<std::uint64_t, wire::Bytes>> snapshots;
    for (const auto& room : m_serverHost->Snapshots()) {
        if (!room.world.players.empty()) {
            snapshots.emplace(room.roomId, std::make_pair(room.world.tick, wire::snapshot(room)));
        }
    }

    // Send() can disconnect a slow peer and remove it from m_sessions.
    const auto sessions = m_sessions;
    for (const auto& [channel, previous] : sessions) {
        (void)previous;
        CompleteAdmission(channel);
        const auto found = m_sessions.find(channel);
        if (found == m_sessions.end()) {
            continue;
        }
        auto& session = found->second;
        if (session.m_roomId == 0) {
            continue;
        }
        if (!m_serverHost->HasClient(session.m_playerId)) {
            channel->Close("Server stopped");
            continue;
        }
        const auto snapshot = snapshots.find(session.m_roomId);
        if (snapshot != snapshots.end() && snapshot->second.first > session.m_lastPublishedTick) {
            session.m_lastPublishedTick = snapshot->second.first;
            channel->Send(snapshot->second.second);
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
