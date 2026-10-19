#include "Client/Rendering/Scene.h"

#include "Common/Protocol/Nickname.h"
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
constexpr float START_BUTTON_WIDTH = 260;
constexpr float START_BUTTON_HEIGHT = 64;
constexpr float BUTTON_GAP = 18;
constexpr float INPUT_WIDTH = 360;
constexpr float INPUT_HEIGHT = 52;

bool draw_menu_button(Rectangle bounds, const char* label, bool enabled)
{
    const bool hovered = enabled && CheckCollisionPointRec(GetMousePosition(), bounds);
    const Color color = !enabled  ? Color{72, 78, 90, 255}
                        : hovered ? Color{255, 151, 48, 255}
                                  : Color{245, 128, 25, 255};
    const Color textColor = enabled ? RAYWHITE : Color{145, 150, 160, 255};

    DrawRectangleRounded(bounds, 0.18f, 8, color);
    const int labelSize = 24;
    DrawText(label,
             static_cast<int>(bounds.x) +
                 (static_cast<int>(bounds.width) - MeasureText(label, labelSize)) / 2,
             static_cast<int>(bounds.y) + (static_cast<int>(bounds.height) - labelSize) / 2,
             labelSize, textColor);

    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void update_nickname(std::string& nickname)
{
    for (int character = GetCharPressed(); character != 0; character = GetCharPressed()) {
        if (character >= 32 && character <= 126 && nickname.size() < MAX_NICKNAME_LENGTH) {
            nickname.push_back(static_cast<char>(character));
        }
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !nickname.empty()) {
        nickname.pop_back();
    }
}

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

void draw_player_label(const PlayerState& player, const Camera3D& camera)
{
    if (player.nickname.empty()) {
        return;
    }

    const auto worldPosition =
        render_vector(player.position + Vector3{0, simulation::PLAYER_RADIUS + 0.35, 0});
    const auto screenPosition = GetWorldToScreen(worldPosition, camera);
    const int fontSize = 16;
    const int width = MeasureText(player.nickname.c_str(), fontSize);
    DrawRectangle(static_cast<int>(screenPosition.x) - width / 2 - 5,
                  static_cast<int>(screenPosition.y) - 3, width + 10, fontSize + 6,
                  Color{12, 18, 30, 200});
    DrawText(player.nickname.c_str(), static_cast<int>(screenPosition.x) - width / 2,
             static_cast<int>(screenPosition.y), fontSize, RAYWHITE);
}

} // namespace

SceneRenderer::SceneRenderer() : m_ballGeometry(MakeBallGeometry()) {}

MainMenuAction SceneRenderer::DrawMainMenu(bool multiplayerAvailable, std::string& nickname,
                                           const std::string& errorMessage) const
{
    const auto screenWidth = static_cast<float>(GetScreenWidth());
    const auto screenHeight = static_cast<float>(GetScreenHeight());
    const float buttonX = (screenWidth - START_BUTTON_WIDTH) * 0.5f;
    const float firstButtonY = screenHeight * 0.52f;
    const Rectangle singleButton{buttonX, firstButtonY, START_BUTTON_WIDTH, START_BUTTON_HEIGHT};
    const Rectangle multiplayerButton{buttonX, firstButtonY + START_BUTTON_HEIGHT + BUTTON_GAP,
                                      START_BUTTON_WIDTH, START_BUTTON_HEIGHT};

    BeginDrawing();
    ClearBackground(Color{22, 28, 42, 255});

    const char* title = "MoongTangE";
    const int titleSize = 54;
    DrawText(title, (GetScreenWidth() - MeasureText(title, titleSize)) / 2,
             static_cast<int>(screenHeight * 0.2f), titleSize, RAYWHITE);

    const Rectangle nicknameBox{(screenWidth - INPUT_WIDTH) * 0.5f, screenHeight * 0.36f,
                                INPUT_WIDTH, INPUT_HEIGHT};
    DrawText("NICKNAME", static_cast<int>(nicknameBox.x), static_cast<int>(nicknameBox.y) - 25, 18,
             LIGHTGRAY);
    DrawRectangleRounded(nicknameBox, 0.12f, 6, Color{12, 18, 30, 255});
    DrawRectangleRoundedLines(nicknameBox, 0.12f, 6, RAYWHITE);
    const char* nicknameText = nickname.empty() ? "Enter nickname" : nickname.c_str();
    DrawText(nicknameText, static_cast<int>(nicknameBox.x) + 16,
             static_cast<int>(nicknameBox.y) + 15, 22,
             nickname.empty() ? Color{110, 118, 132, 255} : RAYWHITE);

    bool singleRequested = false;
    bool multiplayerRequested = false;
    if (errorMessage.empty()) {
        update_nickname(nickname);
        singleRequested = draw_menu_button(singleButton, "SINGLE PLAYER", true);
        multiplayerRequested =
            draw_menu_button(multiplayerButton, "MULTIPLAYER", multiplayerAvailable);
    } else {
        draw_menu_button(singleButton, "SINGLE PLAYER", false);
        draw_menu_button(multiplayerButton, "MULTIPLAYER", false);
    }

    const char* hint =
        multiplayerAvailable ? "Choose a game mode" : "Multiplayer requires a server assignment";
    DrawText(hint, (GetScreenWidth() - MeasureText(hint, 16)) / 2,
             static_cast<int>(multiplayerButton.y + multiplayerButton.height + 18), 16, LIGHTGRAY);

    bool dismissError = false;
    if (!errorMessage.empty()) {
        const Rectangle shade{0, 0, screenWidth, screenHeight};
        DrawRectangleRec(shade, Color{0, 0, 0, 150});
        const Rectangle dialog{(screenWidth - 500) * 0.5f, (screenHeight - 210) * 0.5f, 500, 210};
        DrawRectangleRounded(dialog, 0.08f, 8, Color{30, 37, 52, 255});
        DrawText("COULD NOT START GAME", static_cast<int>(dialog.x) + 28,
                 static_cast<int>(dialog.y) + 28, 24, Color{255, 151, 48, 255});
        DrawText(errorMessage.data(), static_cast<int>(dialog.x) + 28,
                 static_cast<int>(dialog.y) + 76, 18, RAYWHITE);
        const Rectangle okButton{dialog.x + 170, dialog.y + 138, 160, 46};
        dismissError = draw_menu_button(okButton, "OK", true) || IsKeyPressed(KEY_ENTER) ||
                       IsKeyPressed(KEY_ESCAPE);
    }
    EndDrawing();

    if (dismissError) {
        return MainMenuAction::DISMISS_ERROR;
    }
    if (singleRequested) {
        return MainMenuAction::SINGLE_PLAYER;
    }
    if (multiplayerRequested) {
        return MainMenuAction::MULTIPLAYER;
    }
    return MainMenuAction::NONE;
}

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

    for (const auto& player : state.players) {
        draw_player_label(player, camera);
    }

    draw_overlay(state.tick, playerId, mode, status);
    EndDrawing();
}

} // namespace MoongTangE
