#pragma once

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
        InitWindow(1100, 720, "MoongTangE");
        if (!IsWindowReady()) {
            throw std::runtime_error("Graphics window initialization failed");
        }

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
};

} // namespace MoongTangE
