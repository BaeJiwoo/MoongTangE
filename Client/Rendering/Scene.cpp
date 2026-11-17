#include "Client/Rendering/Scene.h"

#include "Client/Rendering/ChomperGeometry.h"
#include "Client/Rendering/PlayerRanking.h"
#include "Client/Rendering/UiLayout.h"
#include "Common/Physics/CompoundBallCollider.h"
#include "Common/Physics/PhysicsWorld.h"
#include "Common/Physics/Terrain.h"
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
constexpr float START_BUTTON_WIDTH = 260;
constexpr float START_BUTTON_HEIGHT = 64;
constexpr float BUTTON_GAP = 18;
constexpr float INPUT_WIDTH = 360;
constexpr float INPUT_HEIGHT = 52;
constexpr float OPTIONS_DIALOG_WIDTH = 680;
constexpr float OPTIONS_DIALOG_HEIGHT = 640;
constexpr float GAME_MENU_WIDTH = 520;
constexpr float GAME_MENU_HEIGHT = 390;

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

MenuAction draw_options_dialog(float screenWidth, float screenHeight,
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
        return MenuAction::APPLY_OPTIONS;
    }
    if (cancelRequested) {
        return MenuAction::CANCEL_OPTIONS;
    }
    if (modeRequests.previous) {
        return MenuAction::PREVIOUS_WINDOW_MODE;
    }
    if (modeRequests.next) {
        return MenuAction::NEXT_WINDOW_MODE;
    }
    if (monitorRequests.previous) {
        return MenuAction::PREVIOUS_MONITOR;
    }
    if (monitorRequests.next) {
        return MenuAction::NEXT_MONITOR;
    }
    if (resolutionRequests.previous) {
        return MenuAction::PREVIOUS_RESOLUTION;
    }
    if (resolutionRequests.next) {
        return MenuAction::NEXT_RESOLUTION;
    }
    if (vSyncRequests.previous || vSyncRequests.next) {
        return MenuAction::TOGGLE_VSYNC;
    }
    if (frameRateRequests.previous) {
        return MenuAction::PREVIOUS_FRAME_RATE_LIMIT;
    }
    if (frameRateRequests.next) {
        return MenuAction::NEXT_FRAME_RATE_LIMIT;
    }
    return MenuAction::NONE;
}

MenuAction draw_display_confirmation(float screenWidth, float screenHeight, int confirmationSeconds)
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
        return MenuAction::CONFIRM_DISPLAY_SETTINGS;
    }
    if (draw_menu_button(revertButton, "REVERT", true) || IsKeyPressed(KEY_ESCAPE)) {
        return MenuAction::REVERT_DISPLAY_SETTINGS;
    }
    return MenuAction::NONE;
}

MenuAction draw_game_menu(float screenWidth, float screenHeight, bool optionsOpen,
                          const DisplaySettingsState& selectedSettings, bool confirmationOpen,
                          int confirmationSeconds)
{
    if (optionsOpen) {
        return confirmationOpen
                   ? draw_display_confirmation(screenWidth, screenHeight, confirmationSeconds)
                   : draw_options_dialog(screenWidth, screenHeight, selectedSettings);
    }

    const Rectangle shade{0, 0, screenWidth, screenHeight};
    DrawRectangleRec(shade, Color{0, 0, 0, 175});

    const Rectangle dialog{(screenWidth - GAME_MENU_WIDTH) * 0.5f,
                           (screenHeight - GAME_MENU_HEIGHT) * 0.5f, GAME_MENU_WIDTH,
                           GAME_MENU_HEIGHT};
    DrawRectangleRounded(dialog, 0.08f, 8, Color{30, 37, 52, 255});
    DrawText("GAME MENU", static_cast<int>(dialog.x) + 30, static_cast<int>(dialog.y) + 28, 28,
             Color{255, 151, 48, 255});
    DrawText("The server keeps running while this menu is open.", static_cast<int>(dialog.x) + 30,
             static_cast<int>(dialog.y) + 76, 17, RAYWHITE);
    DrawText("Movement input is neutral. Press ESC to resume.", static_cast<int>(dialog.x) + 30,
             static_cast<int>(dialog.y) + 106, 16, LIGHTGRAY);

    const Rectangle resumeButton{dialog.x + 80, dialog.y + 150, 360, 54};
    const Rectangle optionsButton{dialog.x + 80, dialog.y + 222, 360, 54};
    const Rectangle exitButton{dialog.x + 80, dialog.y + 294, 360, 54};
    if (draw_menu_button(resumeButton, "RESUME GAME", true)) {
        return MenuAction::RESUME_GAME;
    }
    if (draw_menu_button(optionsButton, "OPTIONS", true)) {
        return MenuAction::SHOW_OPTIONS;
    }
    if (draw_menu_button(exitButton, "EXIT GAME", true)) {
        return MenuAction::EXIT_GAME;
    }
    return MenuAction::NONE;
}

