#include <cstdio>
#include <cstring>

#include "check.hpp"

int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : "";
    int ran = 0;
    for (const testing::Case& c : testing::registry()) {
        if (std::strncmp(c.name, filter, std::strlen(filter)) != 0) {
            continue;
        }
        const int before = testing::failures();
        c.run();
        ++ran;
        std::printf("[%s] %s\n", testing::failures() == before ? " ok " : "FAIL", c.name);
    }
    if (ran == 0) {
        std::fprintf(stderr, "no tests match '%s'\n", filter);
        return 2;
    }
    std::printf("%d tests, %d failed checks\n", ran, testing::failures());
    return testing::failures() == 0 ? 0 : 1;
}
