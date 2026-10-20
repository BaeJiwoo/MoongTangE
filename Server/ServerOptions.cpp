#include "Server/ServerOptions.h"

#include "Common/Utility/CommandLine.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace MoongTangE {
namespace {

bool valid_server_id(std::string_view value)
{
    return !value.empty() && value.size() <= 64 &&
           std::all_of(value.begin(), value.end(), [](char character) {
               return (character >= 'A' && character <= 'Z') ||
                      (character >= 'a' && character <= 'z') ||
                      (character >= '0' && character <= '9') || character == '-' ||
                      character == '_';
           });
}

bool valid_host(std::string_view value)
{
    return !value.empty() && value.size() <= 253 &&
           std::none_of(value.begin(), value.end(), [](unsigned char character) {
               return std::isspace(character) != 0;
           });
}

bool valid_control_url(std::string_view value)
{
    constexpr std::string_view HTTP_PREFIX = "http://";
    if (!value.starts_with(HTTP_PREFIX) || value.size() > 512 || !valid_host(value)) {
        return false;
    }
    const auto authority = value.substr(HTTP_PREFIX.size());
    return !authority.empty() && authority.find_first_of("/?#@") == std::string_view::npos;
}

} // namespace

ServerOptions parse_server_options(std::span<const std::string_view> arguments)
{
    ServerOptions options;
    OptionReader reader(arguments);

    while (!reader.Empty()) {
        const auto argument = reader.Next();
        if (argument == "--help") {
            options.m_showHelp = true;
            return options;
        }

        if (argument == "--public") {
            options.m_publicBind = true;
        } else if (argument == "--require-ticket") {
            options.m_requireTicket = true;
        } else if (argument == "--matchmaking-url") {
            options.m_matchmakingUrl = reader.ValueFor(argument);
            if (!valid_control_url(*options.m_matchmakingUrl)) {
                throw std::invalid_argument("Matchmaking URL must be an http:// origin");
            }
        } else if (argument == "--server-id") {
            options.m_serverId = reader.ValueFor(argument);
            if (!valid_server_id(options.m_serverId)) {
                throw std::invalid_argument("Server ID must contain 1..64 safe characters");
            }
        } else if (argument == "--advertise-host") {
            options.m_advertisedHost = reader.ValueFor(argument);
            if (!valid_host(options.m_advertisedHost)) {
                throw std::invalid_argument("Invalid advertised host");
            }
        } else if (argument == "--reservation-seconds") {
            options.m_reservationLifetime =
                std::chrono::seconds(unsigned_option(reader.ValueFor(argument), 300));
        } else if (argument == "--port") {
            constexpr unsigned MAX_PORT = 65535;
            options.m_port = static_cast<std::uint16_t>(
                unsigned_option(reader.ValueFor(argument), MAX_PORT, true));
        } else if (argument == "--run-for") {
            constexpr unsigned MAX_RUN_SECONDS = 24 * 60 * 60;
            options.m_runDuration =
                std::chrono::seconds(unsigned_option(reader.ValueFor(argument), MAX_RUN_SECONDS));
        } else if (argument == "--max-rooms") {
            options.m_rooms.m_maxRooms = unsigned_option(
                reader.ValueFor(argument), static_cast<unsigned>(GameRoomOptions::MAX_ROOM_LIMIT));
        } else if (argument == "--empty-room-seconds") {
            options.m_rooms.m_emptyRoomLifetime = std::chrono::seconds(unsigned_option(
                reader.ValueFor(argument), GameRoomOptions::MAX_EMPTY_ROOM_SECONDS, true));
        } else if (argument == "--world-seed") {
            options.m_rooms.m_worldSeed = uint64_option(reader.ValueFor(argument));
        } else {
            throw std::invalid_argument("Unknown option: " + std::string(argument));
        }
    }

    const bool anyControlOption = options.m_matchmakingUrl || !options.m_serverId.empty() ||
                                  !options.m_advertisedHost.empty();
    if (anyControlOption && (!options.m_matchmakingUrl || options.m_serverId.empty() ||
                             options.m_advertisedHost.empty())) {
        throw std::invalid_argument(
            "--matchmaking-url, --server-id and --advertise-host must be used together");
    }
    if (options.m_requireTicket && !options.m_matchmakingUrl) {
        throw std::invalid_argument("--require-ticket requires --matchmaking-url");
    }
    return options;
}

} // namespace MoongTangE
