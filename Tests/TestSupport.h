#pragma once
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace test {
inline void check(bool ok, const char* expression, int line) {
    if (!ok) throw std::runtime_error(std::string(expression)+" at line "+std::to_string(line));
}
inline bool near(double a, double b, double tolerance=1e-9) { return std::abs(a-b) <= tolerance; }
template<class F> int run(F f) {
    try { f(); std::cout << "Passed\n"; return 0; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
template<class F> bool throws(F f) {
    try { f(); } catch (const std::exception&) { return true; }
    return false;
}
}
#define CHECK(...) test::check((__VA_ARGS__), #__VA_ARGS__, __LINE__)
