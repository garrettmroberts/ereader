#include "refresh.h"

#include <unity.h>

#include <string>

namespace {

std::string run(int paints, int forceAt) {
    int since = kRefreshStart;
    std::string marks;
    for (int i = 0; i < paints; i++) {
        const RefreshPlan plan = chooseRefresh(since, i == forceAt);
        since = plan.paintsSinceFull;
        marks.push_back(plan.full ? 'F' : 'P');
    }
    return marks;
}

}  // namespace

extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

extern "C" void test_interval_is_one_full_then_two_partials(void) {
    TEST_ASSERT_EQUAL_INT(3, kFullEvery);
    TEST_ASSERT_EQUAL_STRING("FPPFPP", run(6, -1).c_str());
}

extern "C" void test_forced_full_restarts_the_run(void) {
    TEST_ASSERT_EQUAL_STRING("FFPPFP", run(6, 1).c_str());
}

extern "C" void test_forced_full_on_a_scheduled_full_still_resets(void) {
    int since = kRefreshStart;
    const RefreshPlan first = chooseRefresh(since, true);
    TEST_ASSERT_TRUE(first.full);
    TEST_ASSERT_EQUAL_INT(0, first.paintsSinceFull);

    since = kFullEvery - 1;
    const RefreshPlan scheduled = chooseRefresh(since, true);
    TEST_ASSERT_TRUE(scheduled.full);
    TEST_ASSERT_EQUAL_INT(0, scheduled.paintsSinceFull);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_interval_is_one_full_then_two_partials);
    RUN_TEST(test_forced_full_restarts_the_run);
    RUN_TEST(test_forced_full_on_a_scheduled_full_still_resets);
    return UNITY_END();
}
