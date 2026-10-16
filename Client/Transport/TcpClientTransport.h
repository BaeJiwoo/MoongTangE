#pragma once
#include "Common/Transport/IClientTransport.h"
#include <memory>
namespace sandbox {
class TcpClientTransport final : public IClientTransport {
public:
    TcpClientTransport(const std::string& host,std::uint16_t port);
    ~TcpClientTransport() override;
    bool connected() const override;
    std::uint32_t playerId() const override;
    bool send(InputCommand input) override;
    std::optional<WorldSnapshot> receive() override;
    std::string error() const override;
    void close() override;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
