#include "Client/Rendering/Scene.h"

#include "Client/Rendering/UiLayout.h"
#include "Common/Protocol/Nickname.h"
#include "Common/SimulationConfig.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <numbers>
#include <raylib.h>
#include <rlgl.h>
#include <stdexcept>

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
constexpr float OPTIONS_DIALOG_WIDTH = 680;
constexpr float OPTIONS_DIALOG_HEIGHT = 640;
constexpr float EXIT_DIALOG_WIDTH = 520;
constexpr float EXIT_DIALOG_HEIGHT = 260;

struct UiCanvas
{
    Camera2D camera;
    float scale;
    int offsetX;
    int offsetY;
};

struct SelectorRequests
{
    bool previous;
    bool next;
};

UiCanvas make_ui_canvas()
{
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float screenHeight = static_cast<float>(GetScreenHeight());
    const float scale = ui_scale(GetScreenWidth(), GetScreenHeight());
    const int offsetX = static_cast<int>((screenWidth - REFERENCE_WINDOW_WIDTH * scale) * 0.5f);
    const int offsetY = static_cast<int>((screenHeight - REFERENCE_WINDOW_HEIGHT * scale) * 0.5f);
    return {{{static_cast<float>(offsetX), static_cast<float>(offsetY)}, {0, 0}, 0, scale},
            scale,
            offsetX,
            offsetY};
}

void begin_ui(const UiCanvas& canvas)
{
    SetMouseOffset(-canvas.offsetX, -canvas.offsetY);
    SetMouseScale(1.0f / canvas.scale, 1.0f / canvas.scale);
    BeginMode2D(canvas.camera);
}

void end_ui()
{
    EndMode2D();
    SetMouseOffset(0, 0);
    SetMouseScale(1, 1);
}

::Vector2 ui_position(::Vector2 screenPosition, const UiCanvas& canvas)
{
    return {(screenPosition.x - static_cast<float>(canvas.offsetX)) / canvas.scale,
            (screenPosition.y - static_cast<float>(canvas.offsetY)) / canvas.scale};
}

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

const char* window_mode_text(WindowMode windowMode)
{
    switch (windowMode) {
    case WindowMode::WINDOWED:
        return "WINDOWED";
    case WindowMode::BORDERLESS:
        return "FULLSCREEN WINDOWED";
    case WindowMode::FULLSCREEN:
        return "FULLSCREEN";
    }
    return "WINDOWED";
}

SelectorRequests draw_setting_selector(const Rectangle& dialog, float rowY, const char* label,
                                       const char* value, bool enabled)
{
    DrawText(label, static_cast<int>(dialog.x) + 30, static_cast<int>(rowY) + 15, 18,
             enabled ? LIGHTGRAY : Color{110, 118, 132, 255});

    const Rectangle previousButton{dialog.x + 190, rowY, 46, 48};
    const Rectangle valueBox{dialog.x + 246, rowY, 348, 48};
    const Rectangle nextButton{dialog.x + 604, rowY, 46, 48};
    const Color valueColor = enabled ? RAYWHITE : Color{110, 118, 132, 255};

    DrawRectangleRounded(valueBox, 0.12f, 6, Color{12, 18, 30, 255});
    DrawRectangleRoundedLines(valueBox, 0.12f, 6, valueColor);
    const int valueSize = MeasureText(value, 18) <= static_cast<int>(valueBox.width) - 20 ? 18 : 16;
    DrawText(value,
             static_cast<int>(valueBox.x) +
                 (static_cast<int>(valueBox.width) - MeasureText(value, valueSize)) / 2,
             static_cast<int>(valueBox.y) + (static_cast<int>(valueBox.height) - valueSize) / 2,
             valueSize, valueColor);

    return {draw_menu_button(previousButton, "<", enabled),
            draw_menu_button(nextButton, ">", enabled)};
}

