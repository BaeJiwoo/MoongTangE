#include "Common/Utility/CommandLine.h"
#include "TestSupport.h"

int main() {
    return test::run([] {
        CHECK(sandbox::unsignedOption("27015", 65535) == 27015);
        CHECK(sandbox::unsignedOption("0", 65535, true) == 0);
        for (auto bad : {"", "-1", "65536", "10x", "0", "1.5"}) {
            CHECK(test::throws([&] {
                sandbox::unsignedOption(bad, 65535);
            }));
        }
        CHECK(sandbox::endpointOption("localhost:27015").first == "localhost");
        CHECK(sandbox::endpointOption("[::1]:1234").first == "::1");
        for (auto bad : {"localhost", ":1234", "host:0", "[::1]:x", "::1:1234"}) {
            CHECK(test::throws([&] {
                sandbox::endpointOption(bad);
            }));
        }
    });
}
