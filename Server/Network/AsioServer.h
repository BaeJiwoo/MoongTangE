#pragma once

#include "Server/Core/ServerHost.h"

#include <cstdint>
#include <memory>
#include <string>

namespace MoongTangE {

class ServerNetworkRuntime;

class AsioServer
{
  public:
    AsioServer(std::shared_ptr<ServerHost> host, const std::string& address = "127.0.0.1",
               std::uint16_t port = 27015);
    ~AsioServer();

    AsioServer(const AsioServer&) = delete;
    AsioServer& operator=(const AsioServer&) = delete;
    AsioServer(AsioServer&&) = delete;
    AsioServer& operator=(AsioServer&&) = delete;

    std::uint16_t Port() const;

  private:
    std::unique_ptr<ServerNetworkRuntime> m_networkRuntime;
};

} // namespace MoongTangE
