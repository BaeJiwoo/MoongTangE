#pragma once

#include "Server/Core/ServerHost.h"

#include <cstdint>
#include <memory>
#include <string>

namespace MoongTangE {

class NativeServerNetworkRuntime;

class NativeServer
{
  public:
    NativeServer(std::shared_ptr<ServerHost> host, const std::string& address = "127.0.0.1",
                 std::uint16_t port = 27015);
    ~NativeServer();

    NativeServer(const NativeServer&) = delete;
    NativeServer& operator=(const NativeServer&) = delete;
    NativeServer(NativeServer&&) = delete;
    NativeServer& operator=(NativeServer&&) = delete;

    std::uint16_t Port() const;

  private:
    std::unique_ptr<NativeServerNetworkRuntime> m_networkRuntime;
};

} // namespace MoongTangE
