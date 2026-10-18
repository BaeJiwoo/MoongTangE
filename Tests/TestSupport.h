#pragma once

#include <chrono>
#include <cmath>
#include <exception>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <string>
#include <thread>

namespace MoongTangE::test {

inline void check(bool ok, const char* expression,
                  std::source_location location = std::source_location::current())
{
    if (!ok) {
        throw std::runtime_error(std::string(expression) + " at " + location.file_name() + ":" +
                                 std::to_string(location.line()));
    }
}

inline bool approx(double a, double b, double tolerance = 1e-9)
{
    return std::abs(a - b) <= tolerance;
}

template <class F> int run(F f)
{
    try {
        f();
        std::cout << "Passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

template <class F> bool throws(F f)
{
    try {
        f();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

template <class F>
bool wait_for(F predicate, std::chrono::milliseconds timeout = std::chrono::seconds(3))
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return predicate();
}

} // namespace MoongTangE::test

#define MOONGTANGE_CHECK(...) ::MoongTangE::test::check((__VA_ARGS__), #__VA_ARGS__)
