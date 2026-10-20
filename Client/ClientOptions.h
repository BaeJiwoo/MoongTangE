#pragma once

#include "Common/Protocol/AdmissionTicket.h"
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
        } else if (argument == "--connect") {
            options.remote = endpoint_option(reader.ValueFor(argument));
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

    if (singleRequested && options.remote) {
        throw std::invalid_argument("Choose --single or --connect");
    }
    if (!options.ticket.empty() && !options.remote) {
        throw std::invalid_argument("--ticket requires --connect");
    }
    if (worldSeedRequested && options.remote) {
        throw std::invalid_argument("--world-seed is available only for single player");
    }

    return options;
}

} // namespace MoongTangE
