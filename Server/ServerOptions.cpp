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
        } else if (argument == "--port") {
            constexpr unsigned MAX_PORT = 65535;
            options.m_port = static_cast<std::uint16_t>(
                unsigned_option(reader.ValueFor(argument), MAX_PORT, true));
        } else if (argument == "--run-for") {
            constexpr unsigned MAX_RUN_SECONDS = 24 * 60 * 60;
            options.m_runDuration =
                std::chrono::seconds(unsigned_option(reader.ValueFor(argument), MAX_RUN_SECONDS));
        } else {
            throw std::invalid_argument("Unknown option: " + std::string(argument));
        }
    }
    return options;
}
} // namespace MoongTangE