#include "Server/Network/NativeServerNetworkRuntime.h"

#include "Common/Protocol/Nickname.h"
#include "Network/Native/NativeTcpChannel.h"

#include <stdexcept>
#include <utility>

namespace MoongTangE {

NativeServerNetworkRuntime::NativeServerNetworkRuntime(std::shared_ptr<ServerHost> host,
                                                       const std::string& address,
                                                       std::uint16_t port)
    : m_serverHost(std::move(host))
{
    if (!m_serverHost || !m_serverHost->Running()) {
        throw std::invalid_argument("ServerCore is not running");
    }

    m_boundPort =
        m_eventLoop.Listen(address, port, [this](NativeEventLoop::ConnectionId connection) {
            if (m_stopping || m_sessions.size() >= m_serverHost->Capacity() * 2) {
                m_eventLoop.Close(connection);
                return;
            }
            AddSession(connection);
        });
    ScheduleSnapshotBroadcast();

    m_networkThread = std::thread([this] {
        m_eventLoop.Run();
    });
}

NativeServerNetworkRuntime::~NativeServerNetworkRuntime()
{
    m_eventLoop.Post([this] {
        CloseAllSessions();
        m_eventLoop.Stop();
    });
    if (m_networkThread.joinable()) {
        m_networkThread.join();
    }
}

std::uint16_t NativeServerNetworkRuntime::Port() const
{
    return m_boundPort;
}

void NativeServerNetworkRuntime::CloseAllSessions()
{
    m_stopping = true;

    const auto sessions = m_sessions;
    for (const auto& [channel, session] : sessions) {
        (void)session;
        channel->Close("Server stopping");
    }
}

void NativeServerNetworkRuntime::AddSession(NativeEventLoop::ConnectionId connection)
{
    auto channel = std::make_shared<NativeTcpChannel>(m_eventLoop, connection);
    m_sessions.emplace(channel, NetworkSession{});

    const std::weak_ptr<NativeTcpChannel> weakChannel = channel;
    channel->Start(
        [this, weakChannel](const wire::Bytes& data) {
            if (auto activeChannel = weakChannel.lock()) {
                HandleMessage(activeChannel, data);
            }
        },
        [this, weakChannel](std::string) {
            if (auto activeChannel = weakChannel.lock()) {
                RemoveSession(activeChannel);
            }
        });
}

void NativeServerNetworkRuntime::RemoveSession(const std::shared_ptr<NativeTcpChannel>& channel)
{
    const auto session = m_sessions.find(channel);
    if (session != m_sessions.end()) {
        m_serverHost->Disconnect(session->second.m_playerId);
        m_sessions.erase(session);
    }
}

void NativeServerNetworkRuntime::HandleMessage(const std::shared_ptr<NativeTcpChannel>& channel,
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

void NativeServerNetworkRuntime::CompleteAdmission(const std::shared_ptr<NativeTcpChannel>& channel)
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

void NativeServerNetworkRuntime::BroadcastLatestSnapshot()
{
    std::map<std::uint64_t, std::pair<std::uint64_t, wire::Bytes>> snapshots;
    for (const auto& room : m_serverHost->Snapshots()) {
        if (!room.world.players.empty()) {
            snapshots.emplace(room.roomId, std::make_pair(room.world.tick, wire::snapshot(room)));
        }
    }

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

void NativeServerNetworkRuntime::ScheduleSnapshotBroadcast()
{
    m_eventLoop.Schedule(SNAPSHOT_INTERVAL, [this] {
        if (m_stopping) {
            return;
        }

        BroadcastLatestSnapshot();
        ScheduleSnapshotBroadcast();
    });
}

} // namespace MoongTangE
