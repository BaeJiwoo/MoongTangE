#pragma once

#include "Common/Transport/IClientTransport.h"
#include "Server/Core/ServerHost.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace MoongTangE {

class LocalTransport final : public IClientTransport
{
  public:
    explicit LocalTransport(std::shared_ptr<ServerHost> host,
                            const std::string& nickname = "Player")
        : m_serverHost(std::move(host))
    {
        if (!m_serverHost) {
            throw std::invalid_argument("Local transport requires a server");
        }

        m_playerId = m_serverHost->Connect({}, nickname);
        if (m_playerId == 0) {
            throw std::runtime_error("Local server unavailable or full");
        }
    }

    ~LocalTransport() override
    {
        Close();
    }

    LocalTransport(const LocalTransport&) = delete;
    LocalTransport& operator=(const LocalTransport&) = delete;
    LocalTransport(LocalTransport&&) = delete;
    LocalTransport& operator=(LocalTransport&&) = delete;

    bool Connected() const override
    {
        return m_playerId && m_serverHost->HasClient(m_playerId);
    }

    std::uint32_t PlayerId() const override
    {
        return Connected() ? m_playerId : 0;
    }

    std::uint64_t RoomId() const override
    {
        return m_serverHost->Admission(m_playerId).roomId;
    }

    bool Send(InputCommand input) override
    {
        return Connected() && input.playerId == m_playerId && m_serverHost->Submit(input);
    }

    std::optional<WorldSnapshot> Receive() override
    {
        if (!Connected()) {
            return {};
        }

        auto state = m_serverHost->SnapshotFor(m_playerId);
        if (!state || !state->world.tick || state->world.tick <= m_lastReceivedTick) {
            return {};
        }

        m_lastReceivedTick = state->world.tick;
        return std::move(state->world);
    }

    std::string Error() const override
    {
        switch (m_serverHost->Admission(m_playerId).status) {
        case AdmissionStatus::PENDING:
        case AdmissionStatus::ACCEPTED:
            return "";
        case AdmissionStatus::FULL:
            return "All game rooms are full";
        case AdmissionStatus::INVALID_NICKNAME:
            return "Enter a valid nickname before starting";
        case AdmissionStatus::UNAVAILABLE:
            return "Local server disconnected";
        }
        return "Local server disconnected";
    }

    void Close() override
    {
        if (m_playerId) {
            m_serverHost->Disconnect(m_playerId);
            m_playerId = 0;
        }
    }

  private:
    std::shared_ptr<ServerHost> m_serverHost;
    std::uint32_t m_playerId{};
    std::uint64_t m_lastReceivedTick{};
};

} // namespace MoongTangE
