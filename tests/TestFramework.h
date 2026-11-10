// Minimal STL-only test framework for WhatCable-Linux.
//
//   TEST_CASE(name) { CHECK(cond); CHECK_EQ(a, b); REQUIRE(cond); }
//
// CHECK/CHECK_EQ record a failure and continue; REQUIRE returns from the
// test case. Run the binary with test names to run a subset, --list to list.
#pragma once

#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace wct {

struct TestCase {
    const char *name;
    void (*fn)();
};

std::vector<TestCase> &registry();
void reportFailure(const char *file, int line, const std::string &message);

struct Registrar {
    Registrar(const char *name, void (*fn)()) { registry().push_back({name, fn}); }
};

template <typename T>
std::string toString(const T &value)
{
    if constexpr (std::is_same_v<T, bool>) {
        return value ? "true" : "false";
    } else if constexpr (std::is_convertible_v<const T &, std::string_view>) {
        return '"' + std::string(std::string_view(value)) + '"';
    } else if constexpr (std::is_enum_v<T>) {
        return std::to_string(static_cast<long long>(value));
    } else if constexpr (std::is_arithmetic_v<T>) {
        std::ostringstream out;
        out << +value;
        return out.str();
    } else {
        return "<value>";
    }
}

template <typename T>
std::string toString(const std::optional<T> &value)
{
    return value ? toString(*value) : "nullopt";
}

} // namespace wct

#define WCT_CONCAT2(a, b) a##b
#define WCT_CONCAT(a, b) WCT_CONCAT2(a, b)

#define TEST_CASE(name)                                                        \
    static void name();                                                        \
    static const wct::Registrar WCT_CONCAT(name, _registrar)(#name, name);     \
    static void name()

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond))                                                           \
            wct::reportFailure(__FILE__, __LINE__, "CHECK(" #cond ")");        \
    } while (0)

#define REQUIRE(cond)                                                          \
    do {                                                                       \
        if (!(cond)) {                                                         \
            wct::reportFailure(__FILE__, __LINE__, "REQUIRE(" #cond ")");      \
            return;                                                            \
        }                                                                      \
    } while (0)

#define CHECK_EQ(actual, expected)                                             \
    do {                                                                       \
        const auto &wct_a = (actual);                                          \
        const auto &wct_e = (expected);                                        \
        if (!(wct_a == wct_e))                                                 \
            wct::reportFailure(__FILE__, __LINE__,                             \
                               "CHECK_EQ(" #actual ", " #expected "): got " +  \
                                   wct::toString(wct_a) + ", expected " +     \
                                   wct::toString(wct_e));                     \
    } while (0)
