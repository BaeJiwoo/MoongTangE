#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <stop_token>
#include <string>

namespace MoongTangE {

enum class ConnectionStage
{
    SESSION,
    MATCHING,
    CONNECTING,
    CONNECTED,
    CANCELLED,
    FAILED
};

struct GameAssignment
{
    std::string host;
    std::uint16_t port{};
    std::uint64_t roomId{};
    std::string ticket;
    std::chrono::steady_clock::time_point expiresAt;
};

// Single worker at a time. Reuse this object to retain the short-lived guest session.
class MatchmakingClient
{
  public:
    explicit MatchmakingClient(std::string origin, std::string caFile = {});
    ~MatchmakingClient();
    MatchmakingClient(const MatchmakingClient&) = delete;
    MatchmakingClient& operator=(const MatchmakingClient&) = delete;
    MatchmakingClient(MatchmakingClient&&) = delete;
    MatchmakingClient& operator=(MatchmakingClient&&) = delete;

    GameAssignment Match(std::stop_token stopToken,
                         const std::function<void(ConnectionStage)>& progress);
    void CancelReservation() noexcept;
    void CompleteReservation();

  private:
    class GuestApiSession;
    std::unique_ptr<GuestApiSession> m_session;
};

} // namespace MoongTangE
