#include "Common/Physics/PhysicsWorld.h"
#include "TestSupport.h"
#include <numbers>
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
    using namespace sandbox;
    // Without resistance, one circumference of travel gives exactly one revolution.
    PhysicsWorld noResistance({0.8,0,0});
    for (const double radius : {0.25,0.5,1.0}) {
        RigidBody rolling; rolling.radius=radius; rolling.position={0,radius,0};
        rolling.velocity={std::numbers::pi*radius,0,0}; rolling.angularVelocity={0,0,-std::numbers::pi};
        for (int i=0;i<120;++i) {
            noResistance.step(rolling);
            const auto expected=Quaternion::fromRotation({0,0,-rolling.position.x/radius});
            CHECK((rolling.orientation.rotate({1,0,0})-expected.rotate({1,0,0})).length()<1e-9);
        }
        CHECK(test::approx(rolling.position.x,2*std::numbers::pi*radius));
        CHECK(test::approx(std::abs(rolling.orientation.w),1));
    }
    // Sliding friction transfers translation into rotation and dissipates energy.
    RigidBody sliding; sliding.position={0,0.5,0}; sliding.velocity={3,0,0};
    auto energy=[](const RigidBody& b) { return 0.5*b.mass*dot(b.velocity,b.velocity)+0.5*b.inertia()*dot(b.angularVelocity,b.angularVelocity); };
    double previous=energy(sliding);
    for (int i=0;i<120;++i) {
        noResistance.step(sliding);
        CHECK(energy(sliding)<=previous+1e-10); previous=energy(sliding);
    }
    CHECK(test::approx(sliding.velocity.x,15.0/7));
    CHECK(test::approx(sliding.angularVelocity.z,-sliding.velocity.x/sliding.radius));
    const double start=sliding.position.x;
    physics.step(sliding);
    CHECK(sliding.velocity.x>0 && sliding.position.x>start);
    for (int i=0;i<600;++i) {
        const double before=energy(sliding); physics.step(sliding);
        CHECK(energy(sliding)<=before+1e-10);
    }
    CHECK(sliding.velocity==Vector3{} && sliding.angularVelocity==Vector3{});

    RigidBody driven; driven.position={0,0.5,0};
    for (int i=0;i<120;++i) physics.step(driven,PhysicsWorld::fixedDelta,{10,0,0});
    const double speed=driven.velocity.x;
    CHECK(speed>3 && speed<5.5);
    physics.step(driven,PhysicsWorld::fixedDelta,{-10,0,0});
    CHECK(driven.velocity.x>0 && driven.velocity.x<speed);
    for (int i=0;i<120;++i) physics.step(driven,PhysicsWorld::fixedDelta,{-10,0,0});
    CHECK(driven.velocity.x<0);

    RigidBody airborne; airborne.position.y=10000; airborne.velocity={2,0,3}; airborne.angularVelocity={1,2,3};
    for (int i=0;i<600;++i) physics.step(airborne,PhysicsWorld::fixedDelta,{10,0,0});
    CHECK(airborne.velocity.x==2 && airborne.velocity.z==3);
    CHECK(airborne.angularVelocity==Vector3{1,2,3} && airborne.orientation.unit());
    RigidBody a, b; a.position=b.position={0,0.5,0};
    for (int i=0;i<36000;++i) {
        physics.step(a,PhysicsWorld::fixedDelta,{6,0,8}); physics.step(b,PhysicsWorld::fixedDelta,{6,0,8});
        CHECK(a.orientation.unit());
    }
    CHECK(a.position==b.position && a.orientation==b.orientation && a.angularVelocity==b.angularVelocity);
    CHECK(test::throws([&] { physics.step(body, 0); }));
    CHECK(test::throws([&] { physics.step(body, -1); }));
    body.orientation={0,0,0,0}; CHECK(test::throws([&] { physics.step(body); }));
    CHECK(test::throws([] { PhysicsWorld invalid({-1,0,0}); }));
}); }
