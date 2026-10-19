#include "Client/Rendering/DisplaySettings.h"

#include <algorithm>
#include <cstddef>
#include <iterator>

namespace MoongTangE {
namespace {

template <typename T, std::size_t SIZE>
T select_previous(const std::array<T, SIZE>& values, T selected)
{
    const auto current = std::find(values.begin(), values.end(), selected);
    if (current == values.end() || current == values.begin()) {
        return values.back();
    }
    return *std::prev(current);
}

template <typename T, std::size_t SIZE> T select_next(const std::array<T, SIZE>& values, T selected)
{
    const auto current = std::find(values.begin(), values.end(), selected);
    if (current == values.end() || std::next(current) == values.end()) {
        return values.front();
    }
    return *std::next(current);
}

int clamp_monitor(int monitor, int monitorCount)
{
    return std::clamp(monitor, 0, std::max(1, monitorCount) - 1);
}

} // namespace

const DisplaySettingsState& DisplaySettings::Active() const
{
    return m_active;
}

const DisplaySettingsState& DisplaySettings::Pending() const
{
    return m_pending;
}

const DisplaySettingsState& DisplaySettings::Confirmed() const
{
    return m_confirmed;
}

void DisplaySettings::SelectPreviousResolution()
{
    m_pending.resolution = select_previous(RESOLUTIONS, m_pending.resolution);
}

void DisplaySettings::SelectNextResolution()
{
    m_pending.resolution = select_next(RESOLUTIONS, m_pending.resolution);
}

void DisplaySettings::SelectPreviousWindowMode()
{
    m_pending.windowMode = select_previous(WINDOW_MODES, m_pending.windowMode);
}

void DisplaySettings::SelectNextWindowMode()
{
    m_pending.windowMode = select_next(WINDOW_MODES, m_pending.windowMode);
}

void DisplaySettings::SelectPreviousMonitor(int monitorCount)
{
    const int count = std::max(1, monitorCount);
    m_pending.monitor = (clamp_monitor(m_pending.monitor, count) + count - 1) % count;
}

void DisplaySettings::SelectNextMonitor(int monitorCount)
{
    const int count = std::max(1, monitorCount);
    m_pending.monitor = (clamp_monitor(m_pending.monitor, count) + 1) % count;
}

void DisplaySettings::ToggleVSync()
{
    m_pending.vSync = !m_pending.vSync;
}

void DisplaySettings::SelectPreviousFrameRateLimit()
{
    m_pending.frameRateLimit = select_previous(FRAME_RATE_LIMITS, m_pending.frameRateLimit);
}

void DisplaySettings::SelectNextFrameRateLimit()
{
    m_pending.frameRateLimit = select_next(FRAME_RATE_LIMITS, m_pending.frameRateLimit);
}

void DisplaySettings::ClampMonitorCount(int monitorCount)
{
    m_active.monitor = clamp_monitor(m_active.monitor, monitorCount);
    m_pending.monitor = clamp_monitor(m_pending.monitor, monitorCount);
    m_confirmed.monitor = clamp_monitor(m_confirmed.monitor, monitorCount);
}

void DisplaySettings::LoadConfirmed(DisplaySettingsState settings)
{
    if (!IsSupported(settings)) {
        return;
    }

    m_active = settings;
    m_pending = settings;
    m_confirmed = settings;
}

void DisplaySettings::Apply()
{
    m_active = m_pending;
}

void DisplaySettings::Confirm()
{
    m_confirmed = m_active;
    m_pending = m_active;
}

void DisplaySettings::Revert()
{
    m_active = m_confirmed;
    m_pending = m_confirmed;
}

void DisplaySettings::Cancel()
{
    m_pending = m_active;
}

bool DisplaySettings::IsSupported(const DisplaySettingsState& settings)
{
    return std::find(RESOLUTIONS.begin(), RESOLUTIONS.end(), settings.resolution) !=
               RESOLUTIONS.end() &&
           std::find(WINDOW_MODES.begin(), WINDOW_MODES.end(), settings.windowMode) !=
               WINDOW_MODES.end() &&
           settings.monitor >= 0 &&
           std::find(FRAME_RATE_LIMITS.begin(), FRAME_RATE_LIMITS.end(), settings.frameRateLimit) !=
               FRAME_RATE_LIMITS.end();
}

} // namespace MoongTangE
