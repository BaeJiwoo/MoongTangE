#pragma once

#if defined(MOONGTANGE_NETWORK_BACKEND_ASIO)
#include "Server/Network/AsioServer.h"
#elif defined(MOONGTANGE_NETWORK_BACKEND_EPOLL) || defined(MOONGTANGE_NETWORK_BACKEND_IOCP)
#include "Server/Network/NativeServer.h"
#else
#error "MoongTangE network backend was not selected"
#endif

namespace MoongTangE {

#if defined(MOONGTANGE_NETWORK_BACKEND_ASIO)
using NetworkServer = AsioServer;
#else
using NetworkServer = NativeServer;
#endif

} // namespace MoongTangE
