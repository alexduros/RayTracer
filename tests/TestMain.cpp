#include "Test.h"

#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>

namespace test {

std::vector<Case>& cases() {
    static std::vector<Case> c;
    return c;
}

namespace {
int g_checks = 0;
int g_failures = 0;
bool g_updateGolden = false;
std::string g_outputDir;
}  // namespace

void check(bool ok, const char* expr, const char* file, int line, const std::string& detail) {
    ++g_checks;
    if (ok) return;
    ++g_failures;
    std::cout << "    FAILED " << file << ":" << line << ": " << expr;
    if (!detail.empty()) std::cout << "  [" << detail << "]";
    std::cout << "\n";
}

bool updateGolden() { return g_updateGolden; }

const std::string& outputDir() {
    if (g_outputDir.empty()) {
        g_outputDir = RAYMINI_TEST_OUTPUT_DIR;
        std::filesystem::create_directories(g_outputDir);
    }
    return g_outputDir;
}

const std::string& goldenDir() {
    static const std::string dir = RAYMINI_GOLDEN_DIR;
    return dir;
}

std::string modelPath(const char* name) {
    return std::string(RAYMINI_MODELS_DIR) + "/" + name + ".off";
}

}  // namespace test

int main(int argc, char** argv) {
    std::string filter, exclude;
    bool list = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--filter" && i + 1 < argc) filter = argv[++i];
        else if (a == "--exclude" && i + 1 < argc) exclude = argv[++i];
        else if (a == "--update-golden") test::g_updateGolden = true;
        else if (a == "--list") list = true;
        else {
            std::cout << "usage: raymini_tests [--filter substr] [--exclude substr] [--update-golden] [--list]\n";
            return a == "-h" || a == "--help" ? 0 : 2;
        }
    }

    int run = 0;
    for (const test::Case& c : test::cases()) {
        const std::string name = c.name;
        if (!filter.empty() && name.find(filter) == std::string::npos) continue;
        if (!exclude.empty() && name.find(exclude) != std::string::npos) continue;
        if (list) {
            std::cout << name << "\n";
            continue;
        }
        ++run;
        const int failuresBefore = test::g_failures;
        std::cout << "[ RUN  ] " << name << "\n";
        try {
            c.fn();
        } catch (const test::Abort&) {
        } catch (const std::exception& e) {
            test::check(false, "no exception escapes the case", __FILE__, __LINE__, e.what());
        }
        std::cout << (test::g_failures == failuresBefore ? "[  OK  ] " : "[ FAIL ] ") << name << "\n";
    }
    if (list) return 0;

    std::cout << "\n" << run << " cases, " << test::g_checks << " checks, " << test::g_failures << " failures\n";
    if (test::g_updateGolden) std::cout << "golden images written to " << test::goldenDir() << "\n";
    return test::g_failures == 0 ? 0 : 1;
}