MenuAction draw_fail_screen(float screenWidth, float screenHeight, bool restartPending)
{
    DrawRectangleRec({0, 0, screenWidth, screenHeight}, Color{0, 0, 0, 185});
    const Rectangle dialog{(screenWidth - 560) * 0.5f, (screenHeight - 350) * 0.5f, 560, 350};
    DrawRectangleRounded(dialog, 0.08f, 8, Color{30, 37, 52, 255});
    DrawText("FAIL", static_cast<int>(dialog.x) + 30, static_cast<int>(dialog.y) + 25, 48,
             Color{255, 110, 90, 255});
    DrawText("A larger chomper ate you.", static_cast<int>(dialog.x) + 30,
             static_cast<int>(dialog.y) + 95, 22, RAYWHITE);
    DrawText(restartPending ? "Finding a safe place to restart..."
                            : "Score kept. The round timer keeps running.",
             static_cast<int>(dialog.x) + 30, static_cast<int>(dialog.y) + 132, 18, LIGHTGRAY);

    const Rectangle restartButton{dialog.x + 70, dialog.y + 184, 420, 54};
    const Rectangle menuButton{dialog.x + 70, dialog.y + 260, 420, 54};
    if (draw_menu_button(restartButton, "PLAY AGAIN  [ENTER / R]", !restartPending) ||
        (!restartPending && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_R)))) {
        return MenuAction::RESTART_GAME;
    }
    if (draw_menu_button(menuButton, "MAIN MENU  [ESC]", true) || IsKeyPressed(KEY_ESCAPE)) {
        return MenuAction::MAIN_MENU;
    }
    return MenuAction::NONE;
}

std::string leaderboard_name(const PlayerState& player)
{
    auto name = player.nickname.empty() ? std::string("Player") : player.nickname;
    if (MeasureText(name.c_str(), 16) > 132) {
        do {
            name.pop_back();
        } while (!name.empty() && MeasureText((name + "...").c_str(), 16) > 132);
        name += "...";
    }
    return name;
}

