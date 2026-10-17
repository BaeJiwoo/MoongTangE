#pragma once
#include <chrono>
#include <cstdint>
#include <span>
#include <string_view>

namespace MoongTangE {
struct ServerOptions
{
    std::uint16_t m_port{27015};
    std::chrono::seconds m_runDuration{};
    bool m_publicBind{};
    bool m_showHelp{};
};

ServerOptions parse_server_options(std::span<const std::string_view> arguments);
} // namespace MoongTangE
