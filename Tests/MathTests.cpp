#include "Common/Math/Vector3.h"
#include "Common/Math/Quaternion.h"
#include <numbers>
#include "TestSupport.h"
#include <limits>
int main() { return test::run([] {
    using sandbox::Vector3;
    CHECK((Vector3{1,2,3}+Vector3{2,3,4}) == Vector3{3,5,7});
    CHECK((Vector3{1,2,3}-Vector3{2,3,4}) == Vector3{-1,-1,-1});
    CHECK(2.0*Vector3{1,2,3} == Vector3{2,4,6});
    CHECK(test::approx(Vector3{3,4,0}.length(), 5));
    CHECK(test::approx(Vector3{3,4,0}.normalized().length(), 1));
    CHECK(Vector3{}.normalized() == Vector3{});
    CHECK(test::approx(Vector3{1e308,1e308,0}.normalized().length(), 1));
    CHECK(test::approx(Vector3{1e-300,0,0}.normalized().x, 1));
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    CHECK(!Vector3{nan,0,0}.finite());
    CHECK(test::throws([&] { (void)Vector3{nan,0,0}.normalized(); }));
    CHECK(sandbox::cross({1,0,0},{0,1,0})==Vector3{0,0,1});
    const auto q=sandbox::Quaternion::fromRotation({0,0,std::numbers::pi/2});
    CHECK((q.rotate({1,0,0})-Vector3{0,1,0}).length()<1e-9);
    CHECK(((q*q).rotate({1,0,0})-Vector3{-1,0,0}).length()<1e-9);
    const auto halfway=sandbox::interpolate({},q,0.5);
    CHECK(test::approx(halfway.rotate({1,0,0}).x,std::sqrt(0.5)));
    CHECK(sandbox::interpolate(q,{-q.w,-q.x,-q.y,-q.z},0.5).unit());
    CHECK(test::throws([] { (void)sandbox::Quaternion{0,0,0,0}.normalized(); }));
}); }
