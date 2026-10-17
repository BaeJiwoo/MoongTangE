#pragma once
#include "Common/Utility/CommandLine.h"
#include <chrono>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace sandbox {
struct ServerOptions {
    std::uint16_t port{27015};
    std::chrono::seconds duration{};
    bool publicBind{};
    bool showHelp{};
};

inline ServerOptions parseServerOptions(std::span<const std::string_view> arguments) {
    ServerOptions options;
    OptionReader reader(arguments);
    while (!reader.empty()) {
        const auto argument = reader.next();
        if (argument == "--help") {
            options.showHelp = true;
            return options;
        }
        if (argument == "--public") {
            options.publicBind = true;
        } else if (argument == "--port") {
            constexpr unsigned maximumPort = 65535;
            options.port = static_cast<std::uint16_t>(
                unsignedOption(reader.valueFor(argument), maximumPort, true));
        } else if (argument == "--run-for") {
            constexpr unsigned maximumRunSeconds = 24 * 60 * 60;
            options.duration =
                std::chrono::seconds(unsignedOption(reader.valueFor(argument), maximumRunSeconds));
        } else {
            throw std::invalid_argument("Unknown option: " + std::string(argument));
        }
    }
    return options;
}
} // namespace sandbox