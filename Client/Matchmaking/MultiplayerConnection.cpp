#include "Client/Matchmaking/MultiplayerConnection.h"

#include "Client/Transport/TcpClientTransport.h"
#include "Common/Protocol/Nickname.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>

namespace MoongTangE {

MultiplayerConnection::MultiplayerConnection(std::shared_ptr<MatchmakingClient> api,
                                             std::string nickname)
    : m_api(std::move(api))
{
    if (!m_api || !valid_nickname(nickname)) {
        throw std::invalid_argument("Enter a valid nickname before matchmaking");
    }
    m_worker = std::jthread([this, nickname = std::move(nickname)](std::stop_token stopToken) {
        Run(stopToken, nickname);
    });
}

MultiplayerConnection::~MultiplayerConnection()
{
    Cancel();
    m_worker.join();
}

void MultiplayerConnection::Cancel()
{
    {
        const std::lock_guard lock(m_mutex);
        m_stage = ConnectionStage::CANCELLED;
    }
    m_worker.request_stop();
}

void MultiplayerConnection::SetStage(ConnectionStage stage)
{
    const std::lock_guard lock(m_mutex);
    if (m_stage != ConnectionStage::CANCELLED) {
        m_stage = stage;
    }
}

ConnectionStage MultiplayerConnection::Stage() const
{
    return m_stage.load();
}

bool MultiplayerConnection::Finished() const
{
    return m_finished.load();
}

std::string MultiplayerConnection::Error() const
{
    const std::lock_guard lock(m_mutex);
    return m_error;
}

std::unique_ptr<IClientTransport> MultiplayerConnection::TakeTransport()
{
    if (!Finished() || Stage() != ConnectionStage::CONNECTED) {
        return {};
    }
    const std::lock_guard lock(m_mutex);
    return std::move(m_transport);
}

void MultiplayerConnection::Run(std::stop_token stopToken, const std::string& nickname)
{
    try {
        const auto assignment = m_api->Match(stopToken, [this, stopToken](ConnectionStage stage) {
            if (!stopToken.stop_requested()) {
                SetStage(stage);
            }
        });
        if (stopToken.stop_requested()) {
            throw std::runtime_error("Cancelled");
        }
        SetStage(ConnectionStage::CONNECTING);
        auto transport = std::make_unique<TcpClientTransport>(assignment.host, assignment.port,
                                                              assignment.ticket, nickname);
        const auto deadline = std::min(assignment.expiresAt,
                                       std::chrono::steady_clock::now() + std::chrono::seconds(5));
        while (!transport->Connected()) {
            if (stopToken.stop_requested() || m_stage == ConnectionStage::CANCELLED) {
                throw std::runtime_error("Cancelled");
            }
            if (const auto error = transport->Error(); !error.empty()) {
                throw std::runtime_error(error);
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                throw std::runtime_error("Game server connection timed out");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (transport->RoomId() != assignment.roomId) {
            throw std::runtime_error("Game server returned a different room");
        }
        {
            const std::lock_guard lock(m_mutex);
            if (stopToken.stop_requested() || m_stage == ConnectionStage::CANCELLED) {
                throw std::runtime_error("Cancelled");
            }
            m_transport = std::move(transport);
            m_api->CompleteReservation();
            m_stage = ConnectionStage::CONNECTED;
        }
    } catch (const std::exception& error) {
        m_api->CancelReservation();
        const std::lock_guard lock(m_mutex);
        m_error = stopToken.stop_requested() ? "" : error.what();
        m_stage = stopToken.stop_requested() ? ConnectionStage::CANCELLED : ConnectionStage::FAILED;
    }
    m_finished = true;
}

} // namespace MoongTangE
