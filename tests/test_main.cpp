#include "TestFramework.h"
#include "Fixtures.h"

#include <cstdio>
#include <cstring>
#include <exception>
#include <string>

namespace wct {

namespace {
int g_failures = 0;
const char *g_current = "";
}

std::vector<TestCase> &registry()
{
    static std::vector<TestCase> tests;
    return tests;
}

void reportFailure(const char *file, int line, const std::string &message)
{
    ++g_failures;
    std::fprintf(stderr, "%s:%d: [%s] %s\n", file, line, g_current, message.c_str());
}

} // namespace wct

int main(int argc, char **argv)
{
    // --write-fixture DIR: write the dev-laptop sysfs tree for the CLI tests.
    if (argc == 3 && std::strcmp(argv[1], "--write-fixture") == 0) {
        try {
            wct::FakeSysfs fs{std::filesystem::path(argv[2])};
            wct::buildDevLaptop(fs);
        } catch (const std::exception &e) {
            std::fprintf(stderr, "--write-fixture: %s\n", e.what());
            return 1;
        }
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--list") == 0) {
        for (const auto &t : wct::registry())
            std::printf("%s\n", t.name);
        return 0;
    }

    int run = 0;
    int failedTests = 0;
    for (const auto &t : wct::registry()) {
        bool selected = argc < 2;
        for (int i = 1; i < argc && !selected; ++i)
            selected = std::strcmp(argv[i], t.name) == 0;
        if (!selected)
            continue;

        ++run;
        wct::g_current = t.name;
        const int before = wct::g_failures;
        try {
            t.fn();
        } catch (const std::exception &e) {
            wct::reportFailure(__FILE__, __LINE__, std::string("exception: ") + e.what());
        }
        if (wct::g_failures != before)
            ++failedTests;
    }

    if (run == 0) {
        std::fprintf(stderr, "no matching tests\n");
        return 2;
    }
    std::printf("%d test(s), %d failed, %d failed check(s)\n", run, failedTests,
                wct::g_failures);
    return failedTests ? 1 : 0;
}
