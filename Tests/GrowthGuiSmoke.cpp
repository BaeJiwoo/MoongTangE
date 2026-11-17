#include "Client/Rendering/GraphicsWindow.h"
#include "Client/Rendering/Scene.h"
#include "GrowthTestSupport.h"

#include <raylib.h>

int main(int argumentCount, char* arguments[])
{
    return MoongTangE::test::run([&] {
        using namespace MoongTangE;
        SetTraceLogLevel(LOG_WARNING);
        GraphicsWindow window(true);
        SceneRenderer renderer;
        GameWorld world;
        MOONGTANGE_CHECK(world.AddPlayer(1));
        test::grow_player(world, 1, 1.4);
        auto state = world.Snapshot();
        state.players[0].nickname = "Jiwoo";
        state.players[0].orientation = MoongTangE::Quaternion::FromRotation({0, 2.5, 0});
        auto draw = [&] {
            MOONGTANGE_CHECK(renderer.Draw(state, 1, "Embedded server", "Room 1", false, false, {},
                                           false, 0) == MenuAction::NONE);
        };
        for (int frame = 0; frame < 3; ++frame) {
            draw();
        }
        Image first = LoadImageFromScreen();
        for (int frame = 0; frame < 7; ++frame) {
            draw();
        }
        Image second = LoadImageFromScreen();
        int changed = 0;
        for (int y = first.height / 2 - 60; y < first.height / 2 + 60; ++y) {
            for (int x = first.width / 2 - 60; x < first.width / 2 + 60; ++x) {
                const auto a = GetImageColor(first, x, y), b = GetImageColor(second, x, y);
                if (a.r != b.r || a.g != b.g || a.b != b.b) {
                    ++changed;
                }
            }
        }
        UnloadImage(first);
        UnloadImage(second);
        MOONGTANGE_CHECK(changed > 30); // A stationary character's actual rendered mouth animates.
        if (argumentCount > 1) {
            window.SaveScreenshot(arguments[1]);
        }

        // Render a real server mine hit, including authoritative shrink and warning state.
        auto before = world.Snapshot();
        const auto target = std::ranges::min_element(before.mines, {}, [&](const auto& mine) {
                                return (mine.position - before.players[0].position).Length();
                            })->position;
        bool hit = false;
        for (int tick = 0; tick < 3000; ++tick) {
            before = world.Snapshot();
            MOONGTANGE_CHECK(world.Apply(test::steer_to(before, 1, target)));
            world.Step();
            state = world.Snapshot();
            if (state.players[0].mineHitTick > before.players[0].mineHitTick) {
                hit = true;
                break;
            }
        }
        MOONGTANGE_CHECK(hit && state.players[0].radius < before.players[0].radius);
        for (int frame = 0; frame < 3; ++frame) {
            draw();
        }
        if (argumentCount > 2) {
            window.SaveScreenshot(arguments[2]);
        }
    });
}
