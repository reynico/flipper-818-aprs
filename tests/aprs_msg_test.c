#define UNITY_INCLUDE_PRINT_FORMATTED

#include "unity/unity.h"
#include "../src/aprs.h"
#include "../src/aprs_msg.h"

#include <string.h>

#ifndef RUN_TEST
#define RUN_TEST(func) UnityDefaultTestRun(func, #func, __LINE__)
#endif

void setUp(void) {}
void tearDown(void) {}

static bool parse(const char *s, AprsMsg *m)
{
    return aprs_msg_parse(s, (uint16_t)strlen(s), m);
}

static void test_parse_plain(void)
{
    AprsMsg m;

    TEST_ASSERT_TRUE(parse("LU3ARN-14:hello", &m));
    TEST_ASSERT_EQUAL_STRING("LU3ARN-14", m.to);
    TEST_ASSERT_EQUAL_STRING("hello", m.text);
    TEST_ASSERT_EQUAL_STRING("", m.no);
    TEST_ASSERT_FALSE(m.is_ack);
}

static void test_parse_msgno(void)
{
    AprsMsg m;

    TEST_ASSERT_TRUE(parse("LU3ARN   :hello there{42", &m));
    TEST_ASSERT_EQUAL_STRING("LU3ARN", m.to);
    TEST_ASSERT_EQUAL_STRING("hello there", m.text);
    TEST_ASSERT_EQUAL_STRING("42", m.no);

    TEST_ASSERT_TRUE(parse("LU3ARN   :reply ack{AB}CD", &m));
    TEST_ASSERT_EQUAL_STRING("reply ack", m.text);
    TEST_ASSERT_EQUAL_STRING("AB", m.no);

    TEST_ASSERT_TRUE(parse("LU3ARN   :bad{12345678", &m));
    TEST_ASSERT_EQUAL_STRING("bad", m.text);
    TEST_ASSERT_EQUAL_STRING("", m.no);

    TEST_ASSERT_TRUE(parse("LU3ARN   :bad{1 2", &m));
    TEST_ASSERT_EQUAL_STRING("", m.no);

    TEST_ASSERT_TRUE(parse("LU3ARN   :empty{", &m));
    TEST_ASSERT_EQUAL_STRING("", m.no);
}

static void test_parse_ack_rej(void)
{
    AprsMsg m;

    TEST_ASSERT_TRUE(parse("LU3ARN-14:ack42", &m));
    TEST_ASSERT_TRUE(m.is_ack);
    TEST_ASSERT_FALSE(m.is_rej);
    TEST_ASSERT_EQUAL_STRING("42", m.no);

    TEST_ASSERT_TRUE(parse("LU3ARN-14:rej7", &m));
    TEST_ASSERT_TRUE(m.is_rej);
    TEST_ASSERT_EQUAL_STRING("7", m.no);

    TEST_ASSERT_TRUE(parse("LU3ARN-14:acknowledge this", &m));
    TEST_ASSERT_FALSE(m.is_ack);
    TEST_ASSERT_EQUAL_STRING("acknowledge this", m.text);
}

static void test_parse_short(void)
{
    AprsMsg m;

    TEST_ASSERT_FALSE(parse("LU3ARN", &m));
    TEST_ASSERT_FALSE(parse("LU3ARN-14xxxxx", &m));
}

static void test_encode(void)
{
    char out[96];

    TEST_ASSERT_TRUE(aprs_message_no(out, sizeof(out), "LU3ARN", 14, "hi", "123") > 0);
    TEST_ASSERT_EQUAL_STRING(":LU3ARN-14:hi{123", out);
    TEST_ASSERT_EQUAL_INT(0, aprs_message_no(out, sizeof(out), "LU3ARN", 14, "hi", "1{2"));
    TEST_ASSERT_EQUAL_INT(0, aprs_message_no(out, sizeof(out), "LU3ARN", 14, "hi", "123456"));

    TEST_ASSERT_TRUE(aprs_ack(out, sizeof(out), "LU3ARN-14", "123") > 0);
    TEST_ASSERT_EQUAL_STRING(":LU3ARN-14:ack123", out);
    TEST_ASSERT_TRUE(aprs_ack(out, sizeof(out), "LU3ARN", "A1") > 0);
    TEST_ASSERT_EQUAL_STRING(":LU3ARN   :ackA1", out);

    TEST_ASSERT_EQUAL_INT(0, aprs_ack(out, sizeof(out), "LU3ARN-16", "1"));
    TEST_ASSERT_EQUAL_INT(0, aprs_ack(out, sizeof(out), "LU3ARN-", "1"));
    TEST_ASSERT_EQUAL_INT(0, aprs_ack(out, sizeof(out), "LU3:RN", "1"));
    TEST_ASSERT_EQUAL_INT(0, aprs_ack(out, sizeof(out), "TOOLONGX", "1"));
    TEST_ASSERT_EQUAL_INT(0, aprs_ack(out, sizeof(out), "LU3ARN", ""));
}

static void test_ack_rate_limit(void)
{
    AckCache c;

    ack_cache_reset(&c);
    TEST_ASSERT_TRUE(ack_allow(&c, "LU1AAA", "1", 100));
    TEST_ASSERT_FALSE(ack_allow(&c, "LU1BBB", "1", 104));
    TEST_ASSERT_TRUE(ack_allow(&c, "LU1BBB", "1", 105));
    TEST_ASSERT_FALSE(ack_allow(&c, "LU1AAA", "1", 129));
    TEST_ASSERT_TRUE(ack_allow(&c, "LU1AAA", "1", 130));
    TEST_ASSERT_TRUE(ack_allow(&c, "LU1AAA", "2", 135));
}

int main(void)
{
    UnityBegin(__FILE__);
    RUN_TEST(test_parse_plain);
    RUN_TEST(test_parse_msgno);
    RUN_TEST(test_parse_ack_rej);
    RUN_TEST(test_parse_short);
    RUN_TEST(test_encode);
    RUN_TEST(test_ack_rate_limit);
    return UnityEnd();
}
