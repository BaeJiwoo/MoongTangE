#include "Server/ServerOptions.h"

#include "Common/Utility/CommandLine.h"

#include <stdexcept>
#include <string>

namespace MoongTangE {

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
        } else if (argument == "--api-public") {
            options.m_apiPublic = true;
        } else if (argument == "--require-ticket") {
            options.m_requireTicket = true;
        } else if (argument == "--api-port") {
            options.m_apiPort =
                static_cast<std::uint16_t>(unsigned_option(reader.ValueFor(argument), 65535, true));
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
        } else {
            throw std::invalid_argument("Unknown option: " + std::string(argument));
        }
    }

    if ((options.m_apiPublic || options.m_requireTicket) && !options.m_apiPort) {
        throw std::invalid_argument("--api-public and --require-ticket require --api-port");
    }
    return options;
}

} // namespace MoongTangE