void draw_collection_radar(const WorldSnapshot& state, const PlayerState& own)
{
    constexpr float CENTER_X = 980;
    constexpr float CENTER_Y = 585;
    constexpr float RADIUS = 85;
    constexpr double RANGE = 50;
    DrawRectangleRounded({872, 470, 214, 236}, 0.08f, 8, Color{12, 18, 30, 225});
    DrawText("COLLECTION RADAR", 890, 486, 16, RAYWHITE);
    DrawCircleLines(static_cast<int>(CENTER_X), static_cast<int>(CENTER_Y), RADIUS,
                    Color{62, 78, 94, 255});
    DrawLine(895, 585, 1065, 585, Color{42, 55, 71, 255});
    DrawLine(980, 500, 980, 670, Color{42, 55, 71, 255});
    DrawText("W", 975, 503, 13, LIGHTGRAY);
    DrawText("A", 898, 578, 13, LIGHTGRAY);
    DrawText("D", 1054, 578, 13, LIGHTGRAY);
    DrawText("S", 975, 656, 13, LIGHTGRAY);
    auto point = [&](Vector3 position) {
        auto relative = position - own.position;
        relative.y = 0;
        const double distance = relative.Length();
        if (distance > RANGE) {
            relative = relative * (RANGE / distance);
        }
        return ::Vector2{CENTER_X + static_cast<float>(relative.x / RANGE) * RADIUS,
                         CENTER_Y + static_cast<float>(relative.z / RANGE) * RADIUS};
    };
    for (const auto& collectible : state.collectibles) {
        if (collectible.attachedPlayerId == 0) {
            DrawCircleV(point(collectible.position), 3.5f, GOLD);
        }
    }
    for (const auto& mine : state.mines) {
        DrawCircleV(point(mine.position), 4, mine.readyTick <= state.tick ? LIME : GRAY);
    }
    for (const auto& player : state.players) {
        if (player.id != own.id && player.deathTick == 0) {
            DrawCircleV(point(player.position), 5, Color{255, 110, 90, 255});
        }
    }
    DrawCircleV({CENTER_X, CENTER_Y}, 5, SKYBLUE);
    DrawText("Gold: food  Green: mines", 890, 682, 14, LIGHTGRAY);
}

void draw_survival_hud(const WorldSnapshot& state, std::uint32_t playerId)
{
    const auto own = std::ranges::find(state.players, playerId, &PlayerState::id);
    if (own == state.players.end()) {
        return;
    }
    DrawRectangleRounded({14, 14, 480, 158}, 0.08f, 8, Color{12, 18, 30, 230});
    DrawText("MOONGTANGE  /  FREE PLAY", 30, 28, 20, RAYWHITE);
    DrawText("Eat. Grow. Survive. No time limit.", 30, 60, 18, LIGHTGRAY);
    DrawText(TextFormat("MASS %.1f", player_mass(*own)), 30, 91, 30, GOLD);
    DrawText(TextFormat("SIZE %.2f m", own->radius * 2), 280, 100, 19, RAYWHITE);
    DrawText("WASD move   |   ESC menu   |   F3 debug", 30, 142, 15, LIGHTGRAY);
    DrawRectangleRounded({14, 180, 480, 54}, 0.1f, 8, Color{12, 18, 30, 220});
    const bool recentlyHit =
        own->mineHitTick != 0 && state.tick - own->mineHitTick < simulation::MINE_IMMUNITY_TICKS;
    DrawText(recentlyHit ? "MINE HIT!  Lost 40% mass. Keep eating!"
                         : (own->radius >= simulation::MINE_TRIGGER_RADIUS
                                ? "CAUTION: green mines shrink you!"
                                : "Small enough to pass safely through green mines."),
             28, 198, 16, recentlyHit ? Color{255, 126, 97, 255} : Color{157, 230, 156, 255});

    const auto players = ranked_players(state);
    DrawRectangleRounded({862, 14, 224, 72}, 0.08f, 8, Color{12, 18, 30, 230});
    DrawText("MASS RANKING", 880, 28, 20, RAYWHITE);
    DrawText(TextFormat("%u alive", static_cast<unsigned>(players.size())), 880, 58, 16, LIGHTGRAY);
    int row = 0;
    for (const auto* player : players) {
        if (row == 3) {
            break;
        }
        DrawRectangle(862, 98 + row * 34, 224, 32, Color{12, 18, 30, 210});
        DrawText(leaderboard_name(*player).c_str(), 876, 106 + row * 34, 16,
                 player->id == playerId ? SKYBLUE : LIGHTGRAY);
        DrawText(TextFormat("%.1f", player_mass(*player)), 1022, 106 + row * 34, 16, GOLD);
        ++row;
    }
    draw_collection_radar(state, *own);
}