MainMenuAction draw_options_dialog(float screenWidth, float screenHeight,
                                   const DisplaySettingsState& selectedSettings)
{
    const Rectangle shade{0, 0, screenWidth, screenHeight};
    DrawRectangleRec(shade, Color{0, 0, 0, 150});

    const Rectangle dialog{(screenWidth - OPTIONS_DIALOG_WIDTH) * 0.5f,
                           (screenHeight - OPTIONS_DIALOG_HEIGHT) * 0.5f, OPTIONS_DIALOG_WIDTH,
                           OPTIONS_DIALOG_HEIGHT};
    DrawRectangleRounded(dialog, 0.06f, 8, Color{30, 37, 52, 255});
    DrawText("DISPLAY SETTINGS", static_cast<int>(dialog.x) + 30, static_cast<int>(dialog.y) + 24,
             28, Color{255, 151, 48, 255});

    const auto modeRequests = draw_setting_selector(
        dialog, dialog.y + 76, "DISPLAY MODE", window_mode_text(selectedSettings.windowMode), true);

    const int availableMonitorCount = GetMonitorCount();
    const int monitorCount = std::max(1, availableMonitorCount);
    const int monitor = std::clamp(selectedSettings.monitor, 0, monitorCount - 1);
    const int monitorWidth =
        availableMonitorCount > 0 ? GetMonitorWidth(monitor) : GetScreenWidth();
    const int monitorHeight =
        availableMonitorCount > 0 ? GetMonitorHeight(monitor) : GetScreenHeight();
    const int refreshRate = availableMonitorCount > 0 ? GetMonitorRefreshRate(monitor) : 0;
    const char* monitorText =
        refreshRate > 0 ? TextFormat("DISPLAY %d / %d  |  %d x %d @ %d HZ", monitor + 1,
                                     monitorCount, monitorWidth, monitorHeight, refreshRate)
                        : TextFormat("DISPLAY %d / %d  |  %d x %d", monitor + 1, monitorCount,
                                     monitorWidth, monitorHeight);
    const auto monitorRequests =
        draw_setting_selector(dialog, dialog.y + 158, "MONITOR", monitorText, true);

    const bool resolutionEnabled = selectedSettings.windowMode == WindowMode::WINDOWED;
    const char* resolutionText = resolutionEnabled
                                     ? TextFormat("%d x %d", selectedSettings.resolution.width,
                                                  selectedSettings.resolution.height)
                                     : TextFormat("%d x %d (NATIVE)", monitorWidth, monitorHeight);
    const auto resolutionRequests = draw_setting_selector(dialog, dialog.y + 240, "RESOLUTION",
                                                          resolutionText, resolutionEnabled);

    const auto vSyncRequests = draw_setting_selector(dialog, dialog.y + 322, "V-SYNC",
                                                     selectedSettings.vSync ? "ON" : "OFF", true);
    const char* frameRateLimitText = selectedSettings.frameRateLimit == 0
                                         ? "UNLIMITED"
                                         : TextFormat("%d FPS", selectedSettings.frameRateLimit);
    const auto frameRateRequests =
        draw_setting_selector(dialog, dialog.y + 404, "FPS LIMIT", frameRateLimitText, true);

    const char* modeHint = resolutionEnabled ? "Resolution controls the window size"
                                             : "Fullscreen uses the monitor's native mode";
    DrawText(modeHint,
             static_cast<int>(
                 dialog.x + (dialog.width - static_cast<float>(MeasureText(modeHint, 16))) * 0.5f),
             static_cast<int>(dialog.y) + 480, 16, LIGHTGRAY);
    const char* saveHint = "Applied changes must be confirmed before they are saved";
    DrawText(saveHint,
             static_cast<int>(
                 dialog.x + (dialog.width - static_cast<float>(MeasureText(saveHint, 16))) * 0.5f),
             static_cast<int>(dialog.y) + 506, 16, Color{145, 150, 160, 255});

    const Rectangle applyButton{dialog.x + 120, dialog.y + 555, 200, 54};
    const Rectangle cancelButton{dialog.x + 360, dialog.y + 555, 200, 54};
    const bool applyRequested =
        draw_menu_button(applyButton, "APPLY", true) || IsKeyPressed(KEY_ENTER);
    const bool cancelRequested =
        draw_menu_button(cancelButton, "CANCEL", true) || IsKeyPressed(KEY_ESCAPE);

    if (applyRequested) {
        return MainMenuAction::APPLY_OPTIONS;
    }
    if (cancelRequested) {
        return MainMenuAction::CANCEL_OPTIONS;
    }
    if (modeRequests.previous) {
        return MainMenuAction::PREVIOUS_WINDOW_MODE;
    }
    if (modeRequests.next) {
        return MainMenuAction::NEXT_WINDOW_MODE;
    }
    if (monitorRequests.previous) {
        return MainMenuAction::PREVIOUS_MONITOR;
    }
    if (monitorRequests.next) {
        return MainMenuAction::NEXT_MONITOR;
    }
    if (resolutionRequests.previous) {
        return MainMenuAction::PREVIOUS_RESOLUTION;
    }
    if (resolutionRequests.next) {
        return MainMenuAction::NEXT_RESOLUTION;
    }
    if (vSyncRequests.previous || vSyncRequests.next) {
        return MainMenuAction::TOGGLE_VSYNC;
    }
    if (frameRateRequests.previous) {
        return MainMenuAction::PREVIOUS_FRAME_RATE_LIMIT;
    }
    if (frameRateRequests.next) {
        return MainMenuAction::NEXT_FRAME_RATE_LIMIT;
    }
    return MainMenuAction::NONE;
}

