#pragma once

#include "Common/SimulationConfig.h"
#include "Network/Protocol/Codec.h"
#include "Server/Core/ServerHost.h"

#include <asio.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <thread>

namespace MoongTangE {

class TcpChannel;

// Owns the server network thread, connection sessions and snapshot publication.
class ServerNetworkRuntime
{
  public:
    ServerNetworkRuntime(std::shared_ptr<ServerHost> host, const std::string& address,
                         std::uint16_t port);
    ~ServerNetworkRuntime();

    ServerNetworkRuntime(const ServerNetworkRuntime&) = delete;
    ServerNetworkRuntime& operator=(const ServerNetworkRuntime&) = delete;
    ServerNetworkRuntime(ServerNetworkRuntime&&) = delete;
    ServerNetworkRuntime& operator=(ServerNetworkRuntime&&) = delete;

    std::uint16_t Port() const;

  private:
    static constexpr auto SNAPSHOT_INTERVAL = std::chrono::milliseconds(
        1000 * simulation::SNAPSHOT_INTERVAL_TICKS / simulation::TICKS_PER_SECOND);

    void CloseAllSessions();

    void AcceptNextConnection();
    void AddSession(asio::ip::tcp::socket socket);
    void RemoveSession(const std::shared_ptr<TcpChannel>& channel);

    void HandleMessage(const std::shared_ptr<TcpChannel>& channel, const wire::Bytes& data);
    void CompleteAdmission(const std::shared_ptr<TcpChannel>& channel);

    void BroadcastLatestSnapshot();
    void ScheduleSnapshotBroadcast();

    std::shared_ptr<ServerHost> m_serverHost;

    asio::io_context m_ioContext;
    asio::ip::tcp::acceptor m_acceptor;
    asio::steady_timer m_snapshotTimer;

    // Session state belongs to the network thread; the bound port is immutable after startup.
    struct NetworkSession
    {
        std::uint32_t m_playerId{};
        std::uint64_t m_roomId{};
        std::uint64_t m_lastPublishedTick{};
        bool m_helloReceived{};
        bool m_rejected{};
    };

    std::map<std::shared_ptr<TcpChannel>, NetworkSession> m_sessions;

    std::uint16_t m_boundPort{};
    bool m_stopping{};
    std::thread m_networkThread;
};

} // namespace MoongTangE
