#pragma once
#include "Common/Protocol/WorldSnapshot.h"
#include <raylib.h>
#include <rlgl.h>
#include <array>
#include <numbers>
namespace sandbox {
inline ::Vector3 renderVector(Vector3 value) { return {static_cast<float>(value.x),static_cast<float>(value.y),static_cast<float>(value.z)}; }
struct BallTriangle { std::array<Vector3,3> vertices; bool light; };
inline const std::vector<BallTriangle>& ballGeometry() {
    static const auto triangles=[] {
        std::vector<BallTriangle> result;
        constexpr int rings=20, sectors=32;
        auto point=[](int ring,int sector) {
            const double theta=std::numbers::pi*ring/rings, phi=2*std::numbers::pi*sector/sectors;
            return Vector3{std::sin(theta)*std::cos(phi),std::cos(theta),std::sin(theta)*std::sin(phi)};
        };
        for (int r=0;r<rings;++r) for (int s=0;s<sectors;++s) {
            const auto a=point(r,s), b=point(r+1,s), c=point(r,s+1), d=point(r+1,s+1);
            // Alternating panels, a white equator and a dark polar cap reveal every roll direction.
            const bool light=r==9 || r==10 || (r>1 && r<18 && (s/4+r/5)%2==0);
            result.push_back({{a,c,b},light}); result.push_back({{b,c,d},light});
        }
        return result;
    }();
    return triangles;
}
inline void drawRollingBall(const PlayerState& p, bool own) {
    DrawCylinder({static_cast<float>(p.position.x),0.008f,static_cast<float>(p.position.z)},0.48f,0.48f,0.002f,32,Color{28,37,46,255});
    const auto lightDirection=Vector3{-0.4,0.8,0.5}.normalized();
    rlBegin(RL_TRIANGLES);
    for (const auto& triangle : ballGeometry()) {
        const Color color=triangle.light ? Color{245,241,218,255} : (own ? Color{245,128,25,255} : Color{32,125,225,255});
        for (const auto vertex : triangle.vertices) {
            const auto normal=p.orientation.rotate(vertex);
            const double shade=0.4+0.6*std::max(0.0,dot(normal,lightDirection));
            const auto world=renderVector(p.position+normal*0.5);
            rlColor4ub(static_cast<unsigned char>(color.r*shade),static_cast<unsigned char>(color.g*shade),static_cast<unsigned char>(color.b*shade),255);
            rlVertex3f(world.x,world.y,world.z);
        }
    }
    rlEnd();
}
inline void renderScene(const WorldSnapshot& state, std::uint32_t playerId, const char* mode, const char* status) {
    ::Vector3 target{0,0,0};
    for (const auto& p : state.players) if (p.id == playerId) target = renderVector(p.position);
    Camera3D camera{{target.x+4, target.y+3.5f, target.z+5.5f},target,{0,1,0},45,CAMERA_PERSPECTIVE};
    BeginDrawing();
    ClearBackground(Color{22,28,42,255});
    BeginMode3D(camera);
    DrawPlane({target.x,0,target.z},{100,100},Color{50,65,80,255});
    DrawGrid(40,1);
    for (const auto& p : state.players) drawRollingBall(p,p.id==playerId);
    EndMode3D();
    DrawRectangle(14,14,510,158,Color{12,18,30,220});
    DrawText("PORTABLE PHYSICS SANDBOX",28,28,22,RAYWHITE);
    DrawText(TextFormat("%s | Player %u | Tick %llu",mode,playerId,static_cast<unsigned long long>(state.tick)),28,62,18,SKYBLUE);
    DrawText("WASD push  |  Release to coast  |  Opposite key to brake",28,90,16,LIGHTGRAY);
    DrawText("ESC quit  |  Server physics 60 Hz",28,114,16,LIGHTGRAY);
    DrawText(status,28,140,16,LIGHTGRAY);
    DrawFPS(GetScreenWidth()-100,20);
    EndDrawing();
}
}
