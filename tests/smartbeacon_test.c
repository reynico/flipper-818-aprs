#define UNITY_INCLUDE_PRINT_FORMATTED

#include "unity/unity.h"
#include "../src/smartbeacon.h"

#ifndef RUN_TEST
#define RUN_TEST(func) UnityDefaultTestRun(func, #func, __LINE__)
#endif

void setUp(void) {}
void tearDown(void) {}

static void test_rate(void)
{
    TEST_ASSERT_EQUAL_UINT32(SB_SLOW_RATE_S, sb_rate(0.0f));
    TEST_ASSERT_EQUAL_UINT32(SB_SLOW_RATE_S, sb_rate(SB_LOW_SPEED_KMH));
    TEST_ASSERT_EQUAL_UINT32(SB_FAST_RATE_S, sb_rate(SB_HIGH_SPEED_KMH));
    TEST_ASSERT_EQUAL_UINT32(SB_FAST_RATE_S, sb_rate(200.0f));
    TEST_ASSERT_EQUAL_UINT32(180, sb_rate(45.0f));
    TEST_ASSERT_EQUAL_UINT32(810, sb_rate(10.0f));
}

static void test_first_beacon_is_due(void)
{
    SmartBeacon sb;

    sb_reset(&sb);
    TEST_ASSERT_TRUE(sb_due(&sb, 0, 0.0f, 0.0f));
}

static void test_timer(void)
{
    SmartBeacon sb;

    sb_reset(&sb);
    sb_sent(&sb, 1000, 90.0f);
    TEST_ASSERT_FALSE(sb_due(&sb, 1000 + 179, 45.0f, 90.0f));
    TEST_ASSERT_TRUE(sb_due(&sb, 1000 + 180, 45.0f, 90.0f));
    TEST_ASSERT_FALSE(sb_due(&sb, 1000 + 1799, 0.0f, 90.0f));
    TEST_ASSERT_TRUE(sb_due(&sb, 1000 + 1800, 0.0f, 90.0f));
}

static void test_corner_pegging(void)
{
    SmartBeacon sb;

    sb_reset(&sb);
    sb_sent(&sb, 0, 350.0f);

    /* 60 km/h: threshold 28 + 410/60 = 34.8 deg */
    TEST_ASSERT_FALSE(sb_due(&sb, 29, 60.0f, 60.0f));
    TEST_ASSERT_TRUE(sb_due(&sb, 30, 60.0f, 30.0f));
    TEST_ASSERT_FALSE(sb_due(&sb, 30, 60.0f, 20.0f));
    TEST_ASSERT_TRUE(sb_due(&sb, 30, 60.0f, 300.0f));
}

static void test_no_turn_when_stopped(void)
{
    SmartBeacon sb;

    sb_reset(&sb);
    sb_sent(&sb, 0, 0.0f);
    TEST_ASSERT_FALSE(sb_due(&sb, 600, 3.0f, 180.0f));
}

int main(void)
{
    UnityBegin(__FILE__);
    RUN_TEST(test_rate);
    RUN_TEST(test_first_beacon_is_due);
    RUN_TEST(test_timer);
    RUN_TEST(test_corner_pegging);
    RUN_TEST(test_no_turn_when_stopped);
    return UnityEnd();
}
