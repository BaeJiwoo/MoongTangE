#pragma once
#include "Common/Protocol/WorldSnapshot.h"
#include <raylib.h>
namespace sandbox {
inline ::Vector3 renderVector(Vector3 value) { return {static_cast<float>(value.x),static_cast<float>(value.y),static_cast<float>(value.z)}; }
inline void renderScene(const WorldSnapshot& state, std::uint32_t playerId, const char* mode, const char* status) {
    ::Vector3 target{0,0,0};
    for (const auto& p : state.players) if (p.id == playerId) target = renderVector(p.position);
    Camera3D camera{{target.x+9, target.y+9, target.z+12},target,{0,1,0},45,CAMERA_PERSPECTIVE};
    BeginDrawing();
    ClearBackground(Color{22,28,42,255});
    BeginMode3D(camera);
    DrawPlane({target.x,0,target.z},{100,100},Color{50,65,80,255});
    DrawGrid(40,1);
    for (const auto& p : state.players) {
        DrawSphere(renderVector(p.position),0.5f,p.id==playerId ? Color{255,182,72,255} : SKYBLUE);
        DrawSphereWires(renderVector(p.position),0.51f,8,12,p.id==playerId ? ORANGE : BLUE);
    }
    EndMode3D();
    DrawRectangle(14,14,440,132,Color{12,18,30,220});
    DrawText("PORTABLE PHYSICS SANDBOX",28,28,22,RAYWHITE);
    DrawText(TextFormat("%s | Player %u | Tick %llu",mode,playerId,static_cast<unsigned long long>(state.tick)),28,62,18,SKYBLUE);
    DrawText("WASD move  |  ESC quit  |  Server physics 60 Hz",28,90,16,LIGHTGRAY);
    DrawText(status,28,116,16,LIGHTGRAY);
    DrawFPS(GetScreenWidth()-100,20);
    EndDrawing();
}
}
