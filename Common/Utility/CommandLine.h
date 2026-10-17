#pragma once
#include <charconv>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace sandbox {
inline unsigned unsignedOption(std::string_view text, unsigned maximum, bool allowZero = false) {
    if (text.empty()) {
        throw std::invalid_argument("Empty numeric option");
    }
    unsigned value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || value > maximum ||
        (value == 0 && !allowZero)) {
        throw std::invalid_argument("Invalid numeric option: " + std::string(text));
    }
    return value;
}

struct Endpoint {
    std::string host;
    std::uint16_t port{};
};

inline Endpoint endpointOption(std::string_view text) {
    const auto separator = text.rfind(':');
    if (separator == std::string_view::npos || separator == 0) {
        throw std::invalid_argument("Expected host:port or [IPv6]:port");
    }
    std::string host(text.substr(0, separator));
    const bool bracketed = host.front() == '[' && host.back() == ']';
    if (bracketed) {
        host = host.substr(1, host.size() - 2);
    } else if (host.find(':') != std::string::npos) {
        throw std::invalid_argument("Enclose IPv6 addresses in brackets");
    }
    if (host.empty() || host.find_first_of("[]") != std::string::npos) {
        throw std::invalid_argument("Invalid or empty host");
    }
    constexpr unsigned maximumPort = 65535;
    return {host,
            static_cast<std::uint16_t>(unsignedOption(text.substr(separator + 1), maximumPort))};
}

inline std::vector<std::string_view> commandLineArguments(int argc, char** argv) {
    std::vector<std::string_view> result;
    if (argc <= 1) {
        return result;
    }
    const std::span arguments(argv, static_cast<std::size_t>(argc));
    for (const auto argument : arguments.subspan(1)) {
        result.emplace_back(argument);
    }
    return result;
}

// A cursor owns option traversal and rejects missing values at the parsing boundary.
class OptionReader {
public:
    explicit OptionReader(std::span<const std::string_view> arguments) : arguments_(arguments) {}

    bool empty() const {
        return position_ == arguments_.size();
    }

    std::string_view next() {
        if (empty()) {
            throw std::invalid_argument("Missing command-line argument");
        }
        return arguments_[position_++];
    }

    std::string_view valueFor(std::string_view option) {
        if (empty()) {
            throw std::invalid_argument("Missing value for " + std::string(option));
        }
        return next();
    }

private:
    std::span<const std::string_view> arguments_;
    std::size_t position_{};
};
} // namespace sandbox
