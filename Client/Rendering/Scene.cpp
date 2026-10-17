#include "Client/Rendering/Scene.h"

#include "Common/SimulationConfig.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <raylib.h>
#include <rlgl.h>

namespace MoongTangE {
namespace {
constexpr int BALL_RINGS = 20;
constexpr int BALL_SECTORS = 32;
constexpr float BALL_RADIUS = static_cast<float>(simulation::PLAYER_RADIUS);

::Vector3 render_vector(Vector3 value)
{
    return {static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z)};
}

Vector3 sphere_point(int ring, int sector)
{
    const double latitude = std::numbers::pi * ring / BALL_RINGS;
    const double longitude = 2 * std::numbers::pi * sector / BALL_SECTORS;
    return {std::sin(latitude) * std::cos(longitude), std::cos(latitude),
            std::sin(latitude) * std::sin(longitude)};
}

::Vector3 camera_target(const WorldSnapshot& state, std::uint32_t playerId)
{
    for (const auto& player : state.players) {
        if (player.id == playerId) {
            return render_vector(player.position);
        }
    }
    return {};
}

void draw_overlay(std::uint64_t tick, std::uint32_t playerId, const char* mode, const char* status)
{
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

SceneRenderer::SceneRenderer() : m_ballGeometry(MakeBallGeometry()) {}

std::vector<SceneRenderer::BallTriangle> SceneRenderer::MakeBallGeometry()
{
    std::vector<BallTriangle> triangles;
    triangles.reserve(BALL_RINGS * BALL_SECTORS * 2);
    for (int ring = 0; ring < BALL_RINGS; ++ring) {
        for (int sector = 0; sector < BALL_SECTORS; ++sector) {
            const auto topLeft = sphere_point(ring, sector);
            const auto bottomLeft = sphere_point(ring + 1, sector);
            const auto topRight = sphere_point(ring, sector + 1);
            const auto bottomRight = sphere_point(ring + 1, sector + 1);
            // White equator, alternating panels and colored caps reveal every rolling direction.
            const bool equator = ring == BALL_RINGS / 2 - 1 || ring == BALL_RINGS / 2;
            const bool panel =
                ring > 1 && ring < BALL_RINGS - 2 && (sector / 4 + ring / 5) % 2 == 0;
            const bool light = equator || panel;
            triangles.push_back({{topLeft, topRight, bottomLeft}, light});
            triangles.push_back({{bottomLeft, topRight, bottomRight}, light});
        }
    }
    return triangles;
}

void SceneRenderer::DrawBall(const PlayerState& player, bool own) const
{
    DrawCylinder(
        {static_cast<float>(player.position.x), 0.008f, static_cast<float>(player.position.z)},
        0.48f, 0.48f, 0.002f, 32, Color{28, 37, 46, 255});
    const auto lightDirection = Vector3{-0.4, 0.8, 0.5}.Normalized();
    rlBegin(RL_TRIANGLES);
    for (const auto& triangle : m_ballGeometry) {
        const Color color = triangle.light
                                ? Color{245, 241, 218, 255}
                                : (own ? Color{245, 128, 25, 255} : Color{32, 125, 225, 255});
        for (const auto vertex : triangle.vertices) {
            const auto normal = player.orientation.Rotate(vertex);
            const double shade = 0.4 + 0.6 * std::max(0.0, dot(normal, lightDirection));
            const auto world = render_vector(player.position + normal * BALL_RADIUS);
            rlColor4ub(static_cast<unsigned char>(color.r * shade),
                       static_cast<unsigned char>(color.g * shade),
                       static_cast<unsigned char>(color.b * shade), 255);
            rlVertex3f(world.x, world.y, world.z);
        }
    }
    rlEnd();
}

void SceneRenderer::Draw(const WorldSnapshot& state, std::uint32_t playerId, const char* mode,
                         const char* status) const
{
    const auto target = camera_target(state, playerId);
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
        DrawBall(player, player.id == playerId);
    }
    EndMode3D();
    draw_overlay(state.tick, playerId, mode, status);
    EndDrawing();
}
} // namespace MoongTangE
