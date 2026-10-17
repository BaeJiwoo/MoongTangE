#pragma once
#include "Server/Core/ServerHost.h"
#include <memory>
#include <string>

namespace sandbox {
class AsioServer {
public:
    AsioServer(std::shared_ptr<ServerHost> host, const std::string& address = "127.0.0.1",
               std::uint16_t port = 27015);
    ~AsioServer();
    AsioServer(const AsioServer&) = delete;
    AsioServer& operator=(const AsioServer&) = delete;
    std::uint16_t port() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace sandbox
