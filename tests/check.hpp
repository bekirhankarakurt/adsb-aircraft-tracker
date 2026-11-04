#pragma once

#include <cmath>
#include <cstdio>
#include <vector>

namespace testing {

struct Case {
    const char* name;
    void (*run)();
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

inline int& failures() {
    static int count = 0;
    return count;
}

struct Registrar {
    Registrar(const char* name, void (*run)()) { registry().push_back({name, run}); }
};

inline void report(bool ok, const char* expr, const char* file, int line) {
    if (!ok) {
        ++failures();
        std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", file, line, expr);
    }
}

}  // namespace testing

#define ADSB_TEST(suite, name)                                                        \
    static void suite##_##name();                                                     \
    static const testing::Registrar registrar_##suite##_##name(#suite "." #name,      \
                                                               &suite##_##name);      \
    static void suite##_##name()

#define CHECK(expr) testing::report(static_cast<bool>(expr), #expr, __FILE__, __LINE__)
#define CHECK_NEAR(a, b, tol) \
    testing::report(std::fabs((a) - (b)) <= (tol), #a " ~= " #b, __FILE__, __LINE__)
