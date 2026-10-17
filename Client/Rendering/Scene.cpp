#include "Client/Rendering/Scene.h"
#include "Common/SimulationConfig.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <raylib.h>
#include <rlgl.h>

namespace MoongTangE {
namespace {
constexpr int ballRings = 20;
constexpr int ballSectors = 32;
constexpr float ballRadius = static_cast<float>(simulation::PLAYER_RADIUS);

::Vector3 renderVector(Vector3 value) {
    return {static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z)};
}

Vector3 spherePoint(int ring, int sector) {
    const double latitude = std::numbers::pi * ring / ballRings;
    const double longitude = 2 * std::numbers::pi * sector / ballSectors;
    return {std::sin(latitude) * std::cos(longitude), std::cos(latitude),
            std::sin(latitude) * std::sin(longitude)};
}

::Vector3 cameraTarget(const WorldSnapshot& state, std::uint32_t playerId) {
    for (const auto& player : state.players) {
        if (player.id == playerId) {
            return renderVector(player.position);
        }
    }
    return {};
}

void drawOverlay(std::uint64_t tick, std::uint32_t playerId, const char* mode, const char* status) {
    DrawRectangle(14, 14, 510, 158, Color{12, 18, 30, 220});
    DrawText("MoongTangE", 28, 28, 22, RAYWHITE);
    DrawText(TextFormat("%s | Player %u | Tick %llu", mode, playerId,
                        static_cast<unsigned long long>(tick)),
             28, 62, 18, SKYBLUE);
    DrawText("WASD push  |  Release to coast  |  Opposite key to brake", 28, 90, 16, LIGHTGRAY);
    DrawText("ESC quit  |  Server physics 60 Hz", 28, 114, 16, LIGHTGRAY);
    DrawText(status, 28, 140, 16, LIGHTGRAY);
    DrawFPS(GetScreenWidth() - 100, 20);
}
} // namespace

SceneRenderer::SceneRenderer() : ballGeometry_(makeBallGeometry()) {}

std::vector<SceneRenderer::BallTriangle> SceneRenderer::makeBallGeometry() {
    std::vector<BallTriangle> triangles;
    triangles.reserve(ballRings * ballSectors * 2);
    for (int ring = 0; ring < ballRings; ++ring) {
        for (int sector = 0; sector < ballSectors; ++sector) {
            const auto topLeft = spherePoint(ring, sector);
            const auto bottomLeft = spherePoint(ring + 1, sector);
            const auto topRight = spherePoint(ring, sector + 1);
            const auto bottomRight = spherePoint(ring + 1, sector + 1);
            // White equator, alternating panels and colored caps reveal every rolling direction.
            const bool equator = ring == ballRings / 2 - 1 || ring == ballRings / 2;
            const bool panel = ring > 1 && ring < ballRings - 2 && (sector / 4 + ring / 5) % 2 == 0;
            const bool light = equator || panel;
            triangles.push_back({{topLeft, topRight, bottomLeft}, light});
            triangles.push_back({{bottomLeft, topRight, bottomRight}, light});
        }
    }
    return triangles;
}

void SceneRenderer::drawBall(const PlayerState& player, bool own) const {
    DrawCylinder(
        {static_cast<float>(player.position.x), 0.008f, static_cast<float>(player.position.z)},
        0.48f, 0.48f, 0.002f, 32, Color{28, 37, 46, 255});
    const auto lightDirection = Vector3{-0.4, 0.8, 0.5}.normalized();
    rlBegin(RL_TRIANGLES);
    for (const auto& triangle : ballGeometry_) {
        const Color color = triangle.light
                                ? Color{245, 241, 218, 255}
                                : (own ? Color{245, 128, 25, 255} : Color{32, 125, 225, 255});
        for (const auto vertex : triangle.vertices) {
            const auto normal = player.orientation.rotate(vertex);
            const double shade = 0.4 + 0.6 * std::max(0.0, dot(normal, lightDirection));
            const auto world = renderVector(player.position + normal * ballRadius);
            rlColor4ub(static_cast<unsigned char>(color.r * shade),
                       static_cast<unsigned char>(color.g * shade),
                       static_cast<unsigned char>(color.b * shade), 255);
            rlVertex3f(world.x, world.y, world.z);
        }
    }
    rlEnd();
}

void SceneRenderer::draw(const WorldSnapshot& state, std::uint32_t playerId, const char* mode,
                         const char* status) const {
    const auto target = cameraTarget(state, playerId);
    const Camera3D camera{{target.x + 4, target.y + 3.5f, target.z + 5.5f},
                          target,
                          {0, 1, 0},
                          45,
                          CAMERA_PERSPECTIVE};
    BeginDrawing();
    ClearBackground(Color{22, 28, 42, 255});
    BeginMode3D(camera);
    DrawPlane({target.x, 0, target.z}, {100, 100}, Color{50, 65, 80, 255});
    DrawGrid(40, 1);
    for (const auto& player : state.players) {
        drawBall(player, player.id == playerId);
    }
    EndMode3D();
    drawOverlay(state.tick, playerId, mode, status);
    EndDrawing();
}
} // namespace MoongTangE
