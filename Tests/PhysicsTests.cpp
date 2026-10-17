#include "Common/Physics/PhysicsWorld.h"
#include "TestSupport.h"
int main() { return test::run([] {
    sandbox::PhysicsWorld physics;
    sandbox::RigidBody body;
    physics.step(body);
    CHECK(test::approx(body.velocity.y, -9.81/60));
    CHECK(test::approx(body.position.y, 2-9.81/3600));
    for (int i=0; i<3600; ++i) {
        physics.step(body);
        CHECK(body.position.y >= body.radius);
    }
    CHECK(body.position.y == 0.5);
    CHECK(body.velocity.y == 0);
    body.position.y = -10;
    body.velocity.y = -1000;
    physics.step(body);
    CHECK(body.position.y == 0.5 && body.velocity.y == 0);
    sandbox::RigidBody a, b;
    a.velocity.x = b.velocity.x = 3;
    for (int i=0; i<600; ++i) { physics.step(a); physics.step(b); }
    CHECK(a.position == b.position && a.velocity == b.velocity);
    CHECK(test::approx(a.position.x, 30));
    CHECK(test::throws([&] { physics.step(body, 0); }));
    CHECK(test::throws([&] { physics.step(body, -1); }));
}); }
