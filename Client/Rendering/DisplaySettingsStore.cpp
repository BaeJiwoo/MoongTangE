#include "Client/Rendering/DisplaySettingsStore.h"

#include <charconv>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace MoongTangE {
namespace {

constexpr int SETTINGS_VERSION = 1;

std::optional<int> parse_integer(std::string_view text)
{
    int value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

std::optional<WindowMode> parse_window_mode(const std::string& text)
{
    if (text == "windowed") {
        return WindowMode::WINDOWED;
    }
    if (text == "borderless") {
        return WindowMode::BORDERLESS;
    }
    if (text == "fullscreen") {
        return WindowMode::FULLSCREEN;
    }
    return std::nullopt;
}

const char* window_mode_value(WindowMode windowMode)
{
    switch (windowMode) {
    case WindowMode::WINDOWED:
        return "windowed";
    case WindowMode::BORDERLESS:
        return "borderless";
    case WindowMode::FULLSCREEN:
        return "fullscreen";
    }
    return "windowed";
}

std::optional<std::map<std::string, std::string>> read_values(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) {
        return std::nullopt;
    }

    std::map<std::string, std::string> values;
    for (std::string line; std::getline(input, line);) {
        const auto separator = line.find('=');
        if (separator == std::string::npos || separator == 0 || separator == line.size() - 1) {
            return std::nullopt;
        }
        values[line.substr(0, separator)] = line.substr(separator + 1);
    }
    return values;
}

std::optional<std::string> value_for(const std::map<std::string, std::string>& values,
                                     const char* key)
{
    const auto value = values.find(key);
    if (value == values.end()) {
        return std::nullopt;
    }
    return value->second;
}

std::filesystem::path environment_path(const char* name)
{
    const char* value = std::getenv(name);
    return value && value[0] != '\0' ? std::filesystem::path(value) : std::filesystem::path{};
}

} // namespace

DisplaySettingsStore::DisplaySettingsStore(std::filesystem::path path) : m_path(std::move(path)) {}

std::optional<DisplaySettingsState> DisplaySettingsStore::Load() const
{
    if (m_path.empty()) {
        return std::nullopt;
    }

    const auto values = read_values(m_path);
    if (!values) {
        return std::nullopt;
    }

    const auto versionText = value_for(*values, "version");
    const auto widthText = value_for(*values, "width");
    const auto heightText = value_for(*values, "height");
    const auto windowModeText = value_for(*values, "window_mode");
    const auto monitorText = value_for(*values, "monitor");
    const auto vSyncText = value_for(*values, "vsync");
    const auto frameRateLimitText = value_for(*values, "frame_rate_limit");
    if (!versionText || !widthText || !heightText || !windowModeText || !monitorText ||
        !vSyncText || !frameRateLimitText) {
        return std::nullopt;
    }

    const auto version = parse_integer(*versionText);
    const auto width = parse_integer(*widthText);
    const auto height = parse_integer(*heightText);
    const auto windowMode = parse_window_mode(*windowModeText);
    const auto monitor = parse_integer(*monitorText);
    const auto vSync = parse_integer(*vSyncText);
    const auto frameRateLimit = parse_integer(*frameRateLimitText);
    if (!version || *version != SETTINGS_VERSION || !width || !height || !windowMode || !monitor ||
        !vSync || (*vSync != 0 && *vSync != 1) || !frameRateLimit) {
        return std::nullopt;
    }

    const DisplaySettingsState settings{
        {*width, *height}, *windowMode, *monitor, *vSync == 1, *frameRateLimit};
    if (!DisplaySettings::IsSupported(settings)) {
        return std::nullopt;
    }
    return settings;
}

bool DisplaySettingsStore::Save(const DisplaySettingsState& settings) const
{
    if (m_path.empty() || !DisplaySettings::IsSupported(settings)) {
        return false;
    }

    std::error_code error;
    const auto parent = m_path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            return false;
        }
    }

    std::ofstream output(m_path, std::ios::trunc);
    if (!output) {
        return false;
    }
    output << "version=" << SETTINGS_VERSION << '\n'
           << "width=" << settings.resolution.width << '\n'
           << "height=" << settings.resolution.height << '\n'
           << "window_mode=" << window_mode_value(settings.windowMode) << '\n'
           << "monitor=" << settings.monitor << '\n'
           << "vsync=" << (settings.vSync ? 1 : 0) << '\n'
           << "frame_rate_limit=" << settings.frameRateLimit << '\n';
    return output.good();
}

std::filesystem::path DisplaySettingsStore::DefaultPath()
{
#if defined(_WIN32)
    auto base = environment_path("APPDATA");
    if (base.empty()) {
        const auto userProfile = environment_path("USERPROFILE");
        if (userProfile.empty()) {
            return {};
        }
        base = userProfile / "AppData" / "Roaming";
    }
#elif defined(__linux__)
    auto base = environment_path("XDG_CONFIG_HOME");
    if (base.empty()) {
        const auto home = environment_path("HOME");
        if (home.empty()) {
            return {};
        }
        base = home / ".config";
    }
#else
    return {};
#endif
    return base / "MoongTangE" / "display.cfg";
}

} // namespace MoongTangE
