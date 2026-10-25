#include "CoA/Tests/TestMain.h"
#include <cstring>
#include <vector>

namespace {
    struct Entry {
        const char* name;
        CoATest::Fn fn;
    };

    std::vector<Entry>& Tests() {
        static std::vector<Entry> tests;
        return tests;
    }

    int s_failures = 0;
}

void CoATest::Register(const char* name, Fn fn) {
    Tests().push_back({ name, fn });
}

void CoATest::Fail(const char* file, int line, const char* expr) {
    std::printf("  FAIL %s:%d: %s\n", file, line, expr);
    s_failures++;
}

// Usage: CoATests [name-substring]
int main(int argc, char** argv) {
    int ran = 0;

    for (const Entry& test : Tests()) {
        if (argc > 1 && !std::strstr(test.name, argv[1])) {
            continue;
        }

        int before = s_failures;
        test.fn();
        std::printf("%s %s\n", s_failures == before ? "ok  " : "FAIL", test.name);
        ran++;
    }

    std::printf("%d tests, %d failed checks\n", ran, s_failures);
    return s_failures;
}
