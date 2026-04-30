// Minimal host-side test framework for alarm clock components.
// Compile with: g++ -std=c++17 -Wall -Wextra -Werror -DUNIT_TEST -I.. tests/test_*.cpp -o run_tests
#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <cstdio>
#include <cstdlib>
#include <cstring>

static int tests_run = 0;
static int tests_failed = 0;

#define TEST(name)                                                            \
    static void test_##name();                                                \
    static void run_test_##name() {                                           \
        tests_run++;                                                          \
        printf("  TEST %s ... ", #name);                                      \
        test_##name();                                                        \
    }                                                                         \
    static void test_##name()

#define ASSERT_EQ(a, b)                                                       \
    do {                                                                       \
        const auto _a = (a);                                                   \
        const auto _b = (b);                                                   \
        if (_a != _b) {                                                        \
            printf("FAIL\n    %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
            tests_failed++;                                                    \
            return;                                                            \
        }                                                                      \
    } while (0)

#define ASSERT_TRUE(expr)                                                      \
    do {                                                                        \
        if (!(expr)) {                                                          \
            printf("FAIL\n    %s:%d: %s is false\n", __FILE__, __LINE__, #expr);\
            tests_failed++;                                                     \
            return;                                                             \
        }                                                                       \
    } while (0)

#define ASSERT_FALSE(expr)                                                     \
    do {                                                                        \
        if ((expr)) {                                                           \
            printf("FAIL\n    %s:%d: %s is true\n", __FILE__, __LINE__, #expr); \
            tests_failed++;                                                     \
            return;                                                             \
        }                                                                       \
    } while (0)

#define PASS() printf("ok\n")

// Call RUN(name) in main() for each TEST(name) defined.
#define RUN(name) run_test_##name()

#endif // TEST_FRAMEWORK_H
