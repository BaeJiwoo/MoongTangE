#pragma once

#include "Server/Core/ServerHost.h"

#include <cstdint>
#include <memory>
#include <string>

namespace MoongTangE {

class ServerNetworkRuntime;

class StandaloneAsioServer
{
  public:
    StandaloneAsioServer(std::shared_ptr<ServerHost> host, const std::string& address = "127.0.0.1",
                         std::uint16_t port = 27015);
    ~StandaloneAsioServer();

    StandaloneAsioServer(const StandaloneAsioServer&) = delete;
    StandaloneAsioServer& operator=(const StandaloneAsioServer&) = delete;
    StandaloneAsioServer(StandaloneAsioServer&&) = delete;
    StandaloneAsioServer& operator=(StandaloneAsioServer&&) = delete;

    std::uint16_t Port() const;

  private:
    std::unique_ptr<ServerNetworkRuntime> m_networkRuntime;
};

} // namespace MoongTangE
