#pragma once

#include <algorithm>

namespace MoongTangE {

inline constexpr int DEFAULT_WINDOW_WIDTH = 1280;
inline constexpr int DEFAULT_WINDOW_HEIGHT = 800;
inline constexpr float REFERENCE_WINDOW_WIDTH = 1100.0f;
inline constexpr float REFERENCE_WINDOW_HEIGHT = 720.0f;

inline float ui_scale(int screenWidth, int screenHeight)
{
    const float horizontalScale = static_cast<float>(screenWidth) / REFERENCE_WINDOW_WIDTH;
    const float verticalScale = static_cast<float>(screenHeight) / REFERENCE_WINDOW_HEIGHT;
    return std::max(0.01f, std::min(horizontalScale, verticalScale));
}

} // namespace MoongTangE