::Vector3 render_vector(Vector3 value)
{
    return {static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z)};
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

void draw_player_label(const PlayerState& player, const Camera3D& camera, const UiCanvas& canvas)
{
    if (player.nickname.empty()) {
        return;
    }

    const auto worldPosition = render_vector(player.position + Vector3{0, player.radius + 0.35, 0});
    const auto screenPosition = ui_position(GetWorldToScreen(worldPosition, camera), canvas);
    const int fontSize = 16;
    const int width = MeasureText(player.nickname.c_str(), fontSize);
    DrawRectangle(static_cast<int>(screenPosition.x) - width / 2 - 5,
                  static_cast<int>(screenPosition.y) - 3, width + 10, fontSize + 6,
                  Color{12, 18, 30, 200});
    DrawText(player.nickname.c_str(), static_cast<int>(screenPosition.x) - width / 2,
             static_cast<int>(screenPosition.y), fontSize, RAYWHITE);
}

void draw_terrain()
{
    constexpr float EXTENT = static_cast<float>(simulation::MAP_HALF_EXTENT);
    DrawPlane({0, -0.01f, 0}, {2 * EXTENT, 2 * EXTENT}, Color{49, 78, 75, 255});
    for (int line = -32; line <= 32; line += 2) {
        const float offset = static_cast<float>(line);
        DrawLine3D({offset, 0, -EXTENT}, {offset, 0, EXTENT}, Color{64, 96, 88, 255});
        DrawLine3D({-EXTENT, 0, offset}, {EXTENT, 0, offset}, Color{64, 96, 88, 255});
    }
    DrawCubeWires({0, 0.2f, 0}, EXTENT * 2, 0.4f, EXTENT * 2, Color{145, 208, 159, 255});
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

SceneRenderer::SceneRenderer() : m_collectibleModels(std::make_unique<CollectibleModels>()) {}

SceneRenderer::~SceneRenderer() = default;

bool SceneRenderer::DrawConnecting(const char* status, bool cancelling) const
{
    const auto canvas = make_ui_canvas();
    BeginDrawing();
    ClearBackground(Color{22, 28, 42, 255});
    begin_ui(canvas);
    const int centerX = static_cast<int>(REFERENCE_WINDOW_WIDTH / 2);
    const int centerY = static_cast<int>(REFERENCE_WINDOW_HEIGHT / 2);
    DrawText("MULTIPLAYER", centerX - MeasureText("MULTIPLAYER", 36) / 2, centerY - 100, 36,
             RAYWHITE);
    DrawText(status, centerX - MeasureText(status, 22) / 2, centerY - 20, 22, LIGHTGRAY);
    const Rectangle cancelButton{static_cast<float>(centerX) - 100,
                                 static_cast<float>(centerY) + 50, 200, 50};
    const bool cancelled = draw_menu_button(cancelButton, "CANCEL", !cancelling);
    end_ui();
    EndDrawing();
    return cancelled;
}

MenuAction SceneRenderer::DrawMainMenu(bool multiplayerAvailable, bool optionsOpen,
                                       const DisplaySettingsState& selectedSettings,
                                       bool confirmationOpen, int confirmationSeconds,
                                       std::string& nickname, const std::string& errorMessage) const
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
        multiplayerAvailable ? "Choose a game mode" : "Multiplayer is not configured";
    DrawText(hint, (static_cast<int>(screenWidth) - MeasureText(hint, 16)) / 2,
             static_cast<int>(optionsButton.y + optionsButton.height + 18), 16, LIGHTGRAY);

    bool dismissError = false;
    MenuAction optionsAction = MenuAction::NONE;
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
        return MenuAction::DISMISS_ERROR;
    }
    if (optionsAction != MenuAction::NONE) {
        return optionsAction;
    }
    if (singleRequested) {
        return MenuAction::SINGLE_PLAYER;
    }
    if (multiplayerRequested) {
        return MenuAction::MULTIPLAYER;
    }
    if (optionsRequested) {
        return MenuAction::SHOW_OPTIONS;
    }
    return MenuAction::NONE;
}

