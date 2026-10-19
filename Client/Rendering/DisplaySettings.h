#pragma once

#include <array>

namespace MoongTangE {

struct Resolution
{
    int width;
    int height;

    bool operator==(const Resolution&) const = default;
};

enum class WindowMode
{
    WINDOWED,
    BORDERLESS,
    FULLSCREEN
};

struct DisplaySettingsState
{
    Resolution resolution{1280, 800};
    WindowMode windowMode{WindowMode::WINDOWED};
    int monitor{};
    bool vSync{};
    int frameRateLimit{60};

    bool operator==(const DisplaySettingsState&) const = default;
};

class DisplaySettings
{
  public:
    static constexpr std::array RESOLUTIONS{Resolution{1100, 720},  Resolution{1280, 720},
                                            Resolution{1280, 800},  Resolution{1600, 900},
                                            Resolution{1920, 1080}, Resolution{2560, 1440},
                                            Resolution{3840, 2160}};
    static constexpr std::array WINDOW_MODES{WindowMode::WINDOWED, WindowMode::BORDERLESS,
                                             WindowMode::FULLSCREEN};
    static constexpr std::array FRAME_RATE_LIMITS{30, 60, 120, 144, 240, 0};

    const DisplaySettingsState& Active() const;
    const DisplaySettingsState& Pending() const;
    const DisplaySettingsState& Confirmed() const;

    void SelectPreviousResolution();
    void SelectNextResolution();
    void SelectPreviousWindowMode();
    void SelectNextWindowMode();
    void SelectPreviousMonitor(int monitorCount);
    void SelectNextMonitor(int monitorCount);
    void ToggleVSync();
    void SelectPreviousFrameRateLimit();
    void SelectNextFrameRateLimit();

    void ClampMonitorCount(int monitorCount);
    void LoadConfirmed(DisplaySettingsState settings);
    void Apply();
    void Confirm();
    void Revert();
    void Cancel();

    static bool IsSupported(const DisplaySettingsState& settings);

  private:
    DisplaySettingsState m_active;
    DisplaySettingsState m_pending;
    DisplaySettingsState m_confirmed;
};

} // namespace MoongTangE
