#include "Client/Rendering/DisplaySettings.h"
#include "Client/Rendering/DisplaySettingsStore.h"
#include "Client/Rendering/SnapshotInterpolator.h"
#include "Client/Rendering/UiLayout.h"
#include "TestSupport.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <numbers>

int main()
{
    return MoongTangE::test::run([] {
        using namespace MoongTangE;

        constexpr Resolution DEFAULT_RESOLUTION{1280, 800};
        constexpr Resolution PREVIOUS_RESOLUTION{1280, 720};
        constexpr Resolution NEXT_RESOLUTION{1600, 900};

        MOONGTANGE_CHECK(ui_scale(1100, 720) == 1.0f);
        MOONGTANGE_CHECK(ui_scale(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT) > 1.0f);
        MOONGTANGE_CHECK(ui_scale(3840, 2160) == 3.0f);
        MOONGTANGE_CHECK(ui_scale(0, 0) == 0.01f);

        DisplaySettings settings;
        MOONGTANGE_CHECK(settings.Active().resolution == DEFAULT_RESOLUTION);
        MOONGTANGE_CHECK(settings.Pending() == settings.Active());
        MOONGTANGE_CHECK(settings.Confirmed() == settings.Active());
        MOONGTANGE_CHECK(settings.Active().windowMode == WindowMode::WINDOWED);
        MOONGTANGE_CHECK(settings.Active().monitor == 0);
        MOONGTANGE_CHECK(!settings.Active().vSync);
        MOONGTANGE_CHECK(settings.Active().frameRateLimit == 60);
        MOONGTANGE_CHECK(DisplaySettings::RESOLUTIONS.back() == (Resolution{3840, 2160}));

        settings.SelectPreviousResolution();
        MOONGTANGE_CHECK(settings.Pending().resolution == PREVIOUS_RESOLUTION);
        settings.SelectPreviousWindowMode();
        MOONGTANGE_CHECK(settings.Pending().windowMode == WindowMode::FULLSCREEN);
        settings.SelectPreviousMonitor(3);
        MOONGTANGE_CHECK(settings.Pending().monitor == 2);
        settings.ToggleVSync();
        MOONGTANGE_CHECK(settings.Pending().vSync);
        settings.SelectPreviousFrameRateLimit();
        MOONGTANGE_CHECK(settings.Pending().frameRateLimit == 30);
        settings.Cancel();
        MOONGTANGE_CHECK(settings.Pending() == settings.Active());

        settings.SelectNextResolution();
        MOONGTANGE_CHECK(settings.Pending().resolution == NEXT_RESOLUTION);
        MOONGTANGE_CHECK(settings.Active().resolution == DEFAULT_RESOLUTION);
        settings.SelectNextWindowMode();
        MOONGTANGE_CHECK(settings.Pending().windowMode == WindowMode::BORDERLESS);
        settings.SelectNextMonitor(3);
        settings.ToggleVSync();
        settings.SelectNextFrameRateLimit();
        settings.Apply();
        MOONGTANGE_CHECK(settings.Active().resolution == NEXT_RESOLUTION);
        MOONGTANGE_CHECK(settings.Active().windowMode == WindowMode::BORDERLESS);
        MOONGTANGE_CHECK(settings.Active().monitor == 1);
        MOONGTANGE_CHECK(settings.Active().vSync);
        MOONGTANGE_CHECK(settings.Active().frameRateLimit == 120);
        MOONGTANGE_CHECK(settings.Confirmed().resolution == DEFAULT_RESOLUTION);

        settings.Revert();
        MOONGTANGE_CHECK(settings.Active() == DisplaySettingsState{});
        MOONGTANGE_CHECK(settings.Pending() == settings.Active());

        settings.SelectNextResolution();
        settings.SelectNextWindowMode();
        settings.SelectNextMonitor(3);
        settings.ToggleVSync();
        settings.SelectNextFrameRateLimit();
        settings.Apply();
        settings.Confirm();
        MOONGTANGE_CHECK(settings.Confirmed() == settings.Active());
        MOONGTANGE_CHECK(settings.Confirmed().monitor == 1);

        settings.ClampMonitorCount(1);
        MOONGTANGE_CHECK(settings.Active().monitor == 0);
        MOONGTANGE_CHECK(settings.Pending().monitor == 0);
        MOONGTANGE_CHECK(settings.Confirmed().monitor == 0);

        const auto uniqueSuffix = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto settingsDirectory =
            std::filesystem::temp_directory_path() /
            ("MoongTangE-RenderingTests-" + std::to_string(uniqueSuffix));
        const auto settingsPath = settingsDirectory / "display.cfg";
        const DisplaySettingsState savedSettings{
            {3840, 2160}, WindowMode::FULLSCREEN, 2, true, 144};
        const DisplaySettingsStore settingsStore(settingsPath);
        MOONGTANGE_CHECK(settingsStore.Save(savedSettings));
        MOONGTANGE_CHECK(settingsStore.Load() == savedSettings);

        {
            std::ofstream invalidSettings(settingsPath, std::ios::trunc);
            invalidSettings << "version=1\nwidth=999\n";
        }
        MOONGTANGE_CHECK(!settingsStore.Load());
        MOONGTANGE_CHECK(std::filesystem::remove(settingsPath));
        MOONGTANGE_CHECK(std::filesystem::remove(settingsDirectory));

        SnapshotInterpolator view;
        WorldSnapshot a{3, {{1, {0, 0.5, 0}, {}, {}, {}, "Alice"}}, {}}, b = a;
        b.tick = 6;
        b.players[0].position.x = 1;
        b.players[0].orientation = Quaternion::FromRotation({0, 0, -2});
        a.collectibles = {{1, CollectibleKind::APPLE, {0, 1, 0}, {}, 1}};
        b.collectibles = a.collectibles;
        b.collectibles[0].position.x = 2;

        view.Push(a);
        view.Push(b);
        view.Advance(0.025);
        auto middle = view.Sample();
        MOONGTANGE_CHECK(MoongTangE::test::approx(middle.players[0].position.x, 0.5));
        const auto expected = Quaternion::FromRotation({0, 0, -1});
        MOONGTANGE_CHECK(
            (middle.players[0].orientation.Rotate({1, 0, 0}) - expected.Rotate({1, 0, 0}))
                .Length() < 1e-9);
        MOONGTANGE_CHECK(a.players[0].position.x == 0 && b.players[0].position.x == 1);
        MOONGTANGE_CHECK(middle.players[0].nickname == "Alice");
        MOONGTANGE_CHECK(MoongTangE::test::approx(middle.collectibles[0].position.x, 1));

        SnapshotInterpolator attachedView;
        const Vector3 attachmentOffset{0.75, 0, 0};
        const Quaternion collectibleOrientation = Quaternion::FromRotation({0.2, 0, 0});
        WorldSnapshot attachedFrom{
            3,
            {{1, {0, 0.5, 0}, {}, {}, {}, "Alice"}},
            {{1, CollectibleKind::APPLE, {0.75, 0.5, 0}, collectibleOrientation, 1, 1}}};
        WorldSnapshot attachedTo = attachedFrom;
        attachedTo.tick = 6;
        attachedTo.players[0].position = {2, 0.5, 0};
        attachedTo.players[0].orientation = Quaternion::FromRotation({0, std::numbers::pi / 2, 0});
        attachedTo.collectibles[0].position =
            attachedTo.players[0].position +
            attachedTo.players[0].orientation.Rotate(attachmentOffset);
        attachedTo.collectibles[0].orientation =
            (attachedTo.players[0].orientation * collectibleOrientation).Normalized();

        attachedView.Push(attachedFrom);
        attachedView.Push(attachedTo);
        attachedView.Advance(0.025);
        const auto attachedMiddle = attachedView.Sample();
        const auto& attachedPlayer = attachedMiddle.players[0];
        const auto& attachedCollectible = attachedMiddle.collectibles[0];
        const Quaternion inverseAttachedPlayer = attachedPlayer.orientation.Conjugate();
        MOONGTANGE_CHECK(
            (inverseAttachedPlayer.Rotate(attachedCollectible.position - attachedPlayer.position) -
             attachmentOffset)
                .Length() < 1e-9);
        MOONGTANGE_CHECK(((inverseAttachedPlayer * attachedCollectible.orientation)
                              .Normalized()
                              .Rotate({0, 1, 0}) -
                          collectibleOrientation.Rotate({0, 1, 0}))
                             .Length() < 1e-9);

        view.Push(a); // Stale packet cannot rewind the view.
        MOONGTANGE_CHECK(view.Sample().players[0].position == middle.players[0].position);

        WorldSnapshot c = b;
        c.tick = 9;
        c.players[0].position.x = 2;
        view.Push(c);
        MOONGTANGE_CHECK(view.Sample().players[0].position == middle.players[0].position);
        view.Advance(10);
        MOONGTANGE_CHECK(view.Sample().players[0].position.x == 2);
        view.Advance(10);
        MOONGTANGE_CHECK(view.Sample().players[0].position.x == 2); // No extrapolation.

        c.tick = 12;
        c.players = {{2, {5, 0.5, 0}, {}, {}, {}, "Bob"}};
        view.Push(c);
        MOONGTANGE_CHECK(view.Sample().players.size() == 1 && view.Sample().players[0].id == 2);
        MOONGTANGE_CHECK(view.Sample().players[0].position.x == 5);

        view.Reset();
        MOONGTANGE_CHECK(view.Sample().players.empty());
        view.Push(a); // A newly admitted room may have a smaller tick.
        MOONGTANGE_CHECK(view.Sample().tick == a.tick);
        MOONGTANGE_CHECK(view.Sample().players[0].position.x == 0);
    });
}