MainMenuAction draw_display_confirmation(float screenWidth, float screenHeight,
                                         int confirmationSeconds)
{
    const Rectangle shade{0, 0, screenWidth, screenHeight};
    DrawRectangleRec(shade, Color{0, 0, 0, 175});

    const Rectangle dialog{(screenWidth - 560) * 0.5f, (screenHeight - 270) * 0.5f, 560, 270};
    DrawRectangleRounded(dialog, 0.08f, 8, Color{30, 37, 52, 255});
    DrawText("KEEP THESE SETTINGS?", static_cast<int>(dialog.x) + 30,
             static_cast<int>(dialog.y) + 28, 26, Color{255, 151, 48, 255});
    DrawText("The previous display settings will be restored in", static_cast<int>(dialog.x) + 30,
             static_cast<int>(dialog.y) + 78, 18, RAYWHITE);
    const char* countdownText =
        TextFormat("%d SECOND%s", confirmationSeconds, confirmationSeconds == 1 ? "" : "S");
    DrawText(countdownText,
             static_cast<int>(dialog.x) +
                 (static_cast<int>(dialog.width) - MeasureText(countdownText, 34)) / 2,
             static_cast<int>(dialog.y) + 112, 34, RAYWHITE);

    const Rectangle keepButton{dialog.x + 55, dialog.y + 185, 210, 54};
    const Rectangle revertButton{dialog.x + 295, dialog.y + 185, 210, 54};
    if (draw_menu_button(keepButton, "KEEP", true) || IsKeyPressed(KEY_ENTER)) {
        return MainMenuAction::CONFIRM_DISPLAY_SETTINGS;
    }
    if (draw_menu_button(revertButton, "REVERT", true) || IsKeyPressed(KEY_ESCAPE)) {
        return MainMenuAction::REVERT_DISPLAY_SETTINGS;
    }
    return MainMenuAction::NONE;
}

GameMenuAction draw_exit_dialog(float screenWidth, float screenHeight)
{
    const Rectangle shade{0, 0, screenWidth, screenHeight};
    DrawRectangleRec(shade, Color{0, 0, 0, 175});

    const Rectangle dialog{(screenWidth - EXIT_DIALOG_WIDTH) * 0.5f,
                           (screenHeight - EXIT_DIALOG_HEIGHT) * 0.5f, EXIT_DIALOG_WIDTH,
                           EXIT_DIALOG_HEIGHT};
    DrawRectangleRounded(dialog, 0.08f, 8, Color{30, 37, 52, 255});
    DrawText("EXIT GAME?", static_cast<int>(dialog.x) + 30, static_cast<int>(dialog.y) + 28, 28,
             Color{255, 151, 48, 255});
    DrawText("The current session will be disconnected safely.", static_cast<int>(dialog.x) + 30,
             static_cast<int>(dialog.y) + 82, 18, RAYWHITE);
    DrawText("Press ESC to keep playing.", static_cast<int>(dialog.x) + 30,
             static_cast<int>(dialog.y) + 112, 16, LIGHTGRAY);

    const Rectangle resumeButton{dialog.x + 45, dialog.y + 170, 195, 54};
    const Rectangle exitButton{dialog.x + 280, dialog.y + 170, 195, 54};
    if (draw_menu_button(resumeButton, "RESUME", true)) {
        return GameMenuAction::RESUME;
    }
    if (draw_menu_button(exitButton, "EXIT GAME", true)) {
        return GameMenuAction::EXIT_GAME;
    }
    return GameMenuAction::NONE;
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
    DrawText("ESC menu  |  Server physics 60 Hz", 28, 114, 16, LIGHTGRAY);

    DrawText(status, 28, 140, 16, LIGHTGRAY);

    DrawFPS(static_cast<int>(REFERENCE_WINDOW_WIDTH) - 100, 20);
}

void draw_player_label(const PlayerState& player, const Camera3D& camera, const UiCanvas& canvas)
{
    if (player.nickname.empty()) {
        return;
    }

    const auto worldPosition =
        render_vector(player.position + Vector3{0, simulation::PLAYER_RADIUS + 0.35, 0});
    const auto screenPosition = ui_position(GetWorldToScreen(worldPosition, camera), canvas);
    const int fontSize = 16;
    const int width = MeasureText(player.nickname.c_str(), fontSize);
    DrawRectangle(static_cast<int>(screenPosition.x) - width / 2 - 5,
                  static_cast<int>(screenPosition.y) - 3, width + 10, fontSize + 6,
                  Color{12, 18, 30, 200});
    DrawText(player.nickname.c_str(), static_cast<int>(screenPosition.x) - width / 2,
             static_cast<int>(screenPosition.y), fontSize, RAYWHITE);
}

} // namespace

