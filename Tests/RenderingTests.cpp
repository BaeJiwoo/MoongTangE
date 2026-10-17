#include "Client/Rendering/SnapshotInterpolator.h"
#include "TestSupport.h"
int main() { return test::run([] {
    using namespace sandbox;
    SnapshotInterpolator view;
    WorldSnapshot a{3,{{1,{0,0.5,0},{}}}}, b=a;
    b.tick=6; b.players[0].position.x=1;
    b.players[0].orientation=Quaternion::fromRotation({0,0,-2});
    view.push(a); view.push(b); view.advance(0.025);
    auto middle=view.sample();
    CHECK(test::approx(middle.players[0].position.x,0.5));
    const auto expected=Quaternion::fromRotation({0,0,-1});
    CHECK((middle.players[0].orientation.rotate({1,0,0})-expected.rotate({1,0,0})).length()<1e-9);
    CHECK(a.players[0].position.x==0 && b.players[0].position.x==1);
    view.push(a); // Stale packet cannot rewind the view.
    CHECK(view.sample().players[0].position==middle.players[0].position);
    WorldSnapshot c=b; c.tick=9; c.players[0].position.x=2;
    view.push(c); CHECK(view.sample().players[0].position==middle.players[0].position);
    view.advance(10); CHECK(view.sample().players[0].position.x==2);
    view.advance(10); CHECK(view.sample().players[0].position.x==2); // No extrapolation.
    c.tick=12; c.players={{2,{5,0.5,0},{}}}; view.push(c);
    CHECK(view.sample().players.size()==1 && view.sample().players[0].id==2);
    CHECK(view.sample().players[0].position.x==5);
}); }
