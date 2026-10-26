#pragma once

#if defined(MOONGTANGE_NETWORK_BACKEND_EPOLL) || defined(MOONGTANGE_NETWORK_BACKEND_IOCP)
#include "Server/Network/NativeServer.h"
#else
#error "MoongTangE network backend was not selected"
#endif

namespace MoongTangE {

using NetworkServer = NativeServer;

} // namespace MoongTangE
