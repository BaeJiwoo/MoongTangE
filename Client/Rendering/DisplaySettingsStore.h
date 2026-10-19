#pragma once

#include "Client/Rendering/DisplaySettings.h"

#include <filesystem>
#include <optional>

namespace MoongTangE {

class DisplaySettingsStore
{
  public:
    explicit DisplaySettingsStore(std::filesystem::path path = DefaultPath());

    std::optional<DisplaySettingsState> Load() const;
    bool Save(const DisplaySettingsState& settings) const;

    static std::filesystem::path DefaultPath();

  private:
    std::filesystem::path m_path;
};

} // namespace MoongTangE
