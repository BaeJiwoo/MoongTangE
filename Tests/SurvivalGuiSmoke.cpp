#include "Client/Rendering/GraphicsWindow.h"
#include "Client/Rendering/Scene.h"
#include "TestSupport.h"

#include <raylib.h>

int main(int argumentCount, char* arguments[])
{
    return MoongTangE::test::run([&] {
        using namespace MoongTangE;
        SetTraceLogLevel(LOG_WARNING);
        GraphicsWindow window(true);
        SceneRenderer renderer;
        WorldSnapshot state{
            100,
            {{1, {-2, 0.5, 0}, {}, {}, {}, "Winner"}, {2, {0, 0.5, 0}, {}, {}, {}, "Player", 100}},
            {{1, CollectibleKind::APPLE, {-2, 1.2, 0}, {}, 2, 1}}};
        auto draw = [&](bool pending = false) {
            return renderer.Draw(state, 2, "Embedded server", "Room 1", false, false, {}, false, 0,
                                 pending);
        };
        for (int frame = 0; frame < 3; ++frame) {
            MOONGTANGE_CHECK(draw() == MenuAction::NONE);
        }
        if (argumentCount > 1) {
            window.SaveScreenshot(arguments[1]);
        }

        // Event numbers belong to the pinned raylib AutomationEventType in rcore.c.
        constexpr unsigned KEY_UP_EVENT = 1;
        constexpr unsigned KEY_DOWN_EVENT = 2;
        PlayAutomationEvent({0, KEY_DOWN_EVENT, {KEY_ENTER, 0, 0, 0}});
        MOONGTANGE_CHECK(draw() == MenuAction::RESTART_GAME);
        PlayAutomationEvent({0, KEY_UP_EVENT, {KEY_ENTER, 0, 0, 0}});
        draw();
        PlayAutomationEvent({0, KEY_DOWN_EVENT, {KEY_R, 0, 0, 0}});
        MOONGTANGE_CHECK(draw(true) == MenuAction::NONE);
        PlayAutomationEvent({0, KEY_UP_EVENT, {KEY_R, 0, 0, 0}});
        draw();
        PlayAutomationEvent({0, KEY_DOWN_EVENT, {KEY_ESCAPE, 0, 0, 0}});
        MOONGTANGE_CHECK(draw() == MenuAction::MAIN_MENU);
        PlayAutomationEvent({0, KEY_UP_EVENT, {KEY_ESCAPE, 0, 0, 0}});
        draw();

        state.tick = 103;
        state.players[1].deathTick = 0;
        state.players[1].position = {-16.5, 2, -22};
        PlayAutomationEvent({0, KEY_DOWN_EVENT, {KEY_ENTER, 0, 0, 0}});
        MOONGTANGE_CHECK(draw() == MenuAction::NONE);
    });
}
