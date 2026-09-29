#pragma once

#include <stdbool.h>
#include <stdint.h>

#define SB_LOW_SPEED_KMH 5.0f
#define SB_HIGH_SPEED_KMH 90.0f
#define SB_SLOW_RATE_S 1800u
#define SB_FAST_RATE_S 90u
#define SB_TURN_MIN_DEG 28.0f
#define SB_TURN_SLOPE 410.0f
#define SB_TURN_TIME_S 30u

typedef struct
{
    bool has_last;
    uint32_t last_s;
    float last_course;
} SmartBeacon;

void sb_reset(SmartBeacon *sb);
uint32_t sb_rate(float speed_kmh);
bool sb_due(const SmartBeacon *sb, uint32_t now_s, float speed_kmh, float course);
void sb_sent(SmartBeacon *sb, uint32_t now_s, float course);
