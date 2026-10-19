#pragma once

#include <algorithm>

namespace MoongTangE {

inline constexpr int DEFAULT_WINDOW_WIDTH = 1280;
inline constexpr int DEFAULT_WINDOW_HEIGHT = 800;
inline constexpr float REFERENCE_WINDOW_WIDTH = 1100.0f;
inline constexpr float REFERENCE_WINDOW_HEIGHT = 720.0f;
inline constexpr float MINIMUM_UI_SCALE = 0.75f;
inline constexpr float MAXIMUM_UI_SCALE = 1.6f;

inline float ui_scale(int screenWidth, int screenHeight)
{
    const float horizontalScale = static_cast<float>(screenWidth) / REFERENCE_WINDOW_WIDTH;
    const float verticalScale = static_cast<float>(screenHeight) / REFERENCE_WINDOW_HEIGHT;
    return std::clamp(std::min(horizontalScale, verticalScale), MINIMUM_UI_SCALE, MAXIMUM_UI_SCALE);
}

} // namespace MoongTangE
