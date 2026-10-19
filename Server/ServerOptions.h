#pragma once

#include "Server/Core/GameRoomOptions.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace MoongTangE {

struct ServerOptions
{
    std::uint16_t m_port{27015};
    std::chrono::seconds m_runDuration{};
    GameRoomOptions m_rooms;
    std::optional<std::string> m_matchmakingUrl;
    std::string m_serverId;
    std::string m_advertisedHost;
    std::chrono::seconds m_reservationLifetime{30};
    bool m_requireTicket{};
    bool m_publicBind{};
    bool m_showHelp{};
};

ServerOptions parse_server_options(std::span<const std::string_view> arguments);

} // namespace MoongTangE
