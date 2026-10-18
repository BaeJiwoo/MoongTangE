#pragma once

#include "Common/Transport/IClientTransport.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace MoongTangE {

class TcpClientTransport final : public IClientTransport
{
  public:
    TcpClientTransport(const std::string& host, std::uint16_t port, std::string ticket = {});
    ~TcpClientTransport() override;

    TcpClientTransport(const TcpClientTransport&) = delete;
    TcpClientTransport& operator=(const TcpClientTransport&) = delete;
    TcpClientTransport(TcpClientTransport&&) = delete;
    TcpClientTransport& operator=(TcpClientTransport&&) = delete;

    bool Connected() const override;
    std::uint32_t PlayerId() const override;
    std::uint64_t RoomId() const override;

    bool Send(InputCommand input) override;
    std::optional<WorldSnapshot> Receive() override;

    std::string Error() const override;

    void Close() override;

  private:
    class ClientNetworkRuntime;
    std::unique_ptr<ClientNetworkRuntime> m_networkRuntime;
};

} // namespace MoongTangE
