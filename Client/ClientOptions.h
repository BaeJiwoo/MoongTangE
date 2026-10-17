#pragma once
#include "Common/Utility/CommandLine.h"
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace MoongTangE {
struct ClientOptions {
    bool smoke{};
    bool showHelp{};
    std::optional<Endpoint> remote;
};

inline ClientOptions parseClientOptions(std::span<const std::string_view> arguments) {
    ClientOptions options;
    bool singleRequested = false;
    OptionReader reader(arguments);
    while (!reader.empty()) {
        const auto argument = reader.next();
        if (argument == "--help") {
            options.showHelp = true;
            return options;
        }
        if (argument == "--single") {
            singleRequested = true;
        } else if (argument == "--smoke") {
            options.smoke = true;
        } else if (argument == "--connect") {
            options.remote = endpointOption(reader.valueFor(argument));
        } else {
            throw std::invalid_argument("Unknown option: " + std::string(argument));
        }
    }
    if (singleRequested && options.remote) {
        throw std::invalid_argument("Choose --single or --connect");
    }
    return options;
}
} // namespace MoongTangE