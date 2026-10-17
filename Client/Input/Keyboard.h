#pragma once
#include "Common/Protocol/InputCommand.h"
#include <raylib.h>

namespace sandbox {
inline InputCommand keyboardInput(std::uint32_t id, std::uint64_t sequence) {
    return {id, sequence, static_cast<double>(IsKeyDown(KEY_D) - IsKeyDown(KEY_A)),
            static_cast<double>(IsKeyDown(KEY_S) - IsKeyDown(KEY_W))};
}
} // namespace sandbox
