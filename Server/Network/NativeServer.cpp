#include "Server/Network/NativeServer.h"

#include "Server/Network/NativeServerNetworkRuntime.h"

#include <utility>

namespace MoongTangE {

NativeServer::NativeServer(std::shared_ptr<ServerHost> host, const std::string& address,
                           std::uint16_t port)
    : m_networkRuntime(std::make_unique<NativeServerNetworkRuntime>(std::move(host), address, port))
{
}

NativeServer::~NativeServer() = default;

std::uint16_t NativeServer::Port() const
{
    return m_networkRuntime->Port();
}

} // namespace MoongTangE
