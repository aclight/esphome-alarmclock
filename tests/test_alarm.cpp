// Host-side tests for alarm clock logic.
// Build: g++ -std=c++17 -Wall -Wextra -Werror -DUNIT_TEST -I.. test_alarm.cpp -o run_tests && ./run_tests

#include "test_framework.h"

// ---------------------------------------------------------------------------
// Smoke test — proves the test harness itself works.
// ---------------------------------------------------------------------------

TEST(smoke_test) {
    ASSERT_EQ(1, 1);
    ASSERT_TRUE(true);
    ASSERT_FALSE(false);
    PASS();
}

// ---------------------------------------------------------------------------
// main — register every TEST here.
// ---------------------------------------------------------------------------

int main() {
    printf("Running alarm-clock tests...\n");

    RUN(smoke_test);

    printf("\n%d test(s) run, %d failed.\n", tests_run, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
