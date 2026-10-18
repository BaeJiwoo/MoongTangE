#include "Server/Network/AsioServer.h"
#include "Server/Network/ServerNetworkRuntime.h"
#include <utility>

namespace MoongTangE {
AsioServer::AsioServer(std::shared_ptr<ServerHost> host, const std::string& address,
                       std::uint16_t port)
    : m_networkRuntime(std::make_unique<ServerNetworkRuntime>(std::move(host), address, port)) {}

AsioServer::~AsioServer() = default;

std::uint16_t AsioServer::port() const {
    return m_networkRuntime->port();
}
} // namespace MoongTangE
