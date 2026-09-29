#define UNITY_INCLUDE_PRINT_FORMATTED

#include "unity/unity.h"
#include "../src/kiss.h"
#include "../src/aprs.h"

#include <string.h>

#ifndef RUN_TEST
#define RUN_TEST(func) UnityDefaultTestRun(func, #func, __LINE__)
#endif

void setUp(void) {}
void tearDown(void) {}

static void test_encode_escape(void)
{
    const uint8_t in[] = {0x01, KISS_FEND, 0x02, KISS_FESC, 0x03};
    const uint8_t want[] = {KISS_FEND, 0x00, 0x01, KISS_FESC, KISS_TFEND, 0x02,
                            KISS_FESC, KISS_TFESC, 0x03, KISS_FEND};
    uint8_t out[32];

    TEST_ASSERT_EQUAL_UINT16(sizeof(want), kiss_encode(in, sizeof(in), out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(want, out, sizeof(want));
    TEST_ASSERT_EQUAL_UINT16(0, kiss_encode(in, sizeof(in), out, 6));
}

static void test_decode_roundtrip(void)
{
    const uint8_t in[] = {0x01, KISS_FEND, 0x02, KISS_FESC, 0x03};
    uint8_t enc[32];
    uint16_t n;
    uint16_t i;
    KissRx k;
    int done = 0;

    n = kiss_encode(in, sizeof(in), enc, sizeof(enc));
    kiss_rx_reset(&k);
    for (i = 0; i < n; i++)
        if (kiss_rx_byte(&k, enc[i]))
            done++;

    TEST_ASSERT_EQUAL_INT(1, done);
    TEST_ASSERT_EQUAL_UINT16(sizeof(in) + 1, k.len);
    TEST_ASSERT_EQUAL_HEX8(0x00, k.buf[0]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(in, k.buf + 1, sizeof(in));
}

static void test_decode_bad(void)
{
    KissRx k;
    uint16_t i;

    kiss_rx_reset(&k);
    TEST_ASSERT_FALSE(kiss_rx_byte(&k, KISS_FEND));
    TEST_ASSERT_FALSE(kiss_rx_byte(&k, KISS_FEND));

    kiss_rx_byte(&k, 0x00);
    kiss_rx_byte(&k, KISS_FESC);
    kiss_rx_byte(&k, 0x42);
    kiss_rx_byte(&k, 0x01);
    TEST_ASSERT_FALSE(kiss_rx_byte(&k, KISS_FEND));

    for (i = 0; i < KISS_FRAME_MAX + 10; i++)
        kiss_rx_byte(&k, 0x41);
    TEST_ASSERT_FALSE(kiss_rx_byte(&k, KISS_FEND));

    kiss_rx_byte(&k, 0x00);
    kiss_rx_byte(&k, 0x41);
    TEST_ASSERT_TRUE(kiss_rx_byte(&k, KISS_FEND));
}

static void test_ui_check(void)
{
    Packet p;
    char src[7];
    uint8_t ssid;

    TEST_ASSERT_TRUE(aprs_packet(&p, "LU3ARN", 14, "APFLIP", 0, ">hi", "WIDE1-1,WIDE2-1"));
    TEST_ASSERT_TRUE(ax25_ui_check(p.ax25, p.ax25_len, src, &ssid));
    TEST_ASSERT_EQUAL_STRING("LU3ARN", src);
    TEST_ASSERT_EQUAL_UINT8(14, ssid);

    TEST_ASSERT_TRUE(aprs_packet(&p, "LU3ARN", 0, "APFLIP", 0, ">hi", NULL));
    TEST_ASSERT_TRUE(ax25_ui_check(p.ax25, p.ax25_len, src, &ssid));

    TEST_ASSERT_FALSE(ax25_ui_check(p.ax25, 15, src, &ssid));

    p.ax25[14] = 0x13;
    TEST_ASSERT_FALSE(ax25_ui_check(p.ax25, p.ax25_len, src, &ssid));
    p.ax25[14] = 0x03;

    p.ax25[7] = ('a' << 1);
    TEST_ASSERT_FALSE(ax25_ui_check(p.ax25, p.ax25_len, src, &ssid));

    TEST_ASSERT_TRUE(aprs_packet(&p, "LU3ARN", 14, "APFLIP", 0, ">hi", "WIDE1-1"));
    p.ax25[20] |= 0x80;
    TEST_ASSERT_FALSE(ax25_ui_check(p.ax25, p.ax25_len, src, &ssid));

    TEST_ASSERT_TRUE(aprs_packet(&p, "LU3ARN", 14, "APFLIP", 0, ">hi", "WIDE1-1"));
    p.ax25[20] &= (uint8_t)~1;
    TEST_ASSERT_FALSE(ax25_ui_check(p.ax25, p.ax25_len, src, &ssid));
}

int main(void)
{
    UnityBegin(__FILE__);
    RUN_TEST(test_encode_escape);
    RUN_TEST(test_decode_roundtrip);
    RUN_TEST(test_decode_bad);
    RUN_TEST(test_ui_check);
    return UnityEnd();
}
