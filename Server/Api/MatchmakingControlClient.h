#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <thread>

namespace httplib {

class Client;

} // namespace httplib

namespace MoongTangE {

class ServerHost;

struct MatchmakingControlOptions
{
    std::string m_url;
    std::string m_serverId;
    std::string m_advertisedHost;
    std::uint16_t m_gamePort{};
    std::chrono::seconds m_reservationLifetime{30};
    std::string m_controlKey;
};

// Outbound-only control-plane client. It never mutates GameWorld directly.
class MatchmakingControlClient
{
  public:
    MatchmakingControlClient(std::shared_ptr<ServerHost> host, MatchmakingControlOptions options);
    ~MatchmakingControlClient();
    MatchmakingControlClient(const MatchmakingControlClient&) = delete;
    MatchmakingControlClient& operator=(const MatchmakingControlClient&) = delete;
    MatchmakingControlClient(MatchmakingControlClient&&) = delete;
    MatchmakingControlClient& operator=(MatchmakingControlClient&&) = delete;

  private:
    struct CommandState
    {
        std::uint64_t m_id{};
        std::string m_action;
        std::string m_ticket;
    };

    void Run(std::stop_token stopToken);
    void Synchronize();
    std::string Heartbeat();
    void AcceptCommands(const std::string& body);
    void ReportCommands();
    void ReportReservationChanges();
    bool Report(const std::string& path, const std::string& body);

    std::shared_ptr<ServerHost> m_host;
    MatchmakingControlOptions m_options;
    std::unique_ptr<httplib::Client> m_httpClient;
    std::map<std::uint64_t, CommandState> m_commands;
    std::map<std::string, int> m_reportedStatuses;
    std::jthread m_controlThread;
};

} // namespace MoongTangE