void SceneRenderer::DrawBall(const PlayerState& player, bool own) const
{
    const auto radius = static_cast<float>(player.radius);
    DrawCylinder(
        {static_cast<float>(player.position.x), 0.008f, static_cast<float>(player.position.z)},
        radius * 0.95f, radius * 0.95f, 0.002f, 32, Color{20, 42, 40, 180});
    const double opening = mouth_angle(GetTime() + player.id * 0.13);
    const Color skin = own ? Color{255, 209, 46, 255} : Color{100, 204, 255, 255};
    const auto light = Vector3{-0.4, 0.8, -0.5}.Normalized();
    auto vertex = [&](Vector3 local, Color color, bool shaded) {
        const auto normal = player.orientation.Rotate(local);
        const double shade = shaded ? 0.55 + 0.45 * std::max(0.0, dot(normal, light)) : 1;
        const auto point = render_vector(player.position + normal * player.radius);
        rlColor4ub(static_cast<unsigned char>(color.r * shade),
                   static_cast<unsigned char>(color.g * shade),
                   static_cast<unsigned char>(color.b * shade), 255);
        rlVertex3f(point.x, point.y, point.z);
    };
    rlDisableBackfaceCulling();
    rlBegin(RL_TRIANGLES);
    for (int ring = 0; ring < BALL_RINGS; ++ring) {
        for (int sector = 0; sector < BALL_SECTORS; ++sector) {
            const auto a = chomper_surface(static_cast<double>(ring) / BALL_RINGS,
                                           static_cast<double>(sector) / BALL_SECTORS, opening);
            const auto b = chomper_surface(static_cast<double>(ring + 1) / BALL_RINGS,
                                           static_cast<double>(sector) / BALL_SECTORS, opening);
            const auto c = chomper_surface(static_cast<double>(ring) / BALL_RINGS,
                                           static_cast<double>(sector + 1) / BALL_SECTORS, opening);
            const auto d = chomper_surface(static_cast<double>(ring + 1) / BALL_RINGS,
                                           static_cast<double>(sector + 1) / BALL_SECTORS, opening);
            for (const auto point : {a, b, c, b, d, c}) {
                vertex(point, skin, true);
            }
        }
        for (const double edge : {0.0, 1.0}) {
            vertex({}, Color{92, 34, 43, 255}, false);
            vertex(chomper_surface(static_cast<double>(ring) / BALL_RINGS, edge, opening),
                   Color{133, 42, 53, 255}, false);
            vertex(chomper_surface(static_cast<double>(ring + 1) / BALL_RINGS, edge, opening),
                   Color{133, 42, 53, 255}, false);
        }
    }
    rlEnd();
    rlEnableBackfaceCulling();
    for (const double side : {-1.0, 1.0}) {
        const auto eye = player.position + player.orientation.Rotate(
                                               Vector3{side * 0.4, 0.65, -0.57} * player.radius);
        DrawSphereEx(render_vector(eye), radius * 0.17f, 10, 12, RAYWHITE);
        const auto pupil = eye + player.orientation.Rotate(Vector3{0, 0.02, -0.13} * player.radius);
        DrawSphereEx(render_vector(pupil), radius * 0.085f, 8, 10, Color{24, 33, 47, 255});
    }
}

