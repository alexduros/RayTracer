// Minimal test harness: TEST_CASE / CHECK / REQUIRE and nothing else.
// Cases self-register; TestMain.cpp runs them and returns non-zero on failure.
#pragma once

#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace test {

struct Case {
    const char* name;
    void (*fn)();
};
std::vector<Case>& cases();

struct Registrar {
    Registrar(const char* name, void (*fn)()) { cases().push_back({name, fn}); }
};

/// Thrown by REQUIRE to abort the current case.
struct Abort {};

void check(bool ok, const char* expr, const char* file, int line, const std::string& detail);

/// True when the binary was started with --update-golden.
bool updateGolden();
/// Scratch directory for images written by the tests (created on first use).
const std::string& outputDir();
const std::string& goldenDir();
/// "teapot" -> <models dir>/teapot.off
std::string modelPath(const char* name);

template <class A, class B>
std::string show(const char* aExpr, const A& a, const char* bExpr, const B& b) {
    std::ostringstream s;
    s << aExpr << " = " << a << ", " << bExpr << " = " << b;
    return s.str();
}

}  // namespace test

#define TEST_CONCAT2(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT2(a, b)
#define TEST_CASE(name)                                                                       \
    static void TEST_CONCAT(test_body_, __LINE__)();                                          \
    static test::Registrar TEST_CONCAT(test_reg_, __LINE__)(name, &TEST_CONCAT(test_body_, __LINE__)); \
    static void TEST_CONCAT(test_body_, __LINE__)()

#define CHECK(expr) test::check(static_cast<bool>(expr), #expr, __FILE__, __LINE__, "")
#define CHECK_MSG(expr, msg) test::check(static_cast<bool>(expr), #expr, __FILE__, __LINE__, (msg))
// CHECK_EQ / CHECK_CLOSE capture each operand once: they appear in both the
// comparison and the failure message, and an argument with a side effect
// (e.g. Vec3D::normalize()) must not run twice. The evaluation order of
// function arguments is unspecified, so a double evaluation can even pass on
// one compiler and fail on another.
#define CHECK_EQ(a, b)                                                                   \
    do {                                                                                 \
        auto _test_a = (a);                                                              \
        auto _test_b = (b);                                                              \
        test::check(_test_a == _test_b, #a " == " #b, __FILE__, __LINE__,                \
                    test::show(#a, _test_a, #b, _test_b));                               \
    } while (0)
#define CHECK_CLOSE(a, b, eps)                                                           \
    do {                                                                                 \
        auto _test_a = (a);                                                              \
        auto _test_b = (b);                                                              \
        test::check(std::fabs(static_cast<double>(_test_a) - static_cast<double>(_test_b)) <= (eps), \
                    #a " ~= " #b, __FILE__, __LINE__, test::show(#a, _test_a, #b, _test_b)); \
    } while (0)
#define REQUIRE(expr)                                                              \
    do {                                                                           \
        if (!(expr)) {                                                             \
            test::check(false, #expr, __FILE__, __LINE__, "required, case aborted"); \
            throw test::Abort();                                                   \
        }                                                                          \
    } while (0)
