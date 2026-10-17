#pragma once
#include "Common/Transport/IClientTransport.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace sandbox {
class TcpClientTransport final : public IClientTransport {
public:
    TcpClientTransport(const std::string& host, std::uint16_t port);
    ~TcpClientTransport() override;
    TcpClientTransport(const TcpClientTransport&) = delete;
    TcpClientTransport& operator=(const TcpClientTransport&) = delete;
    TcpClientTransport(TcpClientTransport&&) = delete;
    TcpClientTransport& operator=(TcpClientTransport&&) = delete;
    bool connected() const override;
    std::uint32_t playerId() const override;
    bool send(InputCommand input) override;
    std::optional<WorldSnapshot> receive() override;
    std::string error() const override;
    void close() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace sandbox
