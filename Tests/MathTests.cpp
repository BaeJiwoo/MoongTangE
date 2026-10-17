#include "Common/Math/Vector3.h"
#include "TestSupport.h"
#include <limits>
int main() { return test::run([] {
    using sandbox::Vector3;
    CHECK((Vector3{1,2,3}+Vector3{2,3,4}) == Vector3{3,5,7});
    CHECK((Vector3{1,2,3}-Vector3{2,3,4}) == Vector3{-1,-1,-1});
    CHECK(2.0*Vector3{1,2,3} == Vector3{2,4,6});
    CHECK(test::near(Vector3{3,4,0}.length(), 5));
    CHECK(test::near(Vector3{3,4,0}.normalized().length(), 1));
    CHECK(Vector3{}.normalized() == Vector3{});
    CHECK(test::near(Vector3{1e308,1e308,0}.normalized().length(), 1));
    CHECK(test::near(Vector3{1e-300,0,0}.normalized().x, 1));
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    CHECK(!Vector3{nan,0,0}.finite());
    CHECK(test::throws([&] { (void)Vector3{nan,0,0}.normalized(); }));
}); }
