#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <thread>

namespace httplib {

class Server;
struct Request;
struct Response;

} // namespace httplib

namespace MoongTangE {

class ServerHost;

// Owns HTTP workers; they only read published state or enqueue bounded core commands.
class MatchmakingApi
{
  public:
    MatchmakingApi(std::shared_ptr<ServerHost> host, const std::string& address, std::uint16_t port,
                   std::string apiKey, std::uint16_t gamePort, bool requireTicket,
                   unsigned reservationSeconds);
    ~MatchmakingApi();
    MatchmakingApi(const MatchmakingApi&) = delete;
    MatchmakingApi& operator=(const MatchmakingApi&) = delete;
    MatchmakingApi(MatchmakingApi&&) = delete;
    MatchmakingApi& operator=(MatchmakingApi&&) = delete;

    std::uint16_t Port() const;

  private:
    enum class ReservationOperation
    {
        LOOKUP,
        RESERVE,
        CANCEL
    };

    bool Authorize(const httplib::Request& request, httplib::Response& response) const;
    void Status(const httplib::Request& request, httplib::Response& response) const;
    void Reservation(const httplib::Request& request, httplib::Response& response,
                     ReservationOperation operation);

    std::shared_ptr<ServerHost> m_host;
    std::string m_apiKey;
    std::string m_instanceId;
    std::uint16_t m_gamePort{};
    bool m_requireTicket{};
    unsigned m_reservationSeconds{};
    std::uint16_t m_boundPort{};
    std::unique_ptr<httplib::Server> m_httpServer;
    std::thread m_httpThread;
};

} // namespace MoongTangE
