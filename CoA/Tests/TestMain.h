#ifndef COA_TESTS_TESTMAIN_H
#define COA_TESTS_TESTMAIN_H

// Minimal test support for CoATests: each test is a function registered with COA_TEST; CHECK records a failure
// and continues. The exit code is the number of failed checks.

#include <cstdio>

namespace CoATest {
    using Fn = void (*)();
    void Register(const char* name, Fn fn);
    void Fail(const char* file, int line, const char* expr);
}

#define COA_TEST(name)                                              \
    static void name();                                             \
    static const int name##_registered = (CoATest::Register(#name, name), 0); \
    static void name()

#define CHECK(expr)                                                 \
    do {                                                            \
        if (!(expr)) {                                              \
            CoATest::Fail(__FILE__, __LINE__, #expr);               \
        }                                                           \
    } while (0)

#endif
