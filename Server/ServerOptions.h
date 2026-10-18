#pragma once

#include "Server/Core/GameRoomOptions.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace MoongTangE {

struct ServerOptions
{
    std::uint16_t m_port{27015};
    std::chrono::seconds m_runDuration{};
    GameRoomOptions m_rooms;
    std::optional<std::uint16_t> m_apiPort;
    std::chrono::seconds m_reservationLifetime{30};
    bool m_apiPublic{};
    bool m_requireTicket{};
    bool m_publicBind{};
    bool m_showHelp{};
};

ServerOptions parse_server_options(std::span<const std::string_view> arguments);

} // namespace MoongTangE