class SceneRenderer::CollectibleModels
{
  public:
    struct Asset
    {
        Model model{};
        ::Vector3 center{};
        float normalizedScale{};
    };

    CollectibleModels()
    {
        constexpr std::array<const char*, 4> FILE_NAMES{"apple.glb", "banana.glb", "burger.glb",
                                                        "donut.glb"};
        try {
            for (const auto* fileName : FILE_NAMES) {
                m_assets[m_loadedCount] = Load(fileName);
                ++m_loadedCount;
            }
        } catch (...) {
            Unload();
            throw;
        }
    }

    ~CollectibleModels()
    {
        Unload();
    }

    CollectibleModels(const CollectibleModels&) = delete;
    CollectibleModels& operator=(const CollectibleModels&) = delete;
    CollectibleModels(CollectibleModels&&) = delete;
    CollectibleModels& operator=(CollectibleModels&&) = delete;

    const Asset& For(CollectibleKind kind) const
    {
        if (!valid_collectible_kind(kind)) {
            throw std::invalid_argument("Invalid collectible kind");
        }
        return m_assets[static_cast<std::size_t>(kind) - 1];
    }

  private:
    static Asset Load(const char* fileName)
    {
        const auto path =
            std::filesystem::path(GetApplicationDirectory()) / "Assets" / "Models" / fileName;
        Model model = LoadModel(path.string().c_str());
        if (!IsModelValid(model)) {
            throw std::runtime_error("Could not load collectible model: " + path.string());
        }

        const auto bounds = GetModelBoundingBox(model);
        const float width = bounds.max.x - bounds.min.x;
        const float height = bounds.max.y - bounds.min.y;
        const float depth = bounds.max.z - bounds.min.z;
        const float maximumExtent = std::max({width, height, depth});
        if (!std::isfinite(maximumExtent) || maximumExtent <= 0) {
            UnloadModel(model);
            throw std::runtime_error("Collectible model has invalid bounds: " + path.string());
        }

        return {model,
                {(bounds.min.x + bounds.max.x) * 0.5f, (bounds.min.y + bounds.max.y) * 0.5f,
                 (bounds.min.z + bounds.max.z) * 0.5f},
                static_cast<float>(2 * simulation::COLLECTIBLE_BASE_RADIUS) / maximumExtent};
    }

    void Unload()
    {
        while (m_loadedCount > 0) {
            --m_loadedCount;
            UnloadModel(m_assets[m_loadedCount].model);
        }
    }

    std::array<Asset, 4> m_assets;
    std::size_t m_loadedCount{};
};

SceneRenderer::SceneRenderer()
    : m_ballGeometry(MakeBallGeometry()), m_collectibleModels(std::make_unique<CollectibleModels>())
{
}

SceneRenderer::~SceneRenderer() = default;

