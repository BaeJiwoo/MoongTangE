#pragma once

#include "Client/Rendering/DisplaySettings.h"

#include <algorithm>
#include <raylib.h>
#include <stdexcept>

namespace MoongTangE {

// A single window owns the graphics context. Destruction closes it on every exit path.
class GraphicsWindow
{
  public:
    explicit GraphicsWindow(bool hidden)
    {
        SetConfigFlags(FLAG_MSAA_4X_HINT | (hidden ? FLAG_WINDOW_HIDDEN : FLAG_WINDOW_RESIZABLE));
        const auto resolution = DisplaySettingsState{}.resolution;
        InitWindow(resolution.width, resolution.height, "MoongTangE");
        if (!IsWindowReady()) {
            throw std::runtime_error("Graphics window initialization failed");
        }

        SetExitKey(KEY_NULL);
        SetTargetFPS(60);
    }

    ~GraphicsWindow()
    {
        CloseWindow();
    }

    GraphicsWindow(const GraphicsWindow&) = delete;
    GraphicsWindow& operator=(const GraphicsWindow&) = delete;
    GraphicsWindow(GraphicsWindow&&) = delete;
    GraphicsWindow& operator=(GraphicsWindow&&) = delete;

    void ApplyDisplaySettings(const DisplaySettingsState& settings)
    {
        if (m_windowMode == WindowMode::BORDERLESS) {
            ToggleBorderlessWindowed();
        } else if (m_windowMode == WindowMode::FULLSCREEN) {
            ToggleFullscreen();
        }

        SetWindowSize(settings.resolution.width, settings.resolution.height);
        const int monitorCount = GetMonitorCount();
        if (monitorCount > 0) {
            SetWindowMonitor(std::clamp(settings.monitor, 0, monitorCount - 1));
        }

        if (settings.windowMode == WindowMode::BORDERLESS) {
            ToggleBorderlessWindowed();
        } else if (settings.windowMode == WindowMode::FULLSCREEN) {
            ToggleFullscreen();
        }
        m_windowMode = settings.windowMode;

        if (settings.vSync) {
            SetWindowState(FLAG_VSYNC_HINT);
        } else {
            ClearWindowState(FLAG_VSYNC_HINT);
        }
        SetTargetFPS(settings.frameRateLimit);
    }

    void SaveScreenshot(const char* path) const
    {
        ScreenImage screenshot;
        if (!ExportImage(screenshot.Get(), path)) {
            throw std::runtime_error("Could not save smoke screenshot");
        }
    }

  private:
    class ScreenImage
    {
      public:
        ScreenImage() : m_image(LoadImageFromScreen()) {}

        ~ScreenImage()
        {
            UnloadImage(m_image);
        }

        ScreenImage(const ScreenImage&) = delete;
        ScreenImage& operator=(const ScreenImage&) = delete;
        ScreenImage(ScreenImage&&) = delete;
        ScreenImage& operator=(ScreenImage&&) = delete;

        Image Get() const
        {
            return m_image;
        }

      private:
        Image m_image;
    };

    WindowMode m_windowMode{WindowMode::WINDOWED};
};

} // namespace MoongTangE
