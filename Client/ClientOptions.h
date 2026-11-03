#pragma once

#include "Common/Protocol/AdmissionTicket.h"
#include "Common/Protocol/Nickname.h"
#include "Common/SimulationConfig.h"
#include "Common/Utility/CommandLine.h"

#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace MoongTangE {

struct ClientOptions
{
    bool smoke{};
    bool showHelp{};
    std::optional<Endpoint> remote;
    std::string ticket;
    std::string apiUrl;
    std::string apiCaFile;
    std::string playtestNickname;
    std::uint64_t worldSeed{simulation::DEFAULT_WORLD_SEED};
};

inline ClientOptions parse_client_options(std::span<const std::string_view> arguments)
{
    ClientOptions options;
    bool singleRequested = false;
    bool worldSeedRequested = false;
    OptionReader reader(arguments);

    while (!reader.Empty()) {
        const auto argument = reader.Next();
        if (argument == "--help") {
            options.showHelp = true;
            return options;
        }

        if (argument == "--single") {
            singleRequested = true;
        } else if (argument == "--smoke") {
            options.smoke = true;
        } else if (argument == "--playtest") {
            options.playtestNickname = reader.ValueFor(argument);
            if (!valid_nickname(options.playtestNickname)) {
                throw std::invalid_argument("--playtest requires a valid 1-16 character nickname");
            }
        } else if (argument == "--connect") {
            options.remote = endpoint_option(reader.ValueFor(argument));
        } else if (argument == "--api-url") {
            options.apiUrl = reader.ValueFor(argument);
            if (options.apiUrl.empty()) {
                throw std::invalid_argument("API URL cannot be empty");
            }
        } else if (argument == "--api-ca-file") {
            options.apiCaFile = reader.ValueFor(argument);
            if (options.apiCaFile.empty()) {
                throw std::invalid_argument("CA file cannot be empty");
            }
        } else if (argument == "--ticket") {
            options.ticket = reader.ValueFor(argument);
            if (!valid_admission_ticket(options.ticket)) {
                throw std::invalid_argument("Ticket must contain 64 lowercase hex characters");
            }
        } else if (argument == "--world-seed") {
            options.worldSeed = uint64_option(reader.ValueFor(argument));
            worldSeedRequested = true;
        } else {
            throw std::invalid_argument("Unknown option: " + std::string(argument));
        }
    }

    if ((singleRequested && (options.remote || !options.apiUrl.empty())) ||
        (options.remote && !options.apiUrl.empty())) {
        throw std::invalid_argument("Choose --single, --connect or --api-url");
    }
    if (!options.apiCaFile.empty() && !options.apiUrl.starts_with("https://")) {
        throw std::invalid_argument("--api-ca-file requires an HTTPS --api-url");
    }
    if (!options.ticket.empty() && !options.remote) {
        throw std::invalid_argument("--ticket requires --connect");
    }
    if (worldSeedRequested && (options.remote || !options.apiUrl.empty())) {
        throw std::invalid_argument("--world-seed is available only for single player");
    }

    return options;
}

} // namespace MoongTangE