void SceneRenderer::DrawMine(const MineState& mine, std::uint64_t tick) const
{
    const auto center = render_vector(mine.position);
    const bool ready = mine.readyTick <= tick;
    const float radius = static_cast<float>(simulation::MINE_RADIUS);
    const Color color = ready ? Color{125, 236, 109, 255} : Color{78, 100, 97, 255};
    DrawSphereEx(center, radius * 0.78f, 12, 16, color);
    for (int index = 0; index < 12; ++index) {
        const double angle = static_cast<double>(index) * 2 * std::numbers::pi / 12;
        const Vector3 direction{std::cos(angle), index % 2 == 0 ? 0.45 : -0.1, std::sin(angle)};
        DrawCylinderEx(render_vector(mine.position + direction * 0.35),
                       render_vector(mine.position + direction * (ready ? 0.9 : 0.55)), 0.17f, 0, 6,
                       color);
    }
    DrawSphereEx({center.x, center.y + radius * 0.85f, center.z}, radius * 0.23f, 8, 10,
                 ready ? Color{244, 83, 91, 255} : DARKGRAY);
    DrawCircle3D({center.x, 0.02f, center.z}, radius + 0.35f, {1, 0, 0}, 90,
                 ready ? Color{171, 238, 130, 255} : Color{75, 104, 94, 255});
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

void SceneRenderer::DrawPhysicsDebug(const WorldSnapshot& state, const PlayerState& player) const
{
    (void)state;
    DrawSphereWires(render_vector(player.position), static_cast<float>(player.radius), 12, 16,
                    Color{80, 220, 255, 210});
}

MenuAction SceneRenderer::Draw(const WorldSnapshot& state, std::uint32_t playerId, const char* mode,
                               const char* status, bool gameMenuOpen, bool optionsOpen,
                               const DisplaySettingsState& selectedSettings, bool confirmationOpen,
                               int confirmationSeconds, bool restartPending) const
{
    if (IsKeyPressed(KEY_F3)) {
        m_showPhysicsDebug = !m_showPhysicsDebug;
    }

    const auto uiCanvas = make_ui_canvas();
    const auto target = camera_target(state, playerId);
    const auto own = std::ranges::find(state.players, playerId, &PlayerState::id);
    const float cameraScale =
        own == state.players.end()
            ? 1.0f
            : static_cast<float>(1 + (own->radius - simulation::PLAYER_RADIUS) * 0.65);
    const Camera3D camera{{target.x, target.y + 10 * cameraScale, target.z + 9 * cameraScale},
                          target,
                          {0, 1, 0},
                          45,
                          CAMERA_PERSPECTIVE};

    BeginDrawing();
    ClearBackground(Color{22, 28, 42, 255});

    BeginMode3D(camera);
    draw_terrain();

    for (const auto& mine : state.mines) {
        DrawMine(mine, state.tick);
    }
    for (const auto& collectible : state.collectibles) {
        DrawCollectible(collectible);
    }
    for (const auto& player : state.players) {
        if (player.deathTick != 0) {
            continue;
        }
        DrawBall(player, player.id == playerId);
        if (m_showPhysicsDebug) {
            DrawPhysicsDebug(state, player);
        }
    }
    EndMode3D();

    begin_ui(uiCanvas);
    for (const auto& player : state.players) {
        if (player.deathTick == 0 && player.id != playerId) {
            draw_player_label(player, camera, uiCanvas);
        }
    }

    draw_survival_hud(state, playerId);
    if (m_showPhysicsDebug) {
        DrawText(TextFormat("%s | %s | Tick %llu | collision spheres", mode, status,
                            static_cast<unsigned long long>(state.tick)),
                 28, 250, 16, Color{80, 220, 255, 255});
    }
    const bool defeated =
        std::any_of(state.players.begin(), state.players.end(), [playerId](const auto& player) {
            return player.id == playerId && player.deathTick != 0;
        });
    const auto gameMenuAction =
        defeated ? draw_fail_screen(REFERENCE_WINDOW_WIDTH, REFERENCE_WINDOW_HEIGHT, restartPending)
        : gameMenuOpen
            ? draw_game_menu(REFERENCE_WINDOW_WIDTH, REFERENCE_WINDOW_HEIGHT, optionsOpen,
                             selectedSettings, confirmationOpen, confirmationSeconds)
            : MenuAction::NONE;
    end_ui();
    EndDrawing();
    return gameMenuAction;
}

} // namespace MoongTangE
