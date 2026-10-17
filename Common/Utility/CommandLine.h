#pragma once
#include <charconv>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace sandbox {
inline unsigned unsignedOption(std::string_view text, unsigned maximum, bool allowZero = false) {
    unsigned value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || value > maximum ||
        (!value && !allowZero)) {
        throw std::invalid_argument("Invalid numeric option: " + std::string(text));
    }
    return value;
}

inline std::pair<std::string, std::uint16_t> endpointOption(std::string_view text) {
    const auto separator = text.rfind(':');
    if (separator == std::string_view::npos || separator == 0) {
        throw std::invalid_argument("Expected host:port or [IPv6]:port");
    }
    std::string host(text.substr(0, separator));
    if (host.front() == '[' && host.back() == ']') {
        host = host.substr(1, host.size() - 2);
    } else if (host.find(':') != std::string::npos) {
        throw std::invalid_argument("Enclose IPv6 addresses in brackets");
    }
    if (host.empty()) {
        throw std::invalid_argument("Empty host");
    }
    return {host, static_cast<std::uint16_t>(unsignedOption(text.substr(separator + 1), 65535))};
}
} // namespace sandbox
