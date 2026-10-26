#pragma once

#include "Common/SimulationConfig.h"
#include "Network/Native/NativeEventLoop.h"
#include "Network/Protocol/Codec.h"
#include "Server/Core/ServerHost.h"

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <thread>

namespace MoongTangE {

class NativeTcpChannel;

// Owns the native server event loop, connection sessions and snapshot publication.
class NativeServerNetworkRuntime
{
  public:
    NativeServerNetworkRuntime(std::shared_ptr<ServerHost> host, const std::string& address,
                               std::uint16_t port);
    ~NativeServerNetworkRuntime();

    NativeServerNetworkRuntime(const NativeServerNetworkRuntime&) = delete;
    NativeServerNetworkRuntime& operator=(const NativeServerNetworkRuntime&) = delete;
    NativeServerNetworkRuntime(NativeServerNetworkRuntime&&) = delete;
    NativeServerNetworkRuntime& operator=(NativeServerNetworkRuntime&&) = delete;

    std::uint16_t Port() const;

  private:
    static constexpr auto SNAPSHOT_INTERVAL = std::chrono::milliseconds(
        1000 * simulation::SNAPSHOT_INTERVAL_TICKS / simulation::TICKS_PER_SECOND);

    void CloseAllSessions();
    void AddSession(NativeEventLoop::ConnectionId connection);
    void RemoveSession(const std::shared_ptr<NativeTcpChannel>& channel);

    void HandleMessage(const std::shared_ptr<NativeTcpChannel>& channel, const wire::Bytes& data);
    void CompleteAdmission(const std::shared_ptr<NativeTcpChannel>& channel);

    void BroadcastLatestSnapshot();
    void ScheduleSnapshotBroadcast();

    std::shared_ptr<ServerHost> m_serverHost;
    NativeEventLoop m_eventLoop;

    struct NetworkSession
    {
        std::uint32_t m_playerId{};
        std::uint64_t m_roomId{};
        std::uint64_t m_lastPublishedTick{};
        bool m_helloReceived{};
        bool m_rejected{};
    };

    std::map<std::shared_ptr<NativeTcpChannel>, NetworkSession> m_sessions;

    std::uint16_t m_boundPort{};
    bool m_stopping{};
    std::thread m_networkThread;
};

} // namespace MoongTangE
