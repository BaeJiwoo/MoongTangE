#include "Server/Network/StandaloneAsioServer.h"

#include "Server/Network/ServerNetworkRuntime.h"

#include <utility>

namespace MoongTangE {

StandaloneAsioServer::StandaloneAsioServer(std::shared_ptr<ServerHost> host,
                                           const std::string& address, std::uint16_t port)
    : m_networkRuntime(std::make_unique<ServerNetworkRuntime>(std::move(host), address, port))
{
}

StandaloneAsioServer::~StandaloneAsioServer() = default;

std::uint16_t StandaloneAsioServer::Port() const
{
    return m_networkRuntime->Port();
}

} // namespace MoongTangE
