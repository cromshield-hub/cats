#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <functional>

// Minimal test framework when Google Test is not available
namespace minitest {
    struct TestCase {
        std::string name;
        std::function<bool()> func;
    };

    static std::vector<TestCase>& tests() {
        static std::vector<TestCase> t;
        return t;
    }

    static int addTest(const char* name, std::function<bool()> func) {
        tests().push_back({name, std::move(func)});
        return 0;
    }
}

#define TEST_CASE(name) \
    static bool test_##name(); \
    static int reg_##name = minitest::addTest(#name, test_##name); \
    static bool test_##name()

#define ASSERT_TRUE(expr) do { if (!(expr)) { fprintf(stderr, "  FAIL: %s (line %d)\n", #expr, __LINE__); return false; } } while(0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b) ASSERT_TRUE((a) != (b))

// Include test files as compilation units reference these macros
// The actual test functions are defined in the unit test .cpp files
// using Google Test macros. For standalone mode, we provide a simple runner.

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    int passed = 0, failed = 0;
    for (const auto& test : minitest::tests()) {
        printf("[ RUN  ] %s\n", test.name.c_str());
        if (test.func()) {
            printf("[ PASS ] %s\n", test.name.c_str());
            ++passed;
        } else {
            printf("[ FAIL ] %s\n", test.name.c_str());
            ++failed;
        }
    }

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