MainMenuAction SceneRenderer::DrawMainMenu(bool multiplayerAvailable, bool optionsOpen,
                                           const DisplaySettingsState& selectedSettings,
                                           bool confirmationOpen, int confirmationSeconds,
                                           std::string& nickname,
                                           const std::string& errorMessage) const
{
    const auto uiCanvas = make_ui_canvas();
    const float screenWidth = REFERENCE_WINDOW_WIDTH;
    const float screenHeight = REFERENCE_WINDOW_HEIGHT;
    const float buttonX = (screenWidth - START_BUTTON_WIDTH) * 0.5f;
    const float firstButtonY = screenHeight * 0.52f;
    const Rectangle singleButton{buttonX, firstButtonY, START_BUTTON_WIDTH, START_BUTTON_HEIGHT};
    const Rectangle multiplayerButton{buttonX, firstButtonY + START_BUTTON_HEIGHT + BUTTON_GAP,
                                      START_BUTTON_WIDTH, START_BUTTON_HEIGHT};
    const Rectangle optionsButton{buttonX,
                                  multiplayerButton.y + multiplayerButton.height + BUTTON_GAP,
                                  START_BUTTON_WIDTH, START_BUTTON_HEIGHT};

    BeginDrawing();
    ClearBackground(Color{22, 28, 42, 255});
    begin_ui(uiCanvas);

    const char* title = "MoongTangE";
    const int titleSize = 54;
    DrawText(title, (static_cast<int>(screenWidth) - MeasureText(title, titleSize)) / 2,
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
    bool optionsRequested = false;
    const bool menuEnabled = errorMessage.empty() && !optionsOpen;
    if (menuEnabled) {
        update_nickname(nickname);
        singleRequested = draw_menu_button(singleButton, "SINGLE PLAYER", true);
        multiplayerRequested =
            draw_menu_button(multiplayerButton, "MULTIPLAYER", multiplayerAvailable);
        optionsRequested = draw_menu_button(optionsButton, "OPTIONS", true);
    } else {
        draw_menu_button(singleButton, "SINGLE PLAYER", false);
        draw_menu_button(multiplayerButton, "MULTIPLAYER", false);
        draw_menu_button(optionsButton, "OPTIONS", false);
    }

    const char* hint =
        multiplayerAvailable ? "Choose a game mode" : "Multiplayer requires a server assignment";
    DrawText(hint, (static_cast<int>(screenWidth) - MeasureText(hint, 16)) / 2,
             static_cast<int>(optionsButton.y + optionsButton.height + 18), 16, LIGHTGRAY);

    bool dismissError = false;
    MainMenuAction optionsAction = MainMenuAction::NONE;
    if (optionsOpen) {
        optionsAction =
            confirmationOpen
                ? draw_display_confirmation(screenWidth, screenHeight, confirmationSeconds)
                : draw_options_dialog(screenWidth, screenHeight, selectedSettings);
    } else if (!errorMessage.empty()) {
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
    end_ui();
    EndDrawing();

    if (dismissError) {
        return MainMenuAction::DISMISS_ERROR;
    }
    if (optionsAction != MainMenuAction::NONE) {
        return optionsAction;
    }
    if (singleRequested) {
        return MainMenuAction::SINGLE_PLAYER;
    }
    if (multiplayerRequested) {
        return MainMenuAction::MULTIPLAYER;
    }
    if (optionsRequested) {
        return MainMenuAction::SHOW_OPTIONS;
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

void SceneRenderer::DrawCollectible(const CollectibleState& collectible) const
{
    const auto& asset = m_collectibleModels->For(collectible.kind);
    const float scale = asset.normalizedScale * static_cast<float>(collectible.scale);
    const Vector3 center{asset.center.x, asset.center.y, asset.center.z};
    const auto rotatedCenter = collectible.orientation.Rotate(center * scale);
    const auto modelPosition = render_vector(collectible.position - rotatedCenter);

    const double halfAngle = std::acos(std::clamp(collectible.orientation.w, -1.0, 1.0));
    const double sine = std::sin(halfAngle);
    ::Vector3 rotationAxis{0, 1, 0};
    if (std::abs(sine) > 1e-9) {
        rotationAxis = {static_cast<float>(collectible.orientation.x / sine),
                        static_cast<float>(collectible.orientation.y / sine),
                        static_cast<float>(collectible.orientation.z / sine)};
    }
    const float angleDegrees = static_cast<float>(2 * halfAngle * 180 / std::numbers::pi);

    DrawCylinder({static_cast<float>(collectible.position.x), 0.006f,
                  static_cast<float>(collectible.position.z)},
                 static_cast<float>(simulation::COLLECTIBLE_BASE_RADIUS * collectible.scale),
                 static_cast<float>(simulation::COLLECTIBLE_BASE_RADIUS * collectible.scale),
                 0.002f, 24, Color{28, 37, 46, 180});
    DrawModelEx(asset.model, modelPosition, rotationAxis, angleDegrees, {scale, scale, scale},
                WHITE);
}

GameMenuAction SceneRenderer::Draw(const WorldSnapshot& state, std::uint32_t playerId,
                                   const char* mode, const char* status, bool exitMenuOpen) const
{
    const auto uiCanvas = make_ui_canvas();
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

    for (const auto& collectible : state.collectibles) {
        DrawCollectible(collectible);
    }
    for (const auto& player : state.players) {
        DrawBall(player, player.id == playerId);
    }
    EndMode3D();

    begin_ui(uiCanvas);
    for (const auto& player : state.players) {
        draw_player_label(player, camera, uiCanvas);
    }

    draw_overlay(state.tick, playerId, mode, status);
    const auto gameMenuAction =
        exitMenuOpen ? draw_exit_dialog(REFERENCE_WINDOW_WIDTH, REFERENCE_WINDOW_HEIGHT)
                     : GameMenuAction::NONE;
    end_ui();
    EndDrawing();
    return gameMenuAction;
}

} // namespace MoongTangE
