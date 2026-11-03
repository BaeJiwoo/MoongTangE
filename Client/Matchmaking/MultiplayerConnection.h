#pragma once

#include "Client/Matchmaking/MatchmakingClient.h"
#include "Common/Transport/IClientTransport.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace MoongTangE {

// Owns one attempt, including HTTP cleanup and the pending native TCP handshake.
class MultiplayerConnection
{
  public:
    MultiplayerConnection(std::shared_ptr<MatchmakingClient> api, std::string nickname);
    ~MultiplayerConnection();
    MultiplayerConnection(const MultiplayerConnection&) = delete;
    MultiplayerConnection& operator=(const MultiplayerConnection&) = delete;
    MultiplayerConnection(MultiplayerConnection&&) = delete;
    MultiplayerConnection& operator=(MultiplayerConnection&&) = delete;

    void Cancel();
    ConnectionStage Stage() const;
    bool Finished() const;
    std::string Error() const;
    std::unique_ptr<IClientTransport> TakeTransport();

  private:
    void Run(std::stop_token stopToken, const std::string& nickname);
    void SetStage(ConnectionStage stage);

    std::shared_ptr<MatchmakingClient> m_api;
    std::atomic<ConnectionStage> m_stage{ConnectionStage::SESSION};
    std::atomic<bool> m_finished{false};
    mutable std::mutex m_mutex;
    std::string m_error;
    std::unique_ptr<IClientTransport> m_transport;
    std::jthread m_worker;
};

} // namespace MoongTangE
